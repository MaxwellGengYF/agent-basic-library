/* Prompt builder implementation
 *
 * Port of Hermes Agent agent/prompt_builder.py
 * Content truncation, YAML frontmatter stripping, context assembly.
 */
#include "prompt_builder.h"
#include "agent_compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ------------------------------------------------------------------ */
/* truncate_content                                                    */
/* ------------------------------------------------------------------ */

char* truncate_content(const char* content, const char* filename,
                       size_t max_chars, const char* read_path) {
    (void)filename;
    (void)read_path;

    if (!content) return NULL;
    if (max_chars == 0) return agent_strdup("");

    size_t len = strlen(content);
    if (len <= max_chars) {
        return agent_strdup(content);
    }

    /* head = 70%, tail = 20% */
    size_t head_len = (size_t)(max_chars * 0.7);
    size_t tail_len = (size_t)(max_chars * 0.2);

    /* Ensure we don't overlap */
    if (head_len + tail_len > max_chars) {
        tail_len = max_chars - head_len;
    }

    const char* marker = "\n\n... [truncated] ...\n\n";
    size_t marker_len = strlen(marker);

    size_t out_len = head_len + marker_len + tail_len + 1;
    char* result = (char*)malloc(out_len);
    if (!result) return NULL;

    /* Copy head */
    memcpy(result, content, head_len);

    /* Copy marker */
    memcpy(result + head_len, marker, marker_len);

    /* Copy tail (from end of content) */
    memcpy(result + head_len + marker_len, content + len - tail_len, tail_len);

    result[out_len - 1] = '\0';
    return result;
}

/* ------------------------------------------------------------------ */
/* strip_yaml_frontmatter                                              */
/* ------------------------------------------------------------------ */

char* strip_yaml_frontmatter(const char* content) {
    if (!content) return NULL;

    size_t len = strlen(content);

    /* Check if starts with "---" */
    if (len < 3 || content[0] != '-' || content[1] != '-' || content[2] != '-') {
        return agent_strdup(content);
    }

    /* Find closing "\n---" after position 3 */
    const char* end_pos = NULL;
    for (size_t i = 3; i + 3 < len; i++) {
        if (content[i] == '\n' && content[i+1] == '-' &&
            content[i+2] == '-' && content[i+3] == '-') {
            end_pos = content + i;
            break;
        }
    }

    if (!end_pos) {
        /* No closing --- found, return original */
        return agent_strdup(content);
    }

    /* Skip past closing --- and any trailing newlines */
    const char* body = end_pos + 4; /* skip "\n---" */
    while (*body == '\n') body++;

    if (*body == '\0') {
        /* Body is empty after stripping, return original */
        return agent_strdup(content);
    }

    return agent_strdup(body);
}

/* ------------------------------------------------------------------ */
/* scan_context_content                                                */
/* ------------------------------------------------------------------ */

/* Default threat-pattern strings that indicate prompt injection.
 * This is a minimal embedded set matching the Python threat-pattern core.
 * In production this would delegate to the shared threat_patterns library. */
static const char* THREAT_PATTERNS[] = {
    "ignore all previous instructions",
    "ignore previous instructions",
    "Ignore all previous instructions",
    "Ignore previous instructions",
    "system prompt",
    "System prompt",
    "you are now",
    "You are now",
    "your new role",
    "Your new role",
    "forget everything",
    "Forget everything",
    "new instructions",
    "New instructions",
    "override",
    "Override",
    NULL
};

int scan_context_content(const char* content, const char* filename) {
    (void)filename;
    if (!content) return 0;

    size_t len = strlen(content);
    if (len == 0) return 0;

    /* Scan for threat patterns */
    for (int i = 0; THREAT_PATTERNS[i] != NULL; i++) {
        if (strstr(content, THREAT_PATTERNS[i]) != NULL) {
            return 1; /* Pattern found */
        }
    }

    return 0; /* Clean */
}

/* ------------------------------------------------------------------ */
/* build_context_files_prompt                                          */
/* ------------------------------------------------------------------ */

char* build_context_files_prompt(const char** sections, size_t section_count) {
    if (!sections || section_count == 0) return NULL;

    const char* header = "# Project Context\n\n";
    size_t header_len = strlen(header);

    /* Calculate total length */
    size_t total = header_len;
    for (size_t i = 0; i < section_count; i++) {
        if (sections[i]) {
            total += strlen(sections[i]);
            total += 2; /* For "\n\n" separator */
        }
    }
    total += 1; /* Null terminator */

    char* result = (char*)malloc(total);
    if (!result) return NULL;

    size_t pos = 0;
    memcpy(result + pos, header, header_len);
    pos += header_len;

    for (size_t i = 0; i < section_count; i++) {
        if (sections[i]) {
            size_t slen = strlen(sections[i]);
            memcpy(result + pos, sections[i], slen);
            pos += slen;
            result[pos++] = '\n';
            result[pos++] = '\n';
        }
    }

    result[pos] = '\0';
    return result;
}
