/* Context compressor implementation
 *
 * Port of Hermes Agent agent/context_compressor.py helpers:
 *   - _sanitize_tool_pairs
 *   - _find_tail_cut_by_tokens
 *   - _build_static_fallback_summary
 *   - _prune_old_tool_results
 */
#include "context_compressor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <yyjson.h>

#define CHARS_PER_TOKEN 4
#define MSG_OVERHEAD_TOKENS 10

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

static size_t estimate_msg_tokens(yyjson_mut_val* msg) {
    if (!msg) return 0;
    size_t tokens = 0;
    yyjson_mut_val* content = yyjson_mut_obj_get(msg, "content");
    if (content) {
        if (yyjson_mut_is_str(content)) {
            tokens += strlen(yyjson_mut_get_str(content)) / CHARS_PER_TOKEN;
        } else if (yyjson_mut_is_arr(content)) {
            size_t ci, cmax;
            yyjson_mut_val* part;
            yyjson_mut_arr_foreach(content, ci, cmax, part) {
                if (yyjson_mut_is_str(part)) {
                    tokens += strlen(yyjson_mut_get_str(part)) / CHARS_PER_TOKEN;
                } else if (yyjson_mut_is_obj(part)) {
                    yyjson_mut_val* text = yyjson_mut_obj_get(part, "text");
                    if (text && yyjson_mut_is_str(text)) {
                        tokens += strlen(yyjson_mut_get_str(text)) / CHARS_PER_TOKEN;
                    }
                }
            }
        }
    }
    tokens += MSG_OVERHEAD_TOKENS;

    yyjson_mut_val* tcs = yyjson_mut_obj_get(msg, "tool_calls");
    if (tcs && yyjson_mut_is_arr(tcs)) {
        size_t ti, tmax;
        yyjson_mut_val* tc;
        yyjson_mut_arr_foreach(tcs, ti, tmax, tc) {
            if (yyjson_mut_is_obj(tc)) {
                size_t tc_est = 0;
                yyjson_mut_val *k, *v;
                yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(tc);
                while ((k = yyjson_mut_obj_iter_next(&iter)) != NULL) {
                    const char* ks = yyjson_mut_get_str(k);
                    if (ks) tc_est += strlen(ks) + 2;
                    v = yyjson_mut_obj_iter_get_val(k);
                    if (v) {
                        if (yyjson_mut_is_str(v)) {
                            const char* vs = yyjson_mut_get_str(v);
                            if (vs) tc_est += strlen(vs) + 2;
                        } else if (yyjson_mut_is_obj(v) || yyjson_mut_is_arr(v)) {
                            tc_est += 20;
                        } else { tc_est += 8; }
                    }
                }
                tokens += tc_est / CHARS_PER_TOKEN + 4;
            }
        }
    }
    return tokens;
}

static int id_in_set(const char* id, char** ids, size_t count) {
    if (!id || !ids) return 0;
    for (size_t i = 0; i < count; i++) {
        if (ids[i] && strcmp(ids[i], id) == 0) return 1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* sanitize_tool_pairs                                                 */
/* ------------------------------------------------------------------ */

char* sanitize_tool_pairs(const char* messages_json) {
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

    /* Collect all tool_call_ids from assistant messages */
    size_t max_ids = 256;
    char** call_ids = (char**)calloc(max_ids, sizeof(char*));
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
                    max_ids *= 2;
                    call_ids = (char**)realloc(call_ids, max_ids * sizeof(char*));
                }
                call_ids[id_count++] = strdup(yyjson_mut_get_str(id_val));
            }
        }
    }

    /* Remove orphan tool messages (in reverse order so indices stay valid) */
    size_t total = yyjson_mut_arr_size(root);
    int* to_drop = (int*)calloc(total, sizeof(int));
    int drop_count = 0;

    for (size_t i = 0; i < total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;
        yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
        const char* rs = rv ? yyjson_mut_get_str(rv) : "";
        if (strcmp(rs, "tool") == 0) {
            yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
            const char* tcs = tcid ? yyjson_mut_get_str(tcid) : "";
            if (!id_in_set(tcs, call_ids, id_count)) {
                to_drop[drop_count++] = (int)i;
            }
        }
    }

    for (int d = drop_count - 1; d >= 0; d--) {
        yyjson_mut_arr_remove(root, to_drop[d]);
    }
    free(to_drop);

    /* Inject stub results for missing tool calls */
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
            if (call_ids[i] && strcmp(call_ids[i], tcs) == 0) {
                found = 1;
                break;
            }
        }
        if (!found && call_ids[i]) {
            yyjson_mut_val* stub = yyjson_mut_obj(doc);
            yyjson_mut_obj_add_str(doc, stub, "role", "tool");
            yyjson_mut_obj_add_str(doc, stub, "tool_call_id", call_ids[i]);
            yyjson_mut_obj_add_str(doc, stub, "content", "[tool output unavailable]");
            yyjson_mut_arr_append(root, stub);
        }
    }

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    for (size_t i = 0; i < id_count; i++) free(call_ids[i]);
    free(call_ids);
    return result;
}

/* ------------------------------------------------------------------ */
/* find_tail_cut_by_tokens                                             */
/* ------------------------------------------------------------------ */

void find_tail_cut_by_tokens(const char* messages_json,
                             size_t head_end,
                             size_t token_budget,
                             size_t* out_tail_start,
                             size_t* out_tail_tokens) {
    if (out_tail_start) *out_tail_start = 0;
    if (out_tail_tokens) *out_tail_tokens = 0;
    if (!messages_json || !out_tail_start || !out_tail_tokens) return;

    yyjson_doc* idoc = yyjson_read(messages_json, strlen(messages_json), 0);
    if (!idoc) return;

    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return;

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_arr(root)) {
        yyjson_mut_doc_free(doc);
        return;
    }

    size_t total = yyjson_mut_arr_size(root);
    if (head_end >= total) {
        *out_tail_start = total;
        *out_tail_tokens = 0;
        yyjson_mut_doc_free(doc);
        return;
    }

    size_t accumulated = 0;
    size_t tail_start = total;
    for (size_t i = total; i > head_end; i--) {
        yyjson_mut_val* msg = yyjson_mut_arr_get(root, i - 1);
        if (!msg) continue;
        size_t msg_tokens = estimate_msg_tokens(msg);
        if (accumulated + msg_tokens > token_budget && (total - (i - 1)) > 1) {
            tail_start = i - 1;
            break;
        }
        accumulated += msg_tokens;
        tail_start = i - 1;
    }

    *out_tail_start = tail_start;
    *out_tail_tokens = accumulated;
    yyjson_mut_doc_free(doc);
}

/* ------------------------------------------------------------------ */
/* build_static_fallback_summary                                       */
/* ------------------------------------------------------------------ */

char* build_static_fallback_summary(const char* messages_json,
                                    size_t tail_start) {
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

    size_t total = yyjson_mut_arr_size(root);
    if (tail_start >= total) {
        yyjson_mut_doc_free(doc);
        return strdup("[No messages were pruned. -- REFERENCE ONLY] Respond only to the latest user message below.");
    }

    int user_count = 0, assistant_count = 0, tool_count = 0, system_count = 0;
    for (size_t i = 0; i < tail_start && i < total; i++) {
        yyjson_mut_val* msg = yyjson_mut_arr_get(root, i);
        if (!msg || !yyjson_mut_is_obj(msg)) continue;
        yyjson_mut_val* role = yyjson_mut_obj_get(msg, "role");
        const char* role_str = role ? yyjson_mut_get_str(role) : "";
        if (strcmp(role_str, "user") == 0) user_count++;
        else if (strcmp(role_str, "assistant") == 0) {
            assistant_count++;
            yyjson_mut_val* tcs = yyjson_mut_obj_get(msg, "tool_calls");
            if (tcs && yyjson_mut_is_arr(tcs)) {
                tool_count += (int)yyjson_mut_arr_size(tcs);
            }
        } else if (strcmp(role_str, "system") == 0) system_count++;
        else if (strcmp(role_str, "tool") == 0) tool_count++;
    }

    char buf[1024];
    int pos = snprintf(buf, sizeof(buf),
        "[CONTEXT COMPACTION  --  REFERENCE ONLY] %d messages pruned:",
        (int)tail_start);
    if (user_count > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %d user", user_count);
    if (assistant_count > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %d assistant", assistant_count);
    if (tool_count > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %d tool calls", tool_count);
    if (system_count > 0)
        pos += snprintf(buf + pos, sizeof(buf) - pos, " %d system", system_count);
    snprintf(buf + pos, sizeof(buf) - pos,
        ". Respond only to the latest user message below.");

    yyjson_mut_doc_free(doc);
    return strdup(buf);
}

/* ------------------------------------------------------------------ */
/* prune_old_tool_results                                              */
/* ------------------------------------------------------------------ */

char* prune_old_tool_results(const char* messages_json) {
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

    /* Collect tool_call_ids from assistant messages */
    size_t max_ids = 256;
    char** active_ids = (char**)calloc(max_ids, sizeof(char*));
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
            yyjson_mut_val* idv = yyjson_mut_obj_get(tc, "id");
            if (idv && yyjson_mut_is_str(idv)) {
                if (id_count >= max_ids) {
                    max_ids *= 2;
                    active_ids = (char**)realloc(active_ids, max_ids * sizeof(char*));
                }
                active_ids[id_count++] = strdup(yyjson_mut_get_str(idv));
            }
        }
    }

    /* Remove orphan tool results (reverse order) */
    size_t total = yyjson_mut_arr_size(root);
    int* to_drop = (int*)calloc(total, sizeof(int));
    int drop_count = 0;

    for (size_t i = 0; i < total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;
        yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
        const char* rs = rv ? yyjson_mut_get_str(rv) : "";
        if (strcmp(rs, "tool") != 0) continue;
        yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
        const char* tcs = tcid ? yyjson_mut_get_str(tcid) : "";
        if (!id_in_set(tcs, active_ids, id_count)) {
            to_drop[drop_count++] = (int)i;
        }
    }

    for (int d = drop_count - 1; d >= 0; d--) {
        yyjson_mut_arr_remove(root, to_drop[d]);
    }
    free(to_drop);

    /* Replace long content with placeholder in tool results */
    size_t new_total = yyjson_mut_arr_size(root);
    for (size_t i = 0; i < new_total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;
        yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
        const char* rs = rv ? yyjson_mut_get_str(rv) : "";
        if (strcmp(rs, "tool") != 0) continue;
        yyjson_mut_val* content = yyjson_mut_obj_get(m, "content");
        if (content && yyjson_mut_is_str(content) && strlen(yyjson_mut_get_str(content)) > 100) {
            yyjson_mut_obj_put(m, yyjson_mut_strcpy(doc, "content"),
                yyjson_mut_strcpy(doc, "[Old tool output cleared to save context space]"));
        }
    }

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    for (size_t i = 0; i < id_count; i++) free(active_ids[i]);
    free(active_ids);
    return result;
}
