/* Test file for message_sanitization feature */
#include "test_runner.h"
#include "message_sanitization.h"
#include <stdlib.h>
#include <string.h>
#include <mimalloc.h>

TEST(test_sanitize_surrogates_basic) {
    /* JSON with surrogate characters */
    const char* input = "[{\"role\":\"user\",\"content\":\"hello\xed\xa0\x80world\"}]";
    char* result = sanitize_messages_surrogates(input);
    ASSERT_NOT_NULL(result);
    /* The surrogate should be replaced with U+FFFD (EF BF BD) */
    ASSERT_STR_CONTAINS(result, "\xef\xbf\xbd");
    mi_free(result);
    TEST_END();
}

TEST(test_sanitize_surrogates_clean) {
    /* Clean JSON should pass through */
    const char* input = "[{\"role\":\"user\",\"content\":\"hello world\"}]";
    char* result = sanitize_messages_surrogates(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "hello world");
    mi_free(result);
    TEST_END();
}

TEST(test_sanitize_surrogates_null) {
    char* result = sanitize_messages_surrogates(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_repair_tool_call_arguments_empty) {
    char* result = repair_tool_call_arguments("", "test_tool");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "{}");
    mi_free(result);
    TEST_END();
}

TEST(test_repair_tool_call_arguments_none) {
    char* result = repair_tool_call_arguments("None", "test_tool");
    ASSERT_NOT_NULL(result);
    ASSERT_EQ_STR(result, "{}");
    mi_free(result);
    TEST_END();
}

TEST(test_repair_tool_call_arguments_valid) {
    char* result = repair_tool_call_arguments("{\"key\":\"value\"}", "test_tool");
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "key");
    ASSERT_STR_CONTAINS(result, "value");
    mi_free(result);
    TEST_END();
}

TEST(test_repair_tool_call_arguments_trailing_comma) {
    char* result = repair_tool_call_arguments("{\"key\":\"value\",}", "test_tool");
    ASSERT_NOT_NULL(result);
    /* Should parse successfully */
    ASSERT_STR_CONTAINS(result, "key");
    mi_free(result);
    TEST_END();
}

TEST(test_escape_invalid_chars_in_json_strings) {
    /* String with control character inside quotes */
    const char* input = "\"hello\x01world\"";
    char* result = escape_invalid_chars_in_json_strings(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "\\u0001");
    mi_free(result);
    TEST_END();
}

TEST(test_strip_images_from_messages) {
    /* Message with image_url */
    const char* input = "[{\"role\":\"user\",\"content\":[{\"type\":\"text\",\"text\":\"hello\"},{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,abc\"}}]}]";
    char* result = strip_images_from_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "hello");
    ASSERT_NULL(strstr(result, "image_url"));
    mi_free(result);
    TEST_END();
}

TEST(test_strip_images_tool_message) {
    /* Tool message with only image content should get placeholder */
    const char* input = "[{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"test\",\"arguments\":\"{}\"}}]},{\"role\":\"tool\",\"tool_call_id\":\"call_1\",\"content\":[{\"type\":\"image_url\",\"image_url\":{\"url\":\"data:image/png;base64,abc\"}}]}]";
    char* result = strip_images_from_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "image content removed");
    mi_free(result);
    TEST_END();
}

int run_message_sanitization_tests(void) {
    _reg_test_sanitize_surrogates_basic();
    _reg_test_sanitize_surrogates_clean();
    _reg_test_sanitize_surrogates_null();
    _reg_test_repair_tool_call_arguments_empty();
    _reg_test_repair_tool_call_arguments_none();
    _reg_test_repair_tool_call_arguments_valid();
    _reg_test_repair_tool_call_arguments_trailing_comma();
    _reg_test_escape_invalid_chars_in_json_strings();
    _reg_test_strip_images_from_messages();
    _reg_test_strip_images_tool_message();
    return run_all_tests();
}
