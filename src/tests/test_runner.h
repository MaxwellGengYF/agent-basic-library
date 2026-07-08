/* Minimal C test runner framework - MSVC-compatible.
 * Provides TEST/TEST_END macros, assertions, and manual registration.
 */
#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* Maximum number of registered tests */
#define MAX_TESTS 256

/* Test function type */
typedef void (*test_fn)(void);

/* Test registration entry */
typedef struct {
    const char* name;
    test_fn fn;
} test_entry;

/* Global test registry */
extern test_entry _test_registry[MAX_TESTS];
extern int _test_count;
extern int _test_passed;
extern int _test_failed;

/* Register a test manually */
void register_test(const char* name, test_fn fn);

/* Reset the test registry so a new suite can be run in the same process. */
void reset_test_registry(void);

/* Macro to define a test function.
 * Usage: TEST(my_test) { ... ASSERT_TRUE(...); } TEST_END()
 * Must call register_test at the top of main(). */
#define TEST(name) \
    static void _testfn_##name(void); \
    void _reg_##name(void) { register_test(#name, _testfn_##name); } \
    static void _testfn_##name(void)

/* End a test block (no-op, for symmetry) */
#define TEST_END()

/* Assertions */
#define ASSERT_TRUE(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_TRUE(%s) failed\n", \
                __FILE__, __LINE__, #cond); \
        _test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(cond) ASSERT_TRUE(!(cond))

#define ASSERT_EQ_INT(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_EQ_INT(%s, %s) -- %d != %d\n", \
                __FILE__, __LINE__, #a, #b, (int)(a), (int)(b)); \
        _test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ_STR(a, b) do { \
    const char* _a = (a); \
    const char* _b = (b); \
    if ((_a == NULL && _b != NULL) || (_a != NULL && _b == NULL) || \
        (_a != NULL && _b != NULL && strcmp(_a, _b) != 0)) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_EQ_STR(%s, %s)\n", \
                __FILE__, __LINE__, #a, #b); \
        fprintf(stderr, "    left:  %s\n", _a ? _a : "(null)"); \
        fprintf(stderr, "    right: %s\n", _b ? _b : "(null)"); \
        _test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NULL(ptr) do { \
    if ((ptr) != NULL) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_NULL(%s) -- ptr is non-null\n", \
                __FILE__, __LINE__, #ptr); \
        _test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_NOT_NULL(ptr) do { \
    if ((ptr) == NULL) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_NOT_NULL(%s) -- ptr is null\n", \
                __FILE__, __LINE__, #ptr); \
        _test_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_CONTAINS(haystack, needle) do { \
    const char* _h = (haystack); \
    const char* _n = (needle); \
    if (_h == NULL || _n == NULL || strstr(_h, _n) == NULL) { \
        fprintf(stderr, "  FAIL [%s:%d]: ASSERT_STR_CONTAINS(%s, %s)\n", \
                __FILE__, __LINE__, #haystack, #needle); \
        fprintf(stderr, "    haystack: %s\n", _h ? _h : "(null)"); \
        fprintf(stderr, "    needle:   %s\n", _n ? _n : "(null)"); \
        _test_failed++; \
        return; \
    } \
} while(0)

/* Run all registered tests */
int run_all_tests(void);

#endif /* TEST_RUNNER_H */
