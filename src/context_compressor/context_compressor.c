/* Context compressor implementation
 *
 * Port of Hermes Agent agent/context_compressor.py helpers:
 *   - _sanitize_tool_pairs
 *   - _find_tail_cut_by_tokens
 *   - _build_static_fallback_summary
 *   - _prune_old_tool_results
 */
#include "context_compressor.h"
#include "agent_compat.h"
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

static void free_id_set(char** ids, size_t count) {
    if (!ids) return;
    for (size_t i = 0; i < count; i++) free(ids[i]);
    free(ids);
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
    if (!call_ids) {
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
                    char** tmp = (char**)realloc(call_ids, new_max * sizeof(char*));
                    if (!tmp) {
                        free_id_set(call_ids, id_count);
                        yyjson_mut_doc_free(doc);
                        return NULL;
                    }
                    call_ids = tmp;
                    max_ids = new_max;
                }
                call_ids[id_count++] = agent_strdup(yyjson_mut_get_str(id_val));
                if (!call_ids[id_count - 1]) {
                    free_id_set(call_ids, id_count - 1);
                    yyjson_mut_doc_free(doc);
                    return NULL;
                }
            }
        }
    }

    /* Remove orphan tool messages (in reverse order so indices stay valid) */
    size_t total = yyjson_mut_arr_size(root);
    size_t* to_drop = (size_t*)calloc(total, sizeof(size_t));
    if (!to_drop) {
        free_id_set(call_ids, id_count);
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t drop_count = 0;

    for (size_t i = 0; i < total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;
        yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
        const char* rs = rv ? yyjson_mut_get_str(rv) : "";
        if (strcmp(rs, "tool") == 0) {
            yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
            const char* tcs = tcid ? yyjson_mut_get_str(tcid) : "";
            if (!id_in_set(tcs, call_ids, id_count)) {
                to_drop[drop_count++] = i;
            }
        }
    }

    for (size_t d = drop_count; d > 0; d--) {
        yyjson_mut_arr_remove(root, to_drop[d - 1]);
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
            yyjson_mut_obj_add_str(doc, stub, "name", "invalid_tool_call");
            yyjson_mut_obj_add_str(doc, stub, "content", "[Result unavailable \xe2\x80\x94 see context summary above]");
            yyjson_mut_arr_append(root, stub);
        }
    }

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    free_id_set(call_ids, id_count);
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
        return agent_strdup("[No messages were pruned. -- REFERENCE ONLY] Respond only to the latest user message below.");
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

    /* Compute required size first, then allocate exactly once. */
    int prefix_len = snprintf(NULL, 0,
        "[CONTEXT COMPACTION  --  REFERENCE ONLY] %d messages pruned:",
        (int)tail_start);
    int suffix_len = snprintf(NULL, 0,
        ". Respond only to the latest user message below.");
    int user_len = (user_count > 0) ? snprintf(NULL, 0, " %d user", user_count) : 0;
    int assistant_len = (assistant_count > 0) ? snprintf(NULL, 0, " %d assistant", assistant_count) : 0;
    int tool_len = (tool_count > 0) ? snprintf(NULL, 0, " %d tool calls", tool_count) : 0;
    int system_len = (system_count > 0) ? snprintf(NULL, 0, " %d system", system_count) : 0;

    if (prefix_len < 0 || suffix_len < 0 || user_len < 0 || assistant_len < 0 ||
        tool_len < 0 || system_len < 0) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }

    size_t buf_size = (size_t)prefix_len + (size_t)suffix_len +
                      (size_t)user_len + (size_t)assistant_len +
                      (size_t)tool_len + (size_t)system_len + 1;
    char* buf = (char*)malloc(buf_size);
    if (!buf) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }

    size_t pos = 0;
    pos += (size_t)snprintf(buf + pos, buf_size - pos,
        "[CONTEXT COMPACTION  --  REFERENCE ONLY] %d messages pruned:",
        (int)tail_start);
    if (user_count > 0)
        pos += (size_t)snprintf(buf + pos, buf_size - pos, " %d user", user_count);
    if (assistant_count > 0)
        pos += (size_t)snprintf(buf + pos, buf_size - pos, " %d assistant", assistant_count);
    if (tool_count > 0)
        pos += (size_t)snprintf(buf + pos, buf_size - pos, " %d tool calls", tool_count);
    if (system_count > 0)
        pos += (size_t)snprintf(buf + pos, buf_size - pos, " %d system", system_count);
    snprintf(buf + pos, buf_size - pos,
        ". Respond only to the latest user message below.");

    yyjson_mut_doc_free(doc);
    return buf;
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
    if (!active_ids) {
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
            yyjson_mut_val* idv = yyjson_mut_obj_get(tc, "id");
            if (idv && yyjson_mut_is_str(idv)) {
                if (id_count >= max_ids) {
                    size_t new_max = max_ids * 2;
                    char** tmp = (char**)realloc(active_ids, new_max * sizeof(char*));
                    if (!tmp) {
                        free_id_set(active_ids, id_count);
                        yyjson_mut_doc_free(doc);
                        return NULL;
                    }
                    active_ids = tmp;
                    max_ids = new_max;
                }
                active_ids[id_count++] = agent_strdup(yyjson_mut_get_str(idv));
                if (!active_ids[id_count - 1]) {
                    free_id_set(active_ids, id_count - 1);
                    yyjson_mut_doc_free(doc);
                    return NULL;
                }
            }
        }
    }

    /* Remove orphan tool results (reverse order) */
    size_t total = yyjson_mut_arr_size(root);
    size_t* to_drop = (size_t*)calloc(total, sizeof(size_t));
    if (!to_drop) {
        free_id_set(active_ids, id_count);
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t drop_count = 0;

    for (size_t i = 0; i < total; i++) {
        yyjson_mut_val* m = yyjson_mut_arr_get(root, i);
        if (!m || !yyjson_mut_is_obj(m)) continue;
        yyjson_mut_val* rv = yyjson_mut_obj_get(m, "role");
        const char* rs = rv ? yyjson_mut_get_str(rv) : "";
        if (strcmp(rs, "tool") != 0) continue;
        yyjson_mut_val* tcid = yyjson_mut_obj_get(m, "tool_call_id");
        const char* tcs = tcid ? yyjson_mut_get_str(tcid) : "";
        if (!id_in_set(tcs, active_ids, id_count)) {
            to_drop[drop_count++] = i;
        }
    }

    for (size_t d = drop_count; d > 0; d--) {
        yyjson_mut_arr_remove(root, to_drop[d - 1]);
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

    free_id_set(active_ids, id_count);
    return result;
}

/* ------------------------------------------------------------------ */
/* truncate_tool_call_args_json                                        */
/* ------------------------------------------------------------------ */

/* Recursive helper: walk yyjson_mut_val and truncate long string leaves. */
static void truncate_strings_in_val(yyjson_mut_doc* doc, yyjson_mut_val* val,
                                     size_t head_chars) {
    if (!val) return;

    if (yyjson_mut_is_str(val)) {
        const char* s = yyjson_mut_get_str(val);
        if (s && strlen(s) > head_chars) {
            /* Build truncated string: first head_chars chars + "...[truncated]" */
            size_t prefix_len = head_chars;
            const char* suffix = "...[truncated]";
            size_t suffix_len = strlen(suffix);
            size_t buf_size = prefix_len + suffix_len + 1;
            char* buf = (char*)malloc(buf_size);
            if (!buf) return;
            memcpy(buf, s, prefix_len);
            memcpy(buf + prefix_len, suffix, suffix_len);
            buf[buf_size - 1] = '\0';
            yyjson_mut_set_str(val, yyjson_mut_get_str(yyjson_mut_strcpy(doc, buf)));
            free(buf);
        }
    } else if (yyjson_mut_is_obj(val)) {
        yyjson_mut_val *k, *v;
        yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(val);
        while ((k = yyjson_mut_obj_iter_next(&iter)) != NULL) {
            v = yyjson_mut_obj_iter_get_val(k);
            truncate_strings_in_val(doc, v, head_chars);
        }
    } else if (yyjson_mut_is_arr(val)) {
        size_t idx, max;
        yyjson_mut_val* item;
        yyjson_mut_arr_foreach(val, idx, max, item) {
            truncate_strings_in_val(doc, item, head_chars);
        }
    }
}

char* truncate_tool_call_args_json(const char* args, size_t head_chars) {
    if (!args) return NULL;
    if (strlen(args) == 0) return agent_strdup("");

    yyjson_doc* idoc = yyjson_read(args, strlen(args), 0);
    if (!idoc) {
        /* Not valid JSON — return original string (graceful fallback) */
        return agent_strdup(args);
    }

    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return agent_strdup(args);

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return agent_strdup(args);
    }

    /* Walk the structure and truncate long string leaves */
    truncate_strings_in_val(doc, root, head_chars);

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    if (!result) return agent_strdup(args);
    return result;
}
