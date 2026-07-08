/* Conversation loop pre-flight repair implementation
 *
 * Port of Hermes Agent run_agent.py / agent/conversation_loop.py helpers:
 *   - _sanitize_api_messages
 *   - _repair_message_sequence
 */
#include "conversation_loop.h"
#include "agent_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

/* Valid API roles */
static const char* VALID_ROLES[] = {
    "system", "user", "assistant", "tool", "function", NULL
};

static int is_valid_role(const char* role) {
    if (!role) return 0;
    for (int i = 0; VALID_ROLES[i]; i++) {
        if (strcmp(role, VALID_ROLES[i]) == 0) return 1;
    }
    return 0;
}

/* Check if a tool_call_id exists in a set of IDs */
static int is_known_id(const char* id, char** ids, size_t count) {
    if (!id || !ids) return 0;
    for (size_t i = 0; i < count; i++) {
        if (ids[i] && strcmp(ids[i], id) == 0) return 1;
    }
    return 0;
}

static void free_id_set(char** ids, size_t count) {
    if (!ids) return;
    for (size_t i = 0; i < count; i++) free(ids[i]);
    free(ids);
}

/* ------------------------------------------------------------------ */
/* sanitize_api_messages                                               */
/* ------------------------------------------------------------------ */

char* sanitize_api_messages(const char* messages_json) {
    if (!messages_json) return NULL;

    yyjson_doc* idoc = yyjson_read(messages_json, strlen(messages_json), 0);
    if (!idoc) return NULL;

    /*
     * Work directly on the mutable document. We modify the root array in
     * reverse order so indices stay valid during removal. The tool_call_ids
     * array is freed AFTER the write because yyjson references the strings
     * (yyjson_mut_str, not yyjson_mut_strcpy).
     */
    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return NULL;

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_arr(root)) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }

    /* Pass 1: collect all tool_call_ids from assistant messages */
    size_t max_ids = 256;
    char** tool_call_ids = (char**)calloc(max_ids, sizeof(char*));
    if (!tool_call_ids) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t id_count = 0;

    size_t idx, max;
    yyjson_mut_val* msg;

    yyjson_mut_arr_foreach(root, idx, max, msg) {
        if (!yyjson_mut_is_obj(msg)) continue;
        yyjson_mut_val* role = yyjson_mut_obj_get(msg, "role");
        const char* role_str = role ? yyjson_mut_get_str(role) : "";
        if (strcmp(role_str, "assistant") != 0) continue;

        yyjson_mut_val* tcs = yyjson_mut_obj_get(msg, "tool_calls");
        if (!tcs || !yyjson_mut_is_arr(tcs)) continue;

        size_t ti, tmax;
        yyjson_mut_val* tc;
        yyjson_mut_arr_foreach(tcs, ti, tmax, tc) {
            if (!yyjson_mut_is_obj(tc)) continue;
            yyjson_mut_val* id_val = yyjson_mut_obj_get(tc, "id");
            if (id_val && yyjson_mut_is_str(id_val)) {
                if (id_count >= max_ids) {
                    size_t new_max = max_ids * 2;
                    char** tmp = (char**)realloc(tool_call_ids, new_max * sizeof(char*));
                    if (!tmp) {
                        free_id_set(tool_call_ids, id_count);
                        yyjson_mut_doc_free(doc);
                        return NULL;
                    }
                    tool_call_ids = tmp;
                    max_ids = new_max;
                }
                /* Store a copy; will be freed after write */
                tool_call_ids[id_count++] = agent_strdup(yyjson_mut_get_str(id_val));
                if (!tool_call_ids[id_count - 1]) {
                    free_id_set(tool_call_ids, id_count - 1);
                    yyjson_mut_doc_free(doc);
                    return NULL;
                }
            }
        }
    }

    /* Pass 2: filter in reverse order - remove invalid messages */
    size_t total = yyjson_mut_arr_size(root);
    size_t* to_keep = (size_t*)calloc(total, sizeof(size_t));
    if (!to_keep) {
        free_id_set(tool_call_ids, id_count);
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t keep_count = 0;

    for (size_t i = 0; i < total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;

        yyjson_mut_val* role_val = yyjson_mut_obj_get(m, "role");
        const char* role_str = role_val ? yyjson_mut_get_str(role_val) : "";

        /* Check role allowlist */
        if (!is_valid_role(role_str)) {
            continue; /* Drop */
        }

        /* For tool messages: check if tool_call_id is known */
        if (strcmp(role_str, "tool") == 0) {
            yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
            const char* tcid_str = tcid ? yyjson_mut_get_str(tcid) : "";
            if (!is_known_id(tcid_str, tool_call_ids, id_count)) {
                continue; /* Drop orphan tool results */
            }
        }

        /* For assistant messages: repair empty tool-call names */
        if (strcmp(role_str, "assistant") == 0) {
            yyjson_mut_val* tcs = yyjson_mut_obj_get(m, "tool_calls");
            if (tcs && yyjson_mut_is_arr(tcs)) {
                size_t ti, tmax;
                yyjson_mut_val* tc;
                yyjson_mut_arr_foreach(tcs, ti, tmax, tc) {
                    if (!yyjson_mut_is_obj(tc)) continue;
                    yyjson_mut_val* fn = yyjson_mut_obj_get(tc, "function");
                    if (fn && yyjson_mut_is_obj(fn)) {
                        yyjson_mut_val* fn_name = yyjson_mut_obj_get(fn, "name");
                        if (!fn_name || !yyjson_mut_is_str(fn_name) ||
                            strlen(yyjson_mut_get_str(fn_name)) == 0) {
                            yyjson_mut_obj_put(fn,
                                yyjson_mut_strcpy(doc, "name"),
                                yyjson_mut_strcpy(doc, "unknown_tool"));
                        }
                    }
                }
            }

            /* Check for empty content without tool_calls payload */
            yyjson_mut_val* content = yyjson_mut_obj_get(m, "content");
            int has_content = content && yyjson_mut_is_str(content) &&
                               strlen(yyjson_mut_get_str(content)) > 0;
            int has_tc = tcs && yyjson_mut_is_arr(tcs) &&
                          yyjson_mut_arr_size(tcs) > 0;
            if (!has_content && !has_tc) {
                continue; /* Drop empty assistant without payload */
            }
        }

        /* For user/function messages: drop if empty content */
        if (strcmp(role_str, "user") == 0 || strcmp(role_str, "function") == 0) {
            yyjson_mut_val* content = yyjson_mut_obj_get(m, "content");
            int has_content = content && yyjson_mut_is_str(content) &&
                               strlen(yyjson_mut_get_str(content)) > 0;
            if (!has_content) {
                continue; /* Drop empty user/function */
            }
        }

        to_keep[keep_count++] = i;
    }

    /* Remove messages we don't want (in reverse order) */
    size_t* to_drop = (size_t*)calloc(total, sizeof(size_t));
    if (!to_drop) {
        free(to_keep);
        free_id_set(tool_call_ids, id_count);
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t drop_count = 0;
    for (size_t i = 0; i < total; i++) {
        int found = 0;
        for (size_t k = 0; k < keep_count; k++) {
            if (to_keep[k] == i) { found = 1; break; }
        }
        if (!found) to_drop[drop_count++] = i;
    }

    /* Remove in reverse order so indices stay valid */
    for (size_t d = drop_count; d > 0; d--) {
        yyjson_mut_arr_remove(root, to_drop[d - 1]);
    }
    free(to_drop);
    free(to_keep);

    /* Pass 3: mark found tool_call_ids by checking which have matching tool messages */
    size_t new_total = yyjson_mut_arr_size(root);
    for (size_t i = 0; i < id_count; i++) {
        int found = 0;
        for (size_t j = 0; j < new_total; j++) {
            yyjson_mut_val* m = yyjson_mut_arr_get(root, j);
            if (!m || !yyjson_mut_is_obj(m)) continue;
            yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
            const char* rs = rv ? yyjson_mut_get_str(rv) : "";
            if (strcmp(rs, "tool") != 0) continue;
            yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
            const char* tcs = tcid ? yyjson_mut_get_str(tcid) : "";
            if (tool_call_ids[i] && strcmp(tool_call_ids[i], tcs) == 0) {
                found = 1;
                break;
            }
        }
        /* Inject stub for missing results */
        if (!found && tool_call_ids[i]) {
            yyjson_mut_val* stub = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, stub, "role", "tool");
            yyjson_mut_obj_add_str(doc, stub, "tool_call_id", tool_call_ids[i]);
            yyjson_mut_obj_add_str(doc, stub, "content", "[tool output unavailable]");
            yyjson_mut_arr_append(root, stub);
        }
    }

    /* Write the doc while tool_call_ids strings are still alive */
    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    /* Now free the collected IDs */
    free_id_set(tool_call_ids, id_count);

    return result;
}

/* ------------------------------------------------------------------ */
/* repair_message_sequence                                             */
/* ------------------------------------------------------------------ */

char* repair_message_sequence(const char* messages_json) {
    if (!messages_json) return NULL;

    yyjson_doc* idoc = yyjson_read(messages_json, strlen(messages_json), 0);
    if (!idoc) return NULL;

    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return NULL;

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_arr(root)) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }

    /* Pass 0: Merge consecutive assistant turns.
     * Exempt messages with finish_reason == "incomplete". */
    int changed;
    do {
        changed = 0;
        size_t sz = yyjson_mut_arr_size(root);
        for (size_t i = 0; i + 1 < sz; i++) {
            yyjson_mut_val* m1 = yyjson_mut_arr_get(root, i);
            yyjson_mut_val* m2 = yyjson_mut_arr_get(root, i + 1);
            if (!m1 || !m2) continue;
            if (!yyjson_mut_is_obj(m1) || !yyjson_mut_is_obj(m2)) continue;

            yyjson_mut_val* r1 = yyjson_mut_obj_get(m1, "role");
            yyjson_mut_val* r2 = yyjson_mut_obj_get(m2, "role");
            const char* role1 = r1 ? yyjson_mut_get_str(r1) : "";
            const char* role2 = r2 ? yyjson_mut_get_str(r2) : "";

            if (strcmp(role1, "assistant") != 0 || strcmp(role2, "assistant") != 0)
                continue;

            /* Exempt finish_reason=incomplete */
            yyjson_mut_val* fr = yyjson_mut_obj_get(m2, "finish_reason");
            if (fr && yyjson_mut_is_str(fr)) {
                const char* fr_str = yyjson_mut_get_str(fr);
                if (fr_str && strcmp(fr_str, "incomplete") == 0)
                    continue;
            }

            /* Merge m2 into m1: concatenate content */
            yyjson_mut_val* c1 = yyjson_mut_obj_get(m1, "content");
            yyjson_mut_val* c2 = yyjson_mut_obj_get(m2, "content");
            if (c1 && c2 && yyjson_mut_is_str(c1) && yyjson_mut_is_str(c2)) {
                const char* s1 = yyjson_mut_get_str(c1);
                const char* s2 = yyjson_mut_get_str(c2);
                size_t newlen = strlen(s1) + strlen(s2) + 1;
                char* merged = (char*)malloc(newlen);
                if (merged) {
                    snprintf(merged, newlen, "%s%s", s1, s2);
                    yyjson_mut_obj_put(m1, yyjson_mut_strcpy(doc, "content"),
                                       yyjson_mut_strcpy(doc, merged));
                    free(merged);
                }
            }

            /* Merge tool_calls arrays */
            yyjson_mut_val* tc1 = yyjson_mut_obj_get(m1, "tool_calls");
            yyjson_mut_val* tc2 = yyjson_mut_obj_get(m2, "tool_calls");
            if (tc2 && yyjson_mut_is_arr(tc2)) {
                if (!tc1 || !yyjson_mut_is_arr(tc1)) {
                    yyjson_mut_obj_put(m1, yyjson_mut_strcpy(doc, "tool_calls"), tc2);
                } else {
                    size_t ti, tmax;
                    yyjson_mut_val* tc;
                    yyjson_mut_arr_foreach(tc2, ti, tmax, tc) {
                        yyjson_mut_arr_append(tc1, tc);
                    }
                }
            }

            /* Merge reasoning_content if present */
            yyjson_mut_val* rc2 = yyjson_mut_obj_get(m2, "reasoning_content");
            if (rc2) {
                yyjson_mut_val* rc1 = yyjson_mut_obj_get(m1, "reasoning_content");
                if (rc1 && yyjson_mut_is_str(rc1) && yyjson_mut_is_str(rc2)) {
                    const char* s1 = yyjson_mut_get_str(rc1);
                    const char* s2 = yyjson_mut_get_str(rc2);
                    size_t newlen = strlen(s1) + strlen(s2) + 1;
                    char* merged = (char*)malloc(newlen);
                    if (merged) {
                        snprintf(merged, newlen, "%s%s", s1, s2);
                        yyjson_mut_obj_put(m1, yyjson_mut_strcpy(doc, "reasoning_content"),
                                           yyjson_mut_strcpy(doc, merged));
                        free(merged);
                    }
                } else {
                    yyjson_mut_obj_put(m1, yyjson_mut_strcpy(doc, "reasoning_content"), rc2);
                }
            }

            yyjson_mut_arr_remove(root, i + 1);
            changed = 1;
            break;
        }
    } while (changed);

    /* Pass 1: Merge consecutive user messages when both have string content */
    do {
        changed = 0;
        size_t sz = yyjson_mut_arr_size(root);
        for (size_t i = 0; i + 1 < sz; i++) {
            yyjson_mut_val* m1 = yyjson_mut_arr_get(root, i);
            yyjson_mut_val* m2 = yyjson_mut_arr_get(root, i + 1);
            if (!m1 || !m2) continue;
            if (!yyjson_mut_is_obj(m1) || !yyjson_mut_is_obj(m2)) continue;

            yyjson_mut_val* r1 = yyjson_mut_obj_get(m1, "role");
            yyjson_mut_val* r2 = yyjson_mut_obj_get(m2, "role");
            const char* role1 = r1 ? yyjson_mut_get_str(r1) : "";
            const char* role2 = r2 ? yyjson_mut_get_str(r2) : "";

            if (strcmp(role1, "user") != 0 || strcmp(role2, "user") != 0)
                continue;

            yyjson_mut_val* c1 = yyjson_mut_obj_get(m1, "content");
            yyjson_mut_val* c2 = yyjson_mut_obj_get(m2, "content");
            if (!c1 || !c2 || !yyjson_mut_is_str(c1) || !yyjson_mut_is_str(c2))
                continue;

            const char* s1 = yyjson_mut_get_str(c1);
            const char* s2 = yyjson_mut_get_str(c2);
            size_t newlen = strlen(s1) + strlen(s2) + 1;
            char* merged = (char*)malloc(newlen);
            if (merged) {
                snprintf(merged, newlen, "%s%s", s1, s2);
                yyjson_mut_obj_put(m1, yyjson_mut_strcpy(doc, "content"),
                                   yyjson_mut_strcpy(doc, merged));
                free(merged);
                yyjson_mut_arr_remove(root, i + 1);
                changed = 1;
                break;
            }
        }
    } while (changed);

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return result;
}

/* ------------------------------------------------------------------ */
/* sanitize_and_repair_messages                                        */
/* ------------------------------------------------------------------ */

char* sanitize_and_repair_messages(const char* messages_json) {
    if (!messages_json) return NULL;

    char* sanitized = sanitize_api_messages(messages_json);
    if (!sanitized) return NULL;

    char* result = repair_message_sequence(sanitized);
    free(sanitized);
    return result;
}
