/* Prompt builder  --  C port of Hermes Agent agent/prompt_builder.py
 *
 * Deterministic string-assembly helpers: content truncation,
 * YAML frontmatter stripping, content scanning, context assembly.
 */
#ifndef PROMPT_BUILDER_H
#define PROMPT_BUILDER_H

#include "agent_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Truncate content to fit within max_chars using head=70%, tail=20% split.
 * Returns the truncated string (caller must free) or a copy of original
 * if content fits within max_chars. Returns NULL on error. */
AGENT_PROMPT_BUILDER_API char* truncate_content(const char* content, const char* filename,
                       size_t max_chars, const char* read_path);

/* Strip YAML frontmatter (--- delimited) from content.
 * Returns body string (caller must free) or copy of original if no
 * frontmatter found. Returns NULL on error. */
AGENT_PROMPT_BUILDER_API char* strip_yaml_frontmatter(const char* content);

/* Scan context content for threat patterns (delegates to threat-pattern core).
 * Returns 0 if clean, non-zero if a pattern was found.
 * Param filename used for error reporting (may be NULL). */
AGENT_PROMPT_BUILDER_API int scan_context_content(const char* content, const char* filename);

/* Build the "Project Context" section from a list of section strings.
 * Returns assembled string (caller must free) or NULL on error. */
AGENT_PROMPT_BUILDER_API char* build_context_files_prompt(const char** sections, size_t section_count);

#ifdef __cplusplus
}
#endif

#endif /* PROMPT_BUILDER_H */
