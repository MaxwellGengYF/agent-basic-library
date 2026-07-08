/* Context compressor  --  C port of Hermes Agent agent/context_compressor.py
 *
 * Deterministic bookkeeping helpers: tool-pair sanitization,
 * tail-cut token walk, static fallback summary, tool result pruning.
 */
#ifndef CONTEXT_COMPRESSOR_H
#define CONTEXT_COMPRESSOR_H

#include "agent_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sanitize tool pairs: collect tool_call_ids from assistant messages,
 * drop orphan tool results, inject stub results for missing calls.
 * Returns repaired JSON string (caller must free) or NULL on error. */
AGENT_CONTEXT_COMPRESSOR_API char* sanitize_tool_pairs(const char* messages_json);

/* Find the tail-cut index by walking backward from end, accumulating token budget.
 * Param head_end: index of last message that must be kept (exclusive cut).
 * Param token_budget: max tokens the tail can occupy.
 * Output out_tail_start: index of first message in tail.
 * Output out_tail_tokens: actual tokens in tail (approximate). */
AGENT_CONTEXT_COMPRESSOR_API void find_tail_cut_by_tokens(const char* messages_json,
                             size_t head_end,
                             size_t token_budget,
                             size_t* out_tail_start,
                             size_t* out_tail_tokens);

/* Build a static fallback summary string from pruned/removed messages.
 * Returns a deterministic placeholder string (caller must free) or NULL. */
AGENT_CONTEXT_COMPRESSOR_API char* build_static_fallback_summary(const char* messages_json,
                                    size_t tail_start);

/* Prune old tool results: replace tool-result messages whose tool_call_id
 * is no longer referenced by any remaining assistant message.
 * Returns pruned JSON string (caller must free) or NULL. */
AGENT_CONTEXT_COMPRESSOR_API char* prune_old_tool_results(const char* messages_json);

/* Shrink long string leaves inside a tool-call arguments JSON blob while
 * preserving JSON validity.
 *
 * Parses the arguments JSON, walks the structure, and truncates every
 * string leaf longer than ``head_chars`` by keeping the first
 * ``head_chars`` characters and appending ``..."[truncated]"``.
 * Non-string values are preserved intact. If the input is not valid
 * JSON, returns a copy of the original string (graceful fallback).
 * Returns NULL on allocation failure. */
AGENT_CONTEXT_COMPRESSOR_API char* truncate_tool_call_args_json(const char* args, size_t head_chars);

#ifdef __cplusplus
}
#endif

#endif /* CONTEXT_COMPRESSOR_H */
