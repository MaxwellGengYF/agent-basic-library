/* Unified agent_core shared library helper.
 *
 * Provides a single cross-CRT memory-release entry point so Python FFI
 * consumers can free strings that were allocated by the native library.
 */

#include "agent_config.h"
#include <mimalloc.h>

/* Release memory allocated by any agent_core API function.
 * Python callers MUST use this instead of libc.free() to ensure the same
 * C runtime/heap is used on platforms where it matters (notably Windows). */
AGENT_CORE_API void agent_free(void* ptr) {
    mi_free(ptr);
}
