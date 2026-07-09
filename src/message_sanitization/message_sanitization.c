/* Message sanitization implementation
 *
 * Port of Hermes Agent agent/message_sanitization.py
 * Handles surrogate repair, JSON repair, non-ASCII stripping, image stripping.
 */
#include "message_sanitization.h"
#include "agent_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <yyjson.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Check if byte sequence at pos starts a UTF-8 encoded surrogate (U+D800-U+DFFF). */
static int is_utf8_surrogate(const unsigned char* s, size_t len) {
    if (len < 3) return 0;
    if (s[0] == 0xED && (s[1] & 0xE0) == 0xA0) return 1;
    return 0;
}

/* Replace surrogate code points in a string with U+FFFD (EF BF BD in UTF-8). */
static char* replace_surrogates_in_str(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char* out = (char*)mi_malloc(len * 3 + 1);
    if (!out) return NULL;
    size_t j = 0;
    const unsigned char* s = (const unsigned char*)input;
    size_t i = 0;
    while (i < len) {
        if (is_utf8_surrogate(s + i, len - i)) {
            out[j++] = (char)0xEF;
            out[j++] = (char)0xBF;
            out[j++] = (char)0xBD;
            i += 3;
        } else {
            out[j++] = input[i];
            i++;
        }
    }
    out[j] = '\0';
    return out;
}

/* Strip non-ASCII characters from a string (keep only ASCII 0x00-0x7F). */
static char* strip_non_ascii(const char* input) {
    if (!input) return NULL;
    size_t len = strlen(input);
    char* out = (char*)mi_malloc(len + 1);
    if (!out) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)input[i];
        if (c <= 0x7F) {
            out[j++] = input[i];
        }
    }
    out[j] = '\0';
    return out;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

char* sanitize_messages_surrogates(const char* messages_json) {
    if (!messages_json) return NULL;

    /* Allow invalid unicode so we can parse JSON with surrogate bytes */
    yyjson_read_err err;
    yyjson_doc* idoc = yyjson_read_opts((char*)messages_json, strlen(messages_json),
        YYJSON_READ_ALLOW_INVALID_UNICODE | YYJSON_READ_ALLOW_COMMENTS, NULL, &err);
    if (!idoc) return NULL;

    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return NULL;

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root || !yyjson_mut_is_arr(root)) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }

    /* Walk each message and replace surrogates in all string fields */
    /* For each string value, replace surrogates with U+FFFD */
    size_t idx, max;
    yyjson_mut_val* msg;
    yyjson_mut_arr_foreach(root, idx, max, msg) {
        if (!yyjson_mut_is_obj(msg)) continue;
        yyjson_mut_val *k, *v;
        yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(msg);
        while ((k = yyjson_mut_obj_iter_next(&iter)) != NULL) {
            v = yyjson_mut_obj_iter_get_val(k);
            if (v && yyjson_mut_is_str(v)) {
                const char* s = yyjson_mut_get_str(v);
                if (s) {
                    char* repaired = replace_surrogates_in_str(s);
                    if (repaired && strcmp(repaired, s) != 0) {
                        yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, yyjson_mut_get_str(k)),
                                           yyjson_mut_strcpy(doc, repaired));
                    }
                    mi_free(repaired);
                }
            } else if (v && yyjson_mut_is_arr(v)) {
                /* Walk array of content parts */
                size_t ai, amax;
                yyjson_mut_val* item;
                yyjson_mut_arr_foreach(v, ai, amax, item) {
                    if (!yyjson_mut_is_obj(item)) continue;
                    yyjson_mut_val *ik, *iv;
                    yyjson_mut_obj_iter iiter = yyjson_mut_obj_iter_with(item);
                    while ((ik = yyjson_mut_obj_iter_next(&iiter)) != NULL) {
                        iv = yyjson_mut_obj_iter_get_val(ik);
                        if (iv && yyjson_mut_is_str(iv)) {
                            const char* is = yyjson_mut_get_str(iv);
                            if (is) {
                                char* repaired = replace_surrogates_in_str(is);
                                if (repaired && strcmp(repaired, is) != 0) {
                                    yyjson_mut_obj_put(item,
                                        yyjson_mut_strcpy(doc, yyjson_mut_get_str(ik)),
                                        yyjson_mut_strcpy(doc, repaired));
                                }
                                mi_free(repaired);
                            }
                        }
                    }
                }
            }
        }
    }

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return result;
}

char* repair_tool_call_arguments(const char* raw_args, const char* tool_name) {
    (void)tool_name;
    if (!raw_args) return agent_strdup("{}");

    const char* s = raw_args;
    while (*s && (unsigned char)*s <= ' ') s++;
    size_t len = strlen(s);

    if (len == 0) return agent_strdup("{}");
    if (strcmp(s, "None") == 0) return agent_strdup("{}");

    /* Try parsing with yyjson (lenient by default) */
    yyjson_doc* doc = yyjson_read(s, len, YYJSON_READ_ALLOW_TRAILING_COMMAS |
                                            YYJSON_READ_ALLOW_COMMENTS |
                                            YYJSON_READ_ALLOW_INF_AND_NAN);
    if (doc) {
        yyjson_mut_doc* mdoc = yyjson_doc_mut_copy(doc, NULL);
        yyjson_doc_free(doc);
        if (mdoc) {
            char* json = yyjson_mut_write(mdoc, 0, NULL);
            yyjson_mut_doc_free(mdoc);
            return json ? json : agent_strdup("{}");
        }
        return agent_strdup("{}");
    }

    /* Repair trailing commas */
    char* fixed = agent_strdup(s);
    if (!fixed) return agent_strdup("{}");

    int changed;
    do {
        changed = 0;
        size_t flen = strlen(fixed);
        for (size_t i = 0; i + 1 < flen; i++) {
            if (fixed[i] == ',') {
                size_t j = i + 1;
                while (j < flen && (unsigned char)fixed[j] <= ' ') j++;
                if (j < flen && (fixed[j] == '}' || fixed[j] == ']')) {
                    memmove(fixed + i, fixed + j, flen - j + 1);
                    changed = 1;
                    break;
                }
            }
        }
    } while (changed);

    /* Close unclosed structures */
    int open_curly = 0, open_bracket = 0;
    size_t flen = strlen(fixed);
    for (size_t i = 0; i < flen; i++) {
        if (fixed[i] == '{') open_curly++;
        else if (fixed[i] == '}') open_curly--;
        else if (fixed[i] == '[') open_bracket++;
        else if (fixed[i] == ']') open_bracket--;
    }
    if (open_curly > 0) {
        size_t old_len = strlen(fixed);
        char* tmp = (char*)mi_realloc(fixed, old_len + open_curly + 1);
        if (!tmp) { mi_free(fixed); return agent_strdup("{}"); }
        fixed = tmp;
        for (int i = 0; i < open_curly; i++) fixed[old_len + i] = '}';
        fixed[old_len + open_curly] = '\0';
    }
    if (open_bracket > 0) {
        size_t old_len = strlen(fixed);
        char* tmp = (char*)mi_realloc(fixed, old_len + open_bracket + 1);
        if (!tmp) { mi_free(fixed); return agent_strdup("{}"); }
        fixed = tmp;
        for (int i = 0; i < open_bracket; i++) fixed[old_len + i] = ']';
        fixed[old_len + open_bracket] = '\0';
    }

    /* Remove excess closing braces */
    for (int iter = 0; iter < 50; iter++) {
        yyjson_doc* d2 = yyjson_read(fixed, strlen(fixed),
            YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS);
        if (d2) { yyjson_doc_free(d2); break; }
        size_t fl = strlen(fixed);
        if (fl == 0) break;
        if (fixed[fl-1] == '}') {
            int oc = 0, cc = 0;
            for (size_t i = 0; i < fl; i++) {
                if (fixed[i] == '{') oc++;
                else if (fixed[i] == '}') cc++;
            }
            if (cc > oc) { fixed[fl-1] = '\0'; continue; }
        }
        if (fixed[fl-1] == ']') {
            int ob = 0, cb = 0;
            for (size_t i = 0; i < fl; i++) {
                if (fixed[i] == '[') ob++;
                else if (fixed[i] == ']') cb++;
            }
            if (cb > ob) { fixed[fl-1] = '\0'; continue; }
        }
        break;
    }

    /* Try parsing repaired JSON */
    yyjson_doc* d2 = yyjson_read(fixed, strlen(fixed),
        YYJSON_READ_ALLOW_TRAILING_COMMAS | YYJSON_READ_ALLOW_COMMENTS |
        YYJSON_READ_ALLOW_INF_AND_NAN);
    if (d2) {
        yyjson_mut_doc* mdoc = yyjson_doc_mut_copy(d2, NULL);
        yyjson_doc_free(d2);
        if (mdoc) {
            char* json = yyjson_mut_write(mdoc, 0, NULL);
            yyjson_mut_doc_free(mdoc);
            mi_free(fixed);
            return json ? json : agent_strdup("{}");
        }
        mi_free(fixed);
        return agent_strdup("{}");
    }

    mi_free(fixed);
    return agent_strdup("{}");
}

char* escape_invalid_chars_in_json_strings(const char* raw) {
    if (!raw) return NULL;
    size_t len = strlen(raw);
    char* out = (char*)mi_malloc(len * 6 + 1);
    if (!out) return NULL;

    size_t j = 0;
    int in_string = 0;
    for (size_t i = 0; i < len; i++) {
        char ch = raw[i];
        if (in_string) {
            if (ch == '\\' && i + 1 < len) {
                out[j++] = ch;
                out[j++] = raw[i + 1];
                i++;
                continue;
            }
            if (ch == '"') {
                in_string = 0;
                out[j++] = ch;
            } else if ((unsigned char)ch < 0x20) {
                char buf[8];
                snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)ch);
                for (int k = 0; buf[k]; k++) out[j++] = buf[k];
            } else {
                out[j++] = ch;
            }
        } else {
            if (ch == '"') in_string = 1;
            out[j++] = ch;
        }
    }
    out[j] = '\0';
    return out;
}

char* sanitize_messages_non_ascii(const char* messages_json) {
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

    size_t idx, max;
    yyjson_mut_val* msg;
    yyjson_mut_arr_foreach(root, idx, max, msg) {
        if (!yyjson_mut_is_obj(msg)) continue;

        yyjson_mut_val* content = yyjson_mut_obj_get(msg, "content");
        if (content && yyjson_mut_is_str(content)) {
            const char* s = yyjson_mut_get_str(content);
            char* stripped = strip_non_ascii(s);
            if (stripped && strcmp(stripped, s) != 0) {
                yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, "content"),
                                   yyjson_mut_strcpy(doc, stripped));
            }
            mi_free(stripped);
        } else if (content && yyjson_mut_is_arr(content)) {
            size_t ci, cmax;
            yyjson_mut_val* part;
            yyjson_mut_arr_foreach(content, ci, cmax, part) {
                if (!yyjson_mut_is_obj(part)) continue;
                yyjson_mut_val* text = yyjson_mut_obj_get(part, "text");
                if (text && yyjson_mut_is_str(text)) {
                    const char* t = yyjson_mut_get_str(text);
                    char* stripped = strip_non_ascii(t);
                    if (stripped && strcmp(stripped, t) != 0) {
                        yyjson_mut_obj_put(part, yyjson_mut_strcpy(doc, "text"),
                                           yyjson_mut_strcpy(doc, stripped));
                    }
                    mi_free(stripped);
                }
            }
        }

        yyjson_mut_val* name = yyjson_mut_obj_get(msg, "name");
        if (name && yyjson_mut_is_str(name)) {
            const char* s = yyjson_mut_get_str(name);
            char* stripped = strip_non_ascii(s);
            if (stripped && strcmp(stripped, s) != 0) {
                yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, "name"),
                                   yyjson_mut_strcpy(doc, stripped));
            }
            mi_free(stripped);
        }

        yyjson_mut_val* tcs = yyjson_mut_obj_get(msg, "tool_calls");
        if (tcs && yyjson_mut_is_arr(tcs)) {
            size_t ti, tmax;
            yyjson_mut_val* tc;
            yyjson_mut_arr_foreach(tcs, ti, tmax, tc) {
                if (!yyjson_mut_is_obj(tc)) continue;
                yyjson_mut_val* fn = yyjson_mut_obj_get(tc, "function");
                if (fn && yyjson_mut_is_obj(fn)) {
                    yyjson_mut_val* args = yyjson_mut_obj_get(fn, "arguments");
                    if (args && yyjson_mut_is_str(args)) {
                        const char* a = yyjson_mut_get_str(args);
                        char* stripped = strip_non_ascii(a);
                        if (stripped && strcmp(stripped, a) != 0) {
                            yyjson_mut_obj_put(fn, yyjson_mut_strcpy(doc, "arguments"),
                                               yyjson_mut_strcpy(doc, stripped));
                        }
                        mi_free(stripped);
                    }
                }
            }
        }

        /* Process additional string fields (reasoning_content, etc.) */
        yyjson_mut_val *k, *v;
        yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(msg);
        while ((k = yyjson_mut_obj_iter_next(&iter)) != NULL) {
            const char* key = yyjson_mut_get_str(k);
            if (!key) continue;
            if (strcmp(key, "content") == 0 || strcmp(key, "name") == 0 ||
                strcmp(key, "tool_calls") == 0 || strcmp(key, "role") == 0)
                continue;
            v = yyjson_mut_obj_iter_get_val(k);
            if (v && yyjson_mut_is_str(v)) {
                const char* vs = yyjson_mut_get_str(v);
                char* stripped = strip_non_ascii(vs);
                if (stripped && strcmp(stripped, vs) != 0) {
                    yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, key),
                                       yyjson_mut_strcpy(doc, stripped));
                }
                mi_free(stripped);
            }
        }
    }

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return result;
}

char* strip_images_from_messages(const char* messages_json) {
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

    /* First pass: find indices to delete (non-tool messages with empty content after strip) */
    size_t total = yyjson_mut_arr_size(root);
    size_t* to_delete = (size_t*)mi_calloc(total, sizeof(size_t));
    if (!to_delete) {
        yyjson_mut_doc_free(doc);
        return NULL;
    }
    size_t del_count = 0;

    size_t idx, max;
    yyjson_mut_val* msg;
    yyjson_mut_arr_foreach(root, idx, max, msg) {
        if (!yyjson_mut_is_obj(msg)) continue;
        yyjson_mut_val* content = yyjson_mut_obj_get(msg, "content");
        if (!content || !yyjson_mut_is_arr(content)) continue;

        /* Build a new filtered parts array */
        int found_image = 0;
        int has_other = 0;
        size_t pi, pmax;
        yyjson_mut_val* part;
        yyjson_mut_arr_foreach(content, pi, pmax, part) {
            if (!yyjson_mut_is_obj(part)) { has_other = 1; continue; }
            yyjson_mut_val* ptype = yyjson_mut_obj_get(part, "type");
            const char* type_str = ptype ? yyjson_mut_get_str(ptype) : "";
            if (type_str && (strcmp(type_str, "image_url") == 0 ||
                             strcmp(type_str, "image") == 0 ||
                             strcmp(type_str, "input_image") == 0)) {
                found_image = 1;
            } else {
                has_other = 1;
            }
        }

        if (found_image) {
            if (has_other) {
                /* Build new parts list without images.
                 * Clone each retained part into the new array so that the
                 * original content array can be safely replaced without
                 * creating shared-value aliasing that yyjson release builds
                 * can serialize incorrectly. */
                yyjson_mut_val* new_parts = yyjson_mut_arr(doc);
                yyjson_mut_arr_foreach(content, pi, pmax, part) {
                    if (!yyjson_mut_is_obj(part)) {
                        yyjson_mut_arr_append(new_parts, yyjson_mut_val_mut_copy(doc, part));
                        continue;
                    }
                    yyjson_mut_val* ptype = yyjson_mut_obj_get(part, "type");
                    const char* type_str = ptype ? yyjson_mut_get_str(ptype) : "";
                    if (!(type_str && (strcmp(type_str, "image_url") == 0 ||
                                       strcmp(type_str, "image") == 0 ||
                                       strcmp(type_str, "input_image") == 0))) {
                        yyjson_mut_arr_append(new_parts, yyjson_mut_val_mut_copy(doc, part));
                    }
                }
                yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, "content"), new_parts);
            } else {
                /* Content was entirely images */
                yyjson_mut_val* role = yyjson_mut_obj_get(msg, "role");
                const char* role_str = role ? yyjson_mut_get_str(role) : "";
                if (role_str && strcmp(role_str, "tool") == 0) {
                    /* Tool message - preserve with placeholder */
                    yyjson_mut_obj_put(msg, yyjson_mut_strcpy(doc, "content"),
                        yyjson_mut_strcpy(doc, "[image content removed  --  server does not support images]"));
                } else {
                    /* Non-tool message - mark for deletion */
                    to_delete[del_count++] = idx;
                }
            }
        }
    }

    /* Delete marked messages in reverse order */
    for (size_t i = del_count; i > 0; i--) {
        yyjson_mut_arr_remove(root, to_delete[i - 1]);
    }
    mi_free(to_delete);

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);
    return result;
}
