/* Test file for context_compressor feature */
#include "test_runner.h"
#include "context_compressor.h"
#include <stdlib.h>
#include <string.h>

TEST(test_sanitize_tool_pairs_basic) {
    const char* input = "[{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"test\",\"arguments\":\"{}\"}}]},{\"role\":\"tool\",\"tool_call_id\":\"call_1\",\"content\":\"Result\"}]";
    char* result = sanitize_tool_pairs(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "call_1");
    ASSERT_STR_CONTAINS(result, "Result");
    free(result);
    TEST_END();
}

TEST(test_sanitize_tool_pairs_orphan_dropped) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"tool\",\"tool_call_id\":\"orphan_id\",\"content\":\"Orphan result\"}]";
    char* result = sanitize_tool_pairs(input);
    ASSERT_NOT_NULL(result);
    ASSERT_NULL(strstr(result, "orphan_id"));
    free(result);
    TEST_END();
}

TEST(test_sanitize_tool_pairs_missing_stub) {
    const char* input = "[{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"test\",\"arguments\":\"{}\"}}]}]";
    char* result = sanitize_tool_pairs(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "Result unavailable");
    free(result);
    TEST_END();
}

TEST(test_sanitize_tool_pairs_null) {
    char* result = sanitize_tool_pairs(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_find_tail_cut_by_tokens_basic) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello\"},{\"role\":\"user\",\"content\":\"Question?\"},{\"role\":\"assistant\",\"content\":\"Answer\"}]";
    size_t tail_start = 999;
    size_t tail_tokens = 999;
    find_tail_cut_by_tokens(input, 1, 1000, &tail_start, &tail_tokens);
    ASSERT_TRUE(tail_start <= 1);
    ASSERT_TRUE(tail_tokens > 0);
    TEST_END();
}

TEST(test_find_tail_cut_by_tokens_small_budget) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello there! How can I help you today?\"},{\"role\":\"user\",\"content\":\"I have a long question\"}]";
    size_t tail_start = 999;
    size_t tail_tokens = 999;
    find_tail_cut_by_tokens(input, 0, 5, &tail_start, &tail_tokens);
    ASSERT_TRUE(tail_tokens > 0); /* At least the last message is kept */
    TEST_END();
}

TEST(test_build_static_fallback_summary_basic) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello\"},{\"role\":\"user\",\"content\":\"Question?\"},{\"role\":\"assistant\",\"content\":\"Answer\"}]";
    char* result = build_static_fallback_summary(input, 2);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "pruned");
    free(result);
    TEST_END();
}

TEST(test_build_static_fallback_summary_no_prune) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello\"}]";
    char* result = build_static_fallback_summary(input, 0);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "0 messages");
    free(result);
    TEST_END();
}

TEST(test_build_static_fallback_summary_null) {
    char* result = build_static_fallback_summary(NULL, 0);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_prune_old_tool_results_basic) {
    const char* input = "[{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"test\",\"arguments\":\"{}\"}}]},{\"role\":\"tool\",\"tool_call_id\":\"call_1\",\"content\":\"A very long tool result that should be replaced with a placeholder since it exceeds the character threshold for pruning\"}]";
    char* result = prune_old_tool_results(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "Old tool output cleared");
    free(result);
    TEST_END();
}

TEST(test_prune_old_tool_results_null) {
    char* result = prune_old_tool_results(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

int run_context_compressor_tests(void) {
    _reg_test_sanitize_tool_pairs_basic();
    _reg_test_sanitize_tool_pairs_orphan_dropped();
    _reg_test_sanitize_tool_pairs_missing_stub();
    _reg_test_sanitize_tool_pairs_null();
    _reg_test_find_tail_cut_by_tokens_basic();
    _reg_test_find_tail_cut_by_tokens_small_budget();
    _reg_test_build_static_fallback_summary_basic();
    _reg_test_build_static_fallback_summary_no_prune();
    _reg_test_build_static_fallback_summary_null();
    _reg_test_prune_old_tool_results_basic();
    _reg_test_prune_old_tool_results_null();
    return run_all_tests();
}
