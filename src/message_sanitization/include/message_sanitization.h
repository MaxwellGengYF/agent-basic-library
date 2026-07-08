/* Message sanitization - C port of Hermes Agent agent/message_sanitization.py
 *
 * Pure functions for repairing surrogate characters, malformed JSON,
 * control characters, non-ASCII content, and stripping image blocks.
 */
#ifndef MESSAGE_SANITIZATION_H
#define MESSAGE_SANITIZATION_H

#include "agent_config.h"

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Replace surrogate code points (U+D800–U+DFFF -> U+FFFD) in all string
 * fields of a JSON message array. Returns repaired JSON (caller must free)
 * or NULL on error. Returns copy if no surrogates found. */
AGENT_MESSAGE_SANITIZATION_API char* sanitize_messages_surrogates(const char* messages_json);

/* Repair malformed JSON tool-call arguments: trailing commas, unclosed
 * structures, unescaped control characters. Returns repaired arguments
 * string, "{}" if unrepairable, or NULL on error. Caller must free. */
AGENT_MESSAGE_SANITIZATION_API char* repair_tool_call_arguments(const char* raw_args, const char* tool_name);

/* Escape unescaped control characters (0x00-0x1F) inside JSON string values.
 * Returns new string (caller must free) or NULL on error. */
AGENT_MESSAGE_SANITIZATION_API char* escape_invalid_chars_in_json_strings(const char* raw);

/* Replace non-ASCII characters in message content (last-resort mode for
 * ASCII-only encodings). Returns repaired JSON (caller must free). */
AGENT_MESSAGE_SANITIZATION_API char* sanitize_messages_non_ascii(const char* messages_json);

/* Strip image_url/image/input_image content parts from all messages.
 * Returns repaired JSON (caller must free). Tool-role messages have
 * content replaced with placeholder text. */
AGENT_MESSAGE_SANITIZATION_API char* strip_images_from_messages(const char* messages_json);

#ifdef __cplusplus
}
#endif

#endif /* MESSAGE_SANITIZATION_H */
