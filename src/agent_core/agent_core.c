/* Unified agent_core shared library helper.
 *
 * Provides a single cross-CRT memory-release entry point so Python FFI
 * consumers can free strings that were allocated by the native library.
 */

#include <stdlib.h>

#ifdef _WIN32
    #define AGENT_CORE_API __declspec(dllexport)
#else
    #define AGENT_CORE_API __attribute__((visibility("default")))
#endif

/* Release memory allocated by any agent_core API function.
 * Python callers MUST use this instead of libc.free() to ensure the same
 * C runtime/heap is used on platforms where it matters (notably Windows). */
AGENT_CORE_API void agent_free(void* ptr) {
    free(ptr);
}
