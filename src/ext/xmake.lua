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
