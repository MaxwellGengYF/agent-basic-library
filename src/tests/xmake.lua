-- Test targets for all features

-- MSVC-specific defines (applied inside on_load).
local function apply_msvc_compat(target)
    if target:is_plat("windows") then
        target:add("defines", "_CRT_NONSTDC_NO_DEPRECATE", "_CRT_SECURE_NO_WARNINGS")
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end
end

target("test_runner")
    set_kind("static")
    add_files("test_runner.c")
    add_includedirs(".", {public = true})
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

-- Build rule: do not compile a single source file into two targets.
-- Each test suite is built once as an `object` target; the per-suite
-- binaries and the aggregated `test_all` binary both link those shared
-- objects. The suites expose `run_<suite>_tests()` instead of `main()`,
-- and the per-suite binaries supply their own `main()` via test_main.c.

-- Object targets: compile each test file once and share the objects with
-- both the per-suite binaries and the aggregated test_all binary.
target("test_message_sanitization_objs")
    set_kind("object")
    add_files("test_message_sanitization.c")
    add_deps("message_sanitization", "yyjson", "test_runner")
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_prompt_builder_objs")
    set_kind("object")
    add_files("test_prompt_builder.c")
    add_deps("prompt_builder", "test_runner")
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_conversation_loop_objs")
    set_kind("object")
    add_files("test_conversation_loop.c")
    add_deps("conversation_loop", "yyjson", "test_runner")
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_context_compressor_objs")
    set_kind("object")
    add_files("test_context_compressor.c")
    add_deps("context_compressor", "yyjson", "xxhash", "test_runner")
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

-- Per-suite binaries: link the shared objects with a generic main() wrapper.
target("test_message_sanitization")
    set_kind("binary")
    add_deps("test_message_sanitization_objs")
    add_files("test_main.c")
    add_defines('TEST_SUITE_RUNNER=run_message_sanitization_tests')
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_prompt_builder")
    set_kind("binary")
    add_deps("test_prompt_builder_objs")
    add_files("test_main.c")
    add_defines('TEST_SUITE_RUNNER=run_prompt_builder_tests')
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_conversation_loop")
    set_kind("binary")
    add_deps("test_conversation_loop_objs")
    add_files("test_main.c")
    add_defines('TEST_SUITE_RUNNER=run_conversation_loop_tests')
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

target("test_context_compressor")
    set_kind("binary")
    add_deps("test_context_compressor_objs")
    add_files("test_main.c")
    add_defines('TEST_SUITE_RUNNER=run_context_compressor_tests')
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

-- Aggregated test binary: links all shared test objects.
target("test_all")
    set_kind("binary")
    add_deps("test_message_sanitization_objs", "test_prompt_builder_objs",
             "test_conversation_loop_objs", "test_context_compressor_objs")
    add_files("test_all.c")
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        apply_msvc_compat(target)
    end)

add_tests("test_message_sanitization", {group = "unit"})
add_tests("test_prompt_builder",       {group = "unit"})
add_tests("test_conversation_loop",    {group = "unit"})
add_tests("test_context_compressor",   {group = "unit"})
add_tests("test_all",                  {group = "unit"})
