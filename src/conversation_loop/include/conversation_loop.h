/* Conversation loop pre-flight repair  --  C port of run_agent.py helpers
 *
 * Role allowlist filtering, empty tool-call name repair,
 * tool-call/tool-result orphan reconciliation, message sequence repair.
 */
#ifndef CONVERSATION_LOOP_H
#define CONVERSATION_LOOP_H

#include "agent_config.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Sanitize an API message list: role allowlist check, empty tool-call name
 * repair, orphan tool-result removal, missing-result stub injection.
 * Returns repaired JSON string (caller must free) or NULL on error. */
AGENT_CONVERSATION_LOOP_API char* sanitize_api_messages(const char* messages_json);

/* Repair message sequence: merge consecutive assistant turns (exempting
 * finish_reason=incomplete), merge consecutive user messages with string content.
 * Returns repaired JSON string (caller must free) or NULL on error. */
AGENT_CONVERSATION_LOOP_API char* repair_message_sequence(const char* messages_json);

/* Combined single-pass sanitize + repair. */
AGENT_CONVERSATION_LOOP_API char* sanitize_and_repair_messages(const char* messages_json);

#ifdef __cplusplus
}
#endif

#endif /* CONVERSATION_LOOP_H */
