/* Aggregated test runner: executes every unit-test suite in one binary. */
#include "test_runner.h"
#include <stdio.h>
#include <stddef.h>

/* Each suite is implemented in its own object target; this binary links all
 * of them and runs them sequentially, resetting the registry between suites. */
int run_message_sanitization_tests(void);
int run_prompt_builder_tests(void);
int run_conversation_loop_tests(void);
int run_context_compressor_tests(void);
int run_text_ops_tests(void);


typedef struct {
    const char* name;
    int (*run)(void);
} test_suite;

static test_suite suites[] = {
    {"message_sanitization", run_message_sanitization_tests},
    {"prompt_builder",       run_prompt_builder_tests},
    {"conversation_loop",    run_conversation_loop_tests},
    {"context_compressor",   run_context_compressor_tests},
    {"text_ops",             run_text_ops_tests},
};

int main(void) {
    int failing_suites = 0;

    for (size_t i = 0; i < sizeof(suites) / sizeof(suites[0]); i++) {
        printf("\n>>> Running suite: %s\n", suites[i].name);
        reset_test_registry();
        if (suites[i].run() != 0) {
            failing_suites++;
        }
    }

    printf("\n=== All suites finished: %d suite(s) failed ===\n", failing_suites);
    return failing_suites > 0 ? 1 : 0;
}