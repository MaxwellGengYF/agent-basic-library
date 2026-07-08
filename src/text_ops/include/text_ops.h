/* Text operations -- C port of Hermes Agent scalar text transforms
 *
 * String-in/string-out functions for the hot path: think-block stripping,
 * canonical JSON key-sorting, surrogate sanitization, ASCII stripping,
 * and rough token estimation.
 *
 * Every string-returning function allocates with malloc(); the caller
 * MUST free the result via agent_free() (Python FFI) or free() (C).
 */
#ifndef TEXT_OPS_H
#define TEXT_OPS_H

#include "agent_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Remove reasoning/thinking blocks and stray tool-call XML from content.
 *
 * Implements the same fixed-tag scanner passes as the Python regex suite:
 *   1. Closed <think|thinking|reasoning|thought|REASONING_SCRATCHPAD> pairs.
 *   2. Closed <tool_call|tool_calls|tool_result|function_call|function_calls>
 *      pairs (no attribute gating).
 *   3. <function name="...">...</function> at block boundaries only.
 *   4. Unterminated open reasoning tags at start-of-line.
 *   5. Stray orphan open/close reasoning tags.
 *   6. Stray tool-call closers.
 *
 * Returns the stripped string (caller must free), or NULL on allocation
 * failure. Returns an empty-string allocation for empty/null input. */
AGENT_TEXT_OPS_API char* strip_think_blocks(const char* content);

/* Recursively sort JSON object keys (canonical form for dedup).
 *
 * Parses input JSON with yyjson, recursively sorts all object keys,
 * and re-serialises.  Returns the key-sorted JSON string (caller must
 * free).  On parse failure, returns a copy of the original string
 * (graceful fallback).  Returns NULL on allocation failure. */
AGENT_TEXT_OPS_API char* canonical_json_sort(const char* json_str);

/* Replace WTF-8 lone-surrogate bytes (0xED 0xA0-0xBF ..) with U+FFFD
 * (EF BF BD) in a UTF-8 byte string.
 *
 * Scans the raw bytes for the 3-byte surrogate sequences
 * (0xED [0xA0-0xBF] [0x80-0xBF]) and replaces each with the
 * 3-byte U+FFFD sequence.  This is safe on valid UTF-8 because
 * surrogates are not valid UTF-8; any occurrence is necessarily
 * a lone-surrogate from WTF-8 or corrupted data.
 *
 * Returns the sanitised string (caller must free) or NULL on error.
 * If the input is NULL, returns NULL. */
AGENT_TEXT_OPS_API char* sanitize_surrogates_str(const char* text);

/* Remove all non-ASCII bytes from a UTF-8 string.
 *
 * Keeps only bytes in the range 0x00-0x7F (ASCII).  Multi-byte UTF-8
 * sequences (0x80-0xFF in continuation or leading bytes) are removed
 * entirely.  This is a lossy last-resort recovery for ASCII-only
 * environments.
 *
 * Returns the ASCII-only string (caller must free) or NULL on error. */
AGENT_TEXT_OPS_API char* strip_non_ascii_str(const char* text);

/* Rough token estimate over Unicode code points: (code_points + 3) / 4.
 *
 * Counts code points (not bytes) in the input, using ceiling division
 * so short texts (1-3 code points) never estimate as 0.  Matches the
 * Python ``estimate_tokens_rough``: ``(len(text) + 3) // 4`` where
 * ``len()`` is the Python string length = number of Unicode code points.
 *
 * Returns the estimated token count.  For NULL or empty input, returns 0. */
AGENT_TEXT_OPS_API size_t estimate_tokens_rough(const char* text);

#ifdef __cplusplus
}
#endif

#endif /* TEXT_OPS_H */