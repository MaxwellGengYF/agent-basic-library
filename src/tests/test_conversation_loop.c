/* Test file for conversation_loop feature */
#include "test_runner.h"
#include "conversation_loop.h"
#include <stdlib.h>
#include <string.h>

TEST(test_sanitize_api_messages_basic) {
    const char* input = "[{\"role\":\"system\",\"content\":\"You are a helpful assistant.\"},{\"role\":\"user\",\"content\":\"Hello\"}]";
    char* result = sanitize_api_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "system");
    ASSERT_STR_CONTAINS(result, "user");
    ASSERT_STR_CONTAINS(result, "Hello");
    free(result);
    TEST_END();
}

TEST(test_sanitize_api_messages_invalid_role) {
    const char* input = "[{\"role\":\"invalid_role\",\"content\":\"Should be dropped\"},{\"role\":\"user\",\"content\":\"Hello\"}]";
    char* result = sanitize_api_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "Hello");
    ASSERT_NULL(strstr(result, "invalid_role"));
    free(result);
    TEST_END();
}

TEST(test_sanitize_api_messages_empty_tool_name) {
    const char* input = "[{\"role\":\"assistant\",\"content\":\"Let me check\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"\",\"arguments\":\"{}\"}}]}]";
    char* result = sanitize_api_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "unknown_tool");
    free(result);
    TEST_END();
}

TEST(test_sanitize_api_messages_orphan_tool) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"tool\",\"tool_call_id\":\"nonexistent\",\"content\":\"Result\"}]";
    char* result = sanitize_api_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_NULL(strstr(result, "nonexistent"));
    free(result);
    TEST_END();
}

TEST(test_sanitize_api_messages_null) {
    char* result = sanitize_api_messages(NULL);
    ASSERT_NULL(result);
    TEST_END();
}

TEST(test_repair_message_sequence_consecutive_assistant) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"First\"},{\"role\":\"assistant\",\"content\":\"Second\"}]";
    char* result = repair_message_sequence(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "FirstSecond");
    free(result);
    TEST_END();
}

TEST(test_repair_message_sequence_consecutive_user) {
    const char* input = "[{\"role\":\"user\",\"content\":\"First\"},{\"role\":\"user\",\"content\":\"Second\"},{\"role\":\"assistant\",\"content\":\"Response\"}]";
    char* result = repair_message_sequence(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "FirstSecond");
    free(result);
    TEST_END();
}

TEST(test_repair_message_sequence_clean_passthrough) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"content\":\"Hello\"},{\"role\":\"user\",\"content\":\"How are you?\"}]";
    char* result = repair_message_sequence(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "Hi");
    ASSERT_STR_CONTAINS(result, "Hello");
    ASSERT_STR_CONTAINS(result, "How are you?");
    free(result);
    TEST_END();
}

TEST(test_sanitize_and_repair_messages) {
    const char* input = "[{\"role\":\"user\",\"content\":\"Hi\"},{\"role\":\"assistant\",\"tool_calls\":[{\"id\":\"call_1\",\"function\":{\"name\":\"\",\"arguments\":\"{}\"}}]}]";
    char* result = sanitize_and_repair_messages(input);
    ASSERT_NOT_NULL(result);
    ASSERT_STR_CONTAINS(result, "unknown_tool");
    free(result);
    TEST_END();
}

int run_conversation_loop_tests(void) {
    _reg_test_sanitize_api_messages_basic();
    _reg_test_sanitize_api_messages_invalid_role();
    _reg_test_sanitize_api_messages_empty_tool_name();
    _reg_test_sanitize_api_messages_orphan_tool();
    _reg_test_sanitize_api_messages_null();
    _reg_test_repair_message_sequence_consecutive_assistant();
    _reg_test_repair_message_sequence_consecutive_user();
    _reg_test_repair_message_sequence_clean_passthrough();
    _reg_test_sanitize_and_repair_messages();
    return run_all_tests();
}
