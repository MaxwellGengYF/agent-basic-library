-- Source-level targets
includes("ext/xmake.lua")

-- MSVC compatibility helper
local function enable_msvc_compat(target)
    if target:is_plat("windows") then
        target:add("defines", "_CRT_NONSTDC_NO_DEPRECATE", "_CRT_SECURE_NO_WARNINGS")
    end
end

-- Unified shared library exposing all feature APIs.
target("agent_core")
    set_kind("shared")
    set_basename("agent_core")
    add_files("agent_core/agent_core.c",
              "message_sanitization/*.c",
              "prompt_builder/*.c",
              "conversation_loop/*.c",
              "context_compressor/*.c",
              "text_ops/*.c")
    add_includedirs("include",
                    "message_sanitization/include",
                    "prompt_builder/include",
                    "conversation_loop/include",
                    "context_compressor/include",
                    "text_ops/include",
                    {public = true})
    add_defines("AGENT_CORE_EXPORT_DLL")
    add_deps("yyjson")
    set_languages("c11")
    set_warnings("all")
    on_load(function(target)
        enable_msvc_compat(target)
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end)
target_end()

-- Tests
includes("tests/xmake.lua")