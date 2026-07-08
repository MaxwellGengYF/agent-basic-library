/* Generic main() wrapper for individual test binaries.
 * Build with -DTEST_SUITE_RUNNER=run_<suite>_tests to select the suite. */
#include "test_runner.h"

#ifndef TEST_SUITE_RUNNER
#error TEST_SUITE_RUNNER must be defined to the test suite runner function name
#endif

int TEST_SUITE_RUNNER(void);

int main(void) {
    reset_test_registry();
    return TEST_SUITE_RUNNER();
}
