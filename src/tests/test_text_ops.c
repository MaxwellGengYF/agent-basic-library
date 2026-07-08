/* Test file for text_ops feature */
#include "test_runner.h"
#include "text_ops.h"
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* strip_think_blocks tests                                           */
/* ------------------------------------------------------------------ */

TEST(test_strip_think_blocks_null) {
    char* result = strip_think_blocks(NULL);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_empty) {
    char* result = strip_think_blocks("");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_no_tags) {
    char* result = strip_think_blocks("Hello, this is normal content.");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Hello, this is normal content.");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_paired_thinking) {
    /* Triple-backtick thinking code block */
    char* result = strip_think_blocks("Before \x60\x60\x60thinking content here \x60\x60\x60response after.");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Before  after.");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_paired_think) {
    char* result = strip_think_blocks("Hello <thinking>deep thoughts</thinking> world");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Hello  world");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_paired_reasoning) {
    char* result = strip_think_blocks("Before <reasoning>step by step</reasoning> after");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Before  after");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_case_insensitive) {
    char* result = strip_think_blocks("A <THINKING>UPPER CASE</THINKING> B");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "A  B");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_tool_call_pair) {
    char* result = strip_think_blocks("before <tool_call>some call</tool_call> after");
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "before ");
    ASSERT_STR_CONTAINS(result, " after");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_unterminated_reasoning) {
    const char* input = "Some content\n<reasoning>This never closes";
    char* result = strip_think_blocks(input);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Some content\n");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_stray_orphan_close) {
    char* result = strip_think_blocks("content </thinking> more");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "content more");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_stray_toolcall_close) {
    char* result = strip_think_blocks("hello </tool_call> world");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "hello world");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_function_block) {
    char* result = strip_think_blocks("Some text.\n<function name=\"get_weather\">content</function>\nMore text");
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "Some text.");
    ASSERT_STR_CONTAINS(result, "More text");
    ASSERT_STR_CONTAINS(result, "Some text.\n\nMore text");
    free(result);
    TEST_END();
}

TEST(test_strip_think_blocks_prose_function_safe) {
    char* result = strip_think_blocks("Use <function> in JavaScript is fine");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Use <function> in JavaScript is fine");
    free(result);
    TEST_END();
}

/* ------------------------------------------------------------------ */
/* canonical_json_sort tests                                          */
/* ------------------------------------------------------------------ */

TEST(test_canonical_json_sort_null) {
    char* result = canonical_json_sort(NULL);
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_empty) {
    char* result = canonical_json_sort("");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_already_sorted) {
    char* result = canonical_json_sort("{\"a\":1,\"b\":2}");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "{\"a\":1,\"b\":2}");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_reorder) {
    char* result = canonical_json_sort("{\"z\":1,\"a\":2,\"m\":3}");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "{\"a\":2,\"m\":3,\"z\":1}");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_nested) {
    char* result = canonical_json_sort("{\"z\":{\"b\":1,\"a\":2},\"a\":3}");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "{\"a\":3,\"z\":{\"a\":2,\"b\":1}}");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_not_json) {
    char* result = canonical_json_sort("not json at all");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "not json at all");
    free(result);
    TEST_END();
}

TEST(test_canonical_json_sort_array) {
    char* result = canonical_json_sort("[{\"b\":1,\"a\":2},{\"d\":3,\"c\":4}]");
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "\"a\":2");
    ASSERT_STR_CONTAINS(result, "\"b\":1");
    free(result);
    TEST_END();
}

/* ------------------------------------------------------------------ */
/* sanitize_surrogates_str tests                                      */
/* ------------------------------------------------------------------ */

TEST(test_sanitize_surrogates_str_null) {
    char* result = sanitize_surrogates_str(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_sanitize_surrogates_str_empty) {
    char* result = sanitize_surrogates_str("");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_sanitize_surrogates_str_clean) {
    char* result = sanitize_surrogates_str("Hello, world!");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Hello, world!");
    free(result);
    TEST_END();
}

TEST(test_sanitize_surrogates_str_with_surrogates) {
    char input[] = "data \xED\xA0\x80 from clipboard";
    char* result = sanitize_surrogates_str(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "data ");
    ASSERT_STR_CONTAINS(result, " from clipboard");
    const char* expected_marker = "\xEF\xBF\xBD";
    ASSERT_STR_CONTAINS(result, expected_marker);
    free(result);
    TEST_END();
}

/* ------------------------------------------------------------------ */
/* strip_non_ascii_str tests                                          */
/* ------------------------------------------------------------------ */

TEST(test_strip_non_ascii_str_null) {
    char* result = strip_non_ascii_str(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_strip_non_ascii_str_empty) {
    char* result = strip_non_ascii_str("");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

TEST(test_strip_non_ascii_str_ascii_only) {
    char* result = strip_non_ascii_str("Hello, world!");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Hello, world!");
    free(result);
    TEST_END();
}

TEST(test_strip_non_ascii_str_with_unicode) {
    char* result = strip_non_ascii_str("Hello, w\xC3\xB6rld!");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "Hello, wrld!");
    free(result);
    TEST_END();
}

TEST(test_strip_non_ascii_str_all_unicode) {
    char* result = strip_non_ascii_str("\xE4\xB8\xAD\xE6\x96\x87");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "");
    free(result);
    TEST_END();
}

/* ------------------------------------------------------------------ */
/* estimate_tokens_rough tests                                        */
/* ------------------------------------------------------------------ */

TEST(test_estimate_tokens_rough_null) {
    size_t result = estimate_tokens_rough(NULL);
    ASSERT_EQ_INT(result, 0);
    TEST_END();
}

TEST(test_estimate_tokens_rough_empty) {
    size_t result = estimate_tokens_rough("");
    ASSERT_EQ_INT(result, 0);
    TEST_END();
}

TEST(test_estimate_tokens_rough_short) {
    size_t result = estimate_tokens_rough("ab");
    ASSERT_EQ_INT(result, 1);
    TEST_END();
}

TEST(test_estimate_tokens_rough_four_chars) {
    size_t result = estimate_tokens_rough("abcd");
    ASSERT_EQ_INT(result, 1);
    TEST_END();
}

TEST(test_estimate_tokens_rough_five_chars) {
    size_t result = estimate_tokens_rough("abcde");
    ASSERT_EQ_INT(result, 2);
    TEST_END();
}

TEST(test_estimate_tokens_rough_unicode_codepoints) {
    size_t result = estimate_tokens_rough("\xE4\xB8\xAD\xE6\x96\x87");
    ASSERT_EQ_INT(result, 1);
    TEST_END();
}

/* ------------------------------------------------------------------ */
/* Runner                                                              */
/* ------------------------------------------------------------------ */

int run_text_ops_tests(void) {
    _reg_test_strip_think_blocks_null();
    _reg_test_strip_think_blocks_empty();
    _reg_test_strip_think_blocks_no_tags();
    _reg_test_strip_think_blocks_paired_thinking();
    _reg_test_strip_think_blocks_paired_think();
    _reg_test_strip_think_blocks_paired_reasoning();
    _reg_test_strip_think_blocks_case_insensitive();
    _reg_test_strip_think_blocks_tool_call_pair();
    _reg_test_strip_think_blocks_unterminated_reasoning();
    _reg_test_strip_think_blocks_stray_orphan_close();
    _reg_test_strip_think_blocks_stray_toolcall_close();
    _reg_test_strip_think_blocks_function_block();
    _reg_test_strip_think_blocks_prose_function_safe();
    _reg_test_canonical_json_sort_null();
    _reg_test_canonical_json_sort_empty();
    _reg_test_canonical_json_sort_already_sorted();
    _reg_test_canonical_json_sort_reorder();
    _reg_test_canonical_json_sort_nested();
    _reg_test_canonical_json_sort_not_json();
    _reg_test_canonical_json_sort_array();
    _reg_test_sanitize_surrogates_str_null();
    _reg_test_sanitize_surrogates_str_empty();
    _reg_test_sanitize_surrogates_str_clean();
    _reg_test_sanitize_surrogates_str_with_surrogates();
    _reg_test_strip_non_ascii_str_null();
    _reg_test_strip_non_ascii_str_empty();
    _reg_test_strip_non_ascii_str_ascii_only();
    _reg_test_strip_non_ascii_str_with_unicode();
    _reg_test_strip_non_ascii_str_all_unicode();
    _reg_test_estimate_tokens_rough_null();
    _reg_test_estimate_tokens_rough_empty();
    _reg_test_estimate_tokens_rough_short();
    _reg_test_estimate_tokens_rough_four_chars();
    _reg_test_estimate_tokens_rough_five_chars();
    _reg_test_estimate_tokens_rough_unicode_codepoints();
    return run_all_tests();
}