/* Test runner implementation */
#include "test_runner.h"

test_entry _test_registry[MAX_TESTS];
int _test_count = 0;
int _test_passed = 0;
int _test_failed = 0;

void register_test(const char* name, test_fn fn) {
    if (_test_count < MAX_TESTS) {
        _test_registry[_test_count].name = name;
        _test_registry[_test_count].fn = fn;
        _test_count++;
    }
}

void reset_test_registry(void) {
    _test_count = 0;
    _test_passed = 0;
    _test_failed = 0;
}

int run_all_tests(void) {
    int total = _test_count;
    printf("=== Running %d tests ===\n\n", total);

    _test_passed = 0;
    int fails = 0;

    for (int i = 0; i < total; i++) {
        int before = _test_failed;
        printf("  TEST: %s ... ", _test_registry[i].name);
        fflush(stdout);

        _test_registry[i].fn();

        if (_test_failed == before) {
            printf("PASS\n");
            _test_passed++;
        } else {
            printf("FAIL\n");
            fails++;
        }
    }

    printf("\n=== Results: %d passed, %d failed, %d total ===\n",
           _test_passed, fails, total);
    return fails > 0 ? 1 : 0;
}
