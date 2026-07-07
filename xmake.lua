add_rules("mode.release", "mode.debug")

-- ========== Targets ==========

target("hello-c23")
    set_kind("binary")
    add_files("src/main.c")
    add_deps("yyjson", "xxhash", "mimalloc")
    set_languages("c23")
    set_warnings("all", "error")

-- yyjson: JSON library (static)
target("yyjson")
    set_kind("static")
    add_files("src/ext/yyjson/src/yyjson.c")
    add_includedirs("src/ext/yyjson/src", {public = true})
    set_languages("c99")
    set_warnings("all", "error")
    on_load(function(target)
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end)

-- xxhash: fast hash library (static)
target("xxhash")
    set_kind("static")
    add_files("src/ext/xxhash/xxhash.c")
    add_files("src/ext/xxhash/xxh_x86dispatch.c")
    add_includedirs("src/ext/xxhash", {public = true})
    set_languages("c99")

-- mimalloc: fast memory allocator (static)
target("mimalloc")
    set_kind("static")
    add_files("src/ext/mimalloc/src/static.c")
    add_includedirs("src/ext/mimalloc/include", {public = true})
    set_languages("c99")
    add_defines("MI_SHARED_LIB", "MI_XMALLOC=1", "MI_WIN_NOREDIRECT", "MI_SHARED_LIB_EXPORT", {public = true})
    on_load(function(target)
        target:add("defines", "_CRT_SECURE_NO_WARNINGS")
        if target:is_plat("windows") then
            target:add("syslinks", "advapi32", "bcrypt", {public = true})
        end
    end)
    set_warnings("all", "error")