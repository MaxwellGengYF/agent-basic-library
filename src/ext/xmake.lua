-- Third-party dependencies under src/ext

-- yyjson: JSON library (static)
target("yyjson")
    set_kind("static")
    add_files("yyjson/src/yyjson.c")
    add_includedirs("yyjson/src", {public = true})
    set_languages("c11")
    set_warnings("all")
    on_load(function(target)
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end)
target_end()

-- mimalloc: fast general-purpose allocator (static, object-level)
target("mimalloc")
    set_kind("object")
    add_files("mimalloc/src/static.c")
    add_includedirs("mimalloc/include", {public = true})
    add_defines("MI_SHARED_LIB", "MI_XMALLOC=1", "MI_WIN_NOREDIRECT", "MI_SHARED_LIB_EXPORT", {public = true})
    set_languages("c11")
    set_warnings("all")
    on_load(function(target)
        if target:is_plat("windows") then
            target:add("syslinks", "advapi32", "bcrypt", {public = true})
            target:add("defines", "_CRT_SECURE_NO_WARNINGS")
        elseif target:is_plat("linux") then
            target:add("syslinks", "pthread", "atomic", {public = true})
            target:add("defines", "MI_NO_THP")
        else
            target:add("syslinks", "pthread", {public = true})
        end
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end)
