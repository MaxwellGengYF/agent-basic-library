/* Portable compatibility helpers for agent-basic-library.
 *
 * These are internal utilities intended for use in .c files only.
 * They are placed in a separate header so public API headers stay clean.
 */
#ifndef AGENT_COMPAT_H
#define AGENT_COMPAT_H

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Portable strdup replacement.
 *
 * strdup() is POSIX and only became ISO C in C23. The project builds as C11,
 * and MSVC deprecates the POSIX name. Using a small inline helper avoids
 * relying on platform-specific feature-test macros or deprecation suppressions.
 */
static inline char* agent_strdup(const char* s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char* p = (char*)malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}

#ifdef __cplusplus
}
#endif

#endif /* AGENT_COMPAT_H */
