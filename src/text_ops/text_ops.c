/* Text operations implementation
 *
 * Port of Hermes Agent scalar text transforms:
 *   - agent/agent_runtime_helpers._strip_think_blocks_pure
 *   - agent/tool_dedup._canonical_tool_arguments_text
 *   - agent/message_sanitization._sanitize_surrogates
 *   - agent/message_sanitization._strip_non_ascii
 *   - agent/model_metadata.estimate_tokens_rough
 */
#include "text_ops.h"
#include "agent_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <yyjson.h>

/* ------------------------------------------------------------------ */
/* Internal helpers                                                    */
/* ------------------------------------------------------------------ */

/* Case-insensitive ASCII compare up to n chars. */
static int ieq_n(const char* a, const char* b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        int ca = (unsigned char)a[i];
        int cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return 0;
    }
    return 1;
}

/* Check if character at pos is at a block boundary for <function> gating. */
static int at_block_boundary(const char* content, size_t pos) {
    if (pos == 0) return 1;
    unsigned char c = (unsigned char)content[pos - 1];
    return (c == '\n' || c == '\r' || c == '.' || c == '!' || c == '?' || c == ':');
}

/* ------------------------------------------------------------------ */
/* strip_think_blocks                                                  */
/* ------------------------------------------------------------------ */

/* Reasoning/thinking tag names (all used case-insensitively). */
static const char* REASONING_TAGS[] = {
    "think", "thinking", "reasoning", "thought", "REASONING_SCRATCHPAD", NULL
};

/* Tool-call tag names for closed-pair removal. */
static const char* TOOLCALL_TAGS[] = {
    "tool_call", "tool_calls", "tool_result",
    "function_call", "function_calls", NULL
};

/* Check if name matches any of the reasoning tags (case-insensitive). */
static int is_reasoning_tag(const char* name, size_t nlen) {
    for (int i = 0; REASONING_TAGS[i]; i++) {
        if (ieq_n(name, REASONING_TAGS[i], nlen) &&
            strlen(REASONING_TAGS[i]) == nlen) return 1;
    }
    return 0;
}

/* Check if name matches any of the tool-call tags (case-insensitive). */
static int is_toolcall_tag(const char* name, size_t nlen) {
    for (int i = 0; TOOLCALL_TAGS[i]; i++) {
        if (ieq_n(name, TOOLCALL_TAGS[i], nlen) &&
            strlen(TOOLCALL_TAGS[i]) == nlen) return 1;
    }
    return 0;
}

/* Scan forward for a closing tag matching the open tag. */
static size_t find_closing_tag(const char* content, size_t start,
                                size_t content_len,
                                const char* tag_name, size_t tlen) {
    size_t pos = start;
    while (pos + 3 + tlen <= content_len) {
        if (content[pos] == '<' && content[pos + 1] == '/' &&
            ieq_n(content + pos + 2, tag_name, tlen) &&
            pos + 2 + tlen < content_len &&
            content[pos + 2 + tlen] == '>') {
            return pos + 3 + tlen;
        }
        pos++;
    }
    return content_len;
}

/* Find the end of a tag: scan forward from pos (the position just after '<')
 * to the matching '>'. */
static size_t tag_end(const char* content, size_t pos, size_t content_len) {
    while (pos < content_len && content[pos] != '>') pos++;
    if (pos < content_len) pos++;
    return pos;
}

/* Remove a range [start, end) by copying everything outside it into a new
 * buffer. Returns newly allocated string (caller must free), or NULL on
 * allocation failure. */
static char* remove_range(const char* content, size_t content_len,
                          size_t start, size_t end) {
    size_t new_len = content_len - (end - start);
    char* result = (char*)malloc(new_len + 1);
    if (!result) return NULL;
    if (start > 0) memcpy(result, content, start);
    if (end < content_len) memcpy(result + start, content + end, content_len - end);
    result[new_len] = '\0';
    return result;
}

/* Remove closed tag pairs: <tag_name ...> ... </tag_name> */
static int scan_closed_pair(const char* content, size_t len, size_t pos,
                            size_t* out_start, size_t* out_end,
                            const char** tag_list) {
    for (int ti = 0; tag_list[ti]; ti++) {
        const char* tname = tag_list[ti];
        size_t tlen = strlen(tname);

        if (pos + 1 + tlen <= len &&
            content[pos] == '<' &&
            ieq_n(content + pos + 1, tname, tlen)) {
            unsigned char next = (unsigned char)content[pos + 1 + tlen];
            if (next == '>' || next == ' ' || next == '\t' || next == '\n' || next == '\r') {
                size_t open_end = tag_end(content, pos + 1, len);
                size_t close_end = find_closing_tag(content, open_end, len, tname, tlen);
                if (close_end > pos) {
                    *out_start = pos;
                    *out_end = close_end;
                    return 1;
                }
            }
        }
    }
    return 0;
}

/* Remove <function name="...">...</function> at block boundaries. */
static int scan_function_block(const char* content, size_t len, size_t pos,
                               size_t* out_start, size_t* out_end) {
    if (pos + 9 >= len) return 0;
    if (content[pos] != '<') return 0;
    if (!ieq_n(content + pos + 1, "function", 8)) return 0;
    unsigned char after_fn = (unsigned char)content[pos + 9];
    if (after_fn != ' ' && after_fn != '\t' && after_fn != '>' && after_fn != '\n' && after_fn != '\r') return 0;

    if (!at_block_boundary(content, pos)) return 0;

    /* Check for name="..." attribute */
    size_t attr_len = (len - (pos + 9));
    int has_name_attr = 0;
    for (size_t i = 0; i < attr_len && content[pos + 9 + i] != '>'; i++) {
        if (i + 4 < attr_len &&
            ieq_n(content + pos + 9 + i, "name", 4) &&
            (i == 0 || content[pos + 9 + i - 1] == ' ' || content[pos + 9 + i - 1] == '\t')) {
            has_name_attr = 1;
            break;
        }
    }
    if (!has_name_attr) return 0;

    size_t open_end = tag_end(content, pos + 1, len);
    size_t close_end = find_closing_tag(content, open_end, len, "function", 8);
    if (close_end <= open_end) return 0;

    *out_start = pos;
    *out_end = close_end;
    return 1;
}

/* Remove unterminated reasoning open tag at block boundary to end of string. */
static int scan_unterminated_reasoning(const char* content, size_t len, size_t pos,
                                       size_t* out_start, size_t* out_end) {
    if (pos > 0 && content[pos - 1] != '\n' && content[pos - 1] != '\r') return 0;

    size_t scan_pos = pos;
    while (scan_pos < len && (content[scan_pos] == ' ' || content[scan_pos] == '\t')) scan_pos++;

    if (scan_pos >= len || content[scan_pos] != '<') return 0;

    size_t tag_start = scan_pos + 1;
    for (int ti = 0; REASONING_TAGS[ti]; ti++) {
        const char* tname = REASONING_TAGS[ti];
        size_t tlen = strlen(tname);
        if (tag_start + tlen <= len &&
            ieq_n(content + tag_start, tname, tlen)) {
            unsigned char next = (unsigned char)content[tag_start + tlen];
            if (next == '>' || next == ' ' || next == '\t' || next == '\n' || next == '\r') {
                *out_start = pos;
                *out_end = len;
                return 1;
            }
        }
    }
    return 0;
}

/* Remove stray orphan open/close reasoning tags. */
static int scan_stray_reasoning_tag(const char* content, size_t len, size_t pos,
                                    size_t* out_start, size_t* out_end) {
    if (content[pos] != '<') return 0;

    int is_close = (pos + 1 < len && content[pos + 1] == '/');
    size_t name_start = pos + 1 + (is_close ? 1 : 0);

    for (int ti = 0; REASONING_TAGS[ti]; ti++) {
        const char* tname = REASONING_TAGS[ti];
        size_t tlen = strlen(tname);
        if (name_start + tlen <= len &&
            ieq_n(content + name_start, tname, tlen)) {
            unsigned char after = (unsigned char)content[name_start + tlen];
            if (after == '>' || (is_close && after == '>')) {
                size_t tag_end_pos = tag_end(content, name_start - (is_close ? 1 : 0), len);
                while (tag_end_pos < len && (content[tag_end_pos] == ' ' ||
                       content[tag_end_pos] == '\t' || content[tag_end_pos] == '\n' ||
                       content[tag_end_pos] == '\r')) tag_end_pos++;
                *out_start = pos;
                *out_end = tag_end_pos;
                return 1;
            }
        }
    }
    return 0;
}

/* Remove stray tool-call closers. */
static int scan_stray_toolcall_close(const char* content, size_t len, size_t pos,
                                     size_t* out_start, size_t* out_end) {
    if (pos + 2 >= len || content[pos] != '<' || content[pos + 1] != '/') return 0;

    size_t name_start = pos + 2;
    for (int ti = 0; TOOLCALL_TAGS[ti]; ti++) {
        const char* tname = TOOLCALL_TAGS[ti];
        size_t tlen = strlen(tname);
        if (name_start + tlen <= len &&
            ieq_n(content + name_start, tname, tlen) &&
            content[name_start + tlen] == '>') {
            size_t close_tag_end = name_start + tlen + 1;
            while (close_tag_end < len && (content[close_tag_end] == ' ' ||
                   content[close_tag_end] == '\t' || content[close_tag_end] == '\n' ||
                   content[close_tag_end] == '\r')) close_tag_end++;
            *out_start = pos;
            *out_end = close_tag_end;
            return 1;
        }
    }
    /* Also handle </function> */
    if (name_start + 8 <= len &&
        ieq_n(content + name_start, "function", 8) &&
        content[name_start + 8] == '>') {
        size_t close_tag_end = name_start + 9;
        while (close_tag_end < len && (content[close_tag_end] == ' ' ||
               content[close_tag_end] == '\t' || content[close_tag_end] == '\n' ||
               content[close_tag_end] == '\r')) close_tag_end++;
        *out_start = pos;
        *out_end = close_tag_end;
        return 1;
    }
    return 0;
}

/* Apply a scanner pass: find all non-overlapping matches and build a new
 * string with matches removed. */
static char* apply_scanner_pass(const char* content, size_t len,
                                int (*scanner)(const char*, size_t, size_t,
                                               size_t*, size_t*)) {
    size_t max_matches = 64;
    size_t* starts = (size_t*)malloc(max_matches * sizeof(size_t));
    size_t* ends = (size_t*)malloc(max_matches * sizeof(size_t));
    if (!starts || !ends) {
        free(starts); free(ends);
        return NULL;
    }
    size_t match_count = 0;

    size_t pos = 0;
    while (pos < len) {
        size_t mstart = len, mend = len;
        if (scanner(content, len, pos, &mstart, &mend) && mend > mstart) {
            if (match_count >= max_matches) {
                max_matches *= 2;
                size_t* ns = (size_t*)realloc(starts, max_matches * sizeof(size_t));
                size_t* ne = (size_t*)realloc(ends, max_matches * sizeof(size_t));
                if (!ns || !ne) { free(ns ? ns : starts); free(ne ? ne : ends); return NULL; }
                starts = ns; ends = ne;
            }
            starts[match_count] = mstart;
            ends[match_count] = mend;
            match_count++;
            pos = mend;
        } else {
            pos++;
        }
    }

    if (match_count == 0) {
        free(starts); free(ends);
        return agent_strdup(content);
    }

    /* Merge overlapping/adjacent ranges */
    for (size_t i = 1; i < match_count; i++) {
        if (starts[i] <= ends[i - 1]) {
            if (ends[i] > ends[i - 1]) ends[i - 1] = ends[i];
            for (size_t j = i; j < match_count - 1; j++) {
                starts[j] = starts[j + 1];
                ends[j] = ends[j + 1];
            }
            match_count--;
            i--;
        }
    }

    /* Build output */
    size_t out_len = len;
    for (size_t i = 0; i < match_count; i++) out_len -= (ends[i] - starts[i]);

    char* result = (char*)malloc(out_len + 1);
    if (!result) { free(starts); free(ends); return NULL; }

    size_t out_pos = 0;
    size_t src_pos = 0;
    for (size_t i = 0; i < match_count; i++) {
        size_t before_len = starts[i] - src_pos;
        if (before_len > 0) {
            memcpy(result + out_pos, content + src_pos, before_len);
            out_pos += before_len;
        }
        src_pos = ends[i];
    }
    if (src_pos < len) {
        memcpy(result + out_pos, content + src_pos, len - src_pos);
        out_pos += (len - src_pos);
    }
    result[out_pos] = '\0';

    free(starts); free(ends);
    return result;
}

static int scan_closed_reasoning(const char* content, size_t len, size_t pos,
                                 size_t* out_start, size_t* out_end) {
    return scan_closed_pair(content, len, pos, out_start, out_end, REASONING_TAGS);
}

/* Scan for triple-backtick thinking blocks:  thinking...  response */
static int scan_think_codeblock(const char* content, size_t len, size_t pos,
                                 size_t* out_start, size_t* out_end) {
    /* Look for  thinking (case-insensitive, with backticks) */
    if (pos + 11 > len) return 0;
    if (content[pos] != '`' || content[pos+1] != '`' || content[pos+2] != '`') return 0;
    if (!ieq_n(content + pos + 3, "think", 5) && !ieq_n(content + pos + 3, "thinking", 8)) return 0;
    
    /* Check that after the tag name there's whitespace or newline */
    size_t tag_start = pos + 3;
    size_t tag_nlen = 0;
    if (ieq_n(content + tag_start, "thinking", 8)) tag_nlen = 8;
    else if (ieq_n(content + tag_start, "think", 5)) tag_nlen = 5;
    else return 0;
    
    unsigned char after = (unsigned char)content[tag_start + tag_nlen];
    if (after != ' ' && after != '\t' && after != '\n' && after != '\r' && after != '\0') return 0;
    
    /* Now look for  response closing marker */
    size_t scan_pos = tag_start + tag_nlen;
    while (scan_pos + 11 <= len) {
        if (content[scan_pos] == '`' && content[scan_pos+1] == '`' && content[scan_pos+2] == '`') {
            if (ieq_n(content + scan_pos + 3, "response", 8)) {
                /* Found close marker at scan_pos */
                /* Also check for any non-backtick char after the close */
                size_t close_end = scan_pos + 11;
                *out_start = pos;
                *out_end = close_end;
                return 1;
            }
        }
        scan_pos++;
    }
    
    return 0;
}

static int scan_closed_toolcall(const char* content, size_t len, size_t pos,
                                size_t* out_start, size_t* out_end) {
    return scan_closed_pair(content, len, pos, out_start, out_end, TOOLCALL_TAGS);
}

char* strip_think_blocks(const char* content) {
    if (!content) return agent_strdup("");
    size_t len = strlen(content);
    if (len == 0) return agent_strdup("");

    char* current = agent_strdup(content);
    if (!current) return NULL;
    char* next;

    /* Pass 0: Triple-backtick thinking code blocks */
    next = apply_scanner_pass(current, strlen(current), scan_think_codeblock);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 1: Closed reasoning tag pairs */
    next = apply_scanner_pass(current, strlen(current), scan_closed_reasoning);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 2: Closed tool-call tag pairs */
    next = apply_scanner_pass(current, strlen(current), scan_closed_toolcall);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 3: <function name="...">...</function> at block boundaries */
    next = apply_scanner_pass(current, strlen(current), scan_function_block);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 4: Unterminated reasoning open tag at block boundary to end */
    next = apply_scanner_pass(current, strlen(current), scan_unterminated_reasoning);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 5: Stray orphan open/close reasoning tags */
    next = apply_scanner_pass(current, strlen(current), scan_stray_reasoning_tag);
    free(current);
    if (!next) return NULL;
    current = next;

    /* Pass 6: Stray tool-call closers */
    next = apply_scanner_pass(current, strlen(current), scan_stray_toolcall_close);
    free(current);
    if (!next) return NULL;
    current = next;

    return current;
}

/* ------------------------------------------------------------------ */
/* canonical_json_sort                                                 */
/* ------------------------------------------------------------------ */

/* Recursively sort JSON object keys using yyjson. */
static int sort_json_val(yyjson_mut_doc* doc, yyjson_mut_val* val) {
    if (!val) return 0;

    if (yyjson_mut_is_obj(val)) {
        size_t count = yyjson_mut_obj_size(val);
        if (count == 0) return 1;

        /* Collect all key-value pairs into arrays */
        yyjson_mut_val** keys = (yyjson_mut_val**)malloc(count * sizeof(yyjson_mut_val*));
        yyjson_mut_val** vals = (yyjson_mut_val**)malloc(count * sizeof(yyjson_mut_val*));
        if (!keys || !vals) { free(keys); free(vals); return 0; }

        /* Snapshot all keys and values before any mutation */
        yyjson_mut_val *k;
        yyjson_mut_obj_iter iter = yyjson_mut_obj_iter_with(val);
        size_t idx = 0;
        while ((k = yyjson_mut_obj_iter_next(&iter)) != NULL) {
            yyjson_mut_val* v = yyjson_mut_obj_iter_get_val(k);
            keys[idx] = k;
            vals[idx] = v;
            idx++;
        }

        /* Recursively sort values */
        for (size_t i = 0; i < count; i++) {
            sort_json_val(doc, vals[i]);
        }

        /* Insertion sort by key string */
        for (size_t i = 0; i < count; i++) {
            for (size_t j = i + 1; j < count; j++) {
                const char* ki = yyjson_mut_get_str(keys[i]);
                const char* kj = yyjson_mut_get_str(keys[j]);
                if (ki && kj && strcmp(ki, kj) > 0) {
                    yyjson_mut_val* tmpk = keys[i]; keys[i] = keys[j]; keys[j] = tmpk;
                    yyjson_mut_val* tmpv = vals[i]; vals[i] = vals[j]; vals[j] = tmpv;
                }
            }
        }

        /* Clear the object and re-add in sorted order */
        yyjson_mut_obj_clear(val);
        for (size_t i = 0; i < count; i++) {
            yyjson_mut_obj_add(val, keys[i], vals[i]);
        }

        free(keys); free(vals);
    } else if (yyjson_mut_is_arr(val)) {
        size_t idx2, max;
        yyjson_mut_val* item;
        yyjson_mut_arr_foreach(val, idx2, max, item) {
            sort_json_val(doc, item);
        }
    }

    return 1;
}

char* canonical_json_sort(const char* json_str) {
    if (!json_str || strlen(json_str) == 0) return agent_strdup("");

    yyjson_doc* idoc = yyjson_read(json_str, strlen(json_str), 0);
    if (!idoc) {
        return agent_strdup(json_str);
    }

    yyjson_mut_doc* doc = yyjson_doc_mut_copy(idoc, NULL);
    yyjson_doc_free(idoc);
    if (!doc) return agent_strdup(json_str);

    yyjson_mut_val* root = yyjson_mut_doc_get_root(doc);
    if (!root) {
        yyjson_mut_doc_free(doc);
        return agent_strdup(json_str);
    }

    sort_json_val(doc, root);

    char* result = yyjson_mut_write(doc, 0, NULL);
    yyjson_mut_doc_free(doc);

    if (!result) return agent_strdup(json_str);
    return result;
}

/* ------------------------------------------------------------------ */
/* sanitize_surrogates_str                                             */
/* ------------------------------------------------------------------ */

static int is_surrogate_start(const unsigned char* bytes, size_t len, size_t pos) {
    if (pos + 3 > len) return 0;
    return (bytes[pos] == 0xED &&
            bytes[pos + 1] >= 0xA0 && bytes[pos + 1] <= 0xBF &&
            bytes[pos + 2] >= 0x80 && bytes[pos + 2] <= 0xBF);
}

char* sanitize_surrogates_str(const char* text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    if (len == 0) return agent_strdup("");

    const unsigned char* bytes = (const unsigned char*)text;

    size_t surrogate_count = 0;
    for (size_t i = 0; i < len; i++) {
        if (is_surrogate_start(bytes, len, i)) {
            surrogate_count++;
            i += 2;
        }
    }

    if (surrogate_count == 0) {
        return agent_strdup(text);
    }

    size_t out_len = len;
    char* result = (char*)malloc(out_len + 1);
    if (!result) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < len; i++) {
        if (is_surrogate_start(bytes, len, i)) {
            result[out_pos++] = (char)0xEF;
            result[out_pos++] = (char)0xBF;
            result[out_pos++] = (char)0xBD;
            i += 2;
        } else {
            result[out_pos++] = text[i];
        }
    }
    result[out_pos] = '\0';

    return result;
}

/* ------------------------------------------------------------------ */
/* strip_non_ascii_str                                                 */
/* ------------------------------------------------------------------ */

char* strip_non_ascii_str(const char* text) {
    if (!text) return NULL;

    size_t len = strlen(text);
    if (len == 0) return agent_strdup("");

    size_t ascii_count = 0;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)text[i] <= 0x7F) ascii_count++;
    }

    if (ascii_count == len) return agent_strdup(text);

    char* result = (char*)malloc(ascii_count + 1);
    if (!result) return NULL;

    size_t out_pos = 0;
    for (size_t i = 0; i < len; i++) {
        if ((unsigned char)text[i] <= 0x7F) {
            result[out_pos++] = text[i];
        }
    }
    result[out_pos] = '\0';

    return result;
}

/* ------------------------------------------------------------------ */
/* estimate_tokens_rough                                               */
/* ------------------------------------------------------------------ */

static size_t utf8_code_point_count(const unsigned char* bytes, size_t len) {
    size_t count = 0;
    size_t i = 0;
    while (i < len) {
        unsigned char c = bytes[i];
        if (c <= 0x7F) {
            count++;
            i += 1;
        } else if (c >= 0xC0 && c <= 0xDF) {
            count++;
            i += 2;
        } else if (c >= 0xE0 && c <= 0xEF) {
            count++;
            i += 3;
        } else if (c >= 0xF0 && c <= 0xF7) {
            count++;
            i += 4;
        } else {
            i++;
        }
    }
    return count;
}

size_t estimate_tokens_rough(const char* text) {
    if (!text || strlen(text) == 0) return 0;

    size_t cp = utf8_code_point_count((const unsigned char*)text, strlen(text));
    return (cp + 3) / 4;
}