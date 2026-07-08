-- Third-party dependencies under src/ext

-- yyjson: JSON library (static)
target("yyjson")
    set_kind("static")
    add_files("yyjson/src/yyjson.c")
    add_includedirs("yyjson/src", {public = true})
    set_languages("c11")
    set_warnings("all", "error")
    on_load(function(target)
        target:add("cxflags", "/utf-8", {tools = "cl"})
    end)

-- xxhash: fast hash library (static)
target("xxhash")
    set_kind("static")
    add_files("xxhash/xxhash.c")
    add_includedirs("xxhash", {public = true})
    set_languages("c11")

    -- xxh_x86dispatch.c implements runtime x86 SIMD dispatching and is only
    -- valid on x86/x86_64 targets. Apple Silicon and other non-x86 builds must
    -- not compile it.
    if is_arch("x86", "x64", "x86_64", "i386", "i686") then
        add_files("xxhash/xxh_x86dispatch.c")
    end

-- mimalloc: fast memory allocator (static)
target("mimalloc")
    set_kind("static")
    set_default(false)
    add_files("mimalloc/src/static.c")
    add_includedirs("mimalloc/include", {public = true})
    set_languages("c11")
    add_defines("MI_SHARED_LIB", "MI_XMALLOC=1", "MI_WIN_NOREDIRECT", "MI_SHARED_LIB_EXPORT", {public = true})
    on_load(function(target)
        target:add("defines", "_CRT_SECURE_NO_WARNINGS")
        if target:is_plat("windows") then
            target:add("syslinks", "advapi32", "bcrypt", {public = true})
        end
    end)
    set_warnings("all", "error")

-- glib: built from the meson-based submodule
target("glib")
    set_kind("static")
    set_default(false)
    set_targetdir("$(builddir)/glib")
    set_filename("glib-2.0.lib")

    on_load(function(target)
        import("core.project.config")
        local projectdir = os.projectdir()
        local buildroot = path.absolute(config.builddir(), projectdir)
        local installdir = path.join(buildroot, "glib/install")

        -- Public include layout mirrors a standard GLib installation.
        target:add("includedirs", path.join(installdir, "include/glib-2.0"), {public = true})
        target:add("includedirs", path.join(installdir, "lib/glib-2.0/include"), {public = true})

        -- Companion static archives are renamed to .lib in the target directory.
        target:add("linkdirs", path.join(buildroot, "glib"), {public = true})
        target:add("links", "glib-2.0", "pcre2-8", "intl", "ffi", "z", {public = true})

        -- Required Windows system libraries.
        target:add("syslinks", "advapi32", "ws2_32", "ole32", "shell32", "user32", {public = true})

        -- Static compilation macros.
        target:add("defines", "GLIB_STATIC_COMPILATION", "G_INTL_STATIC_COMPILATION", "PCRE2_STATIC", {public = true})
    end)

    on_build(function(target)
        import("core.project.config")
        local projectdir = os.projectdir()
        local buildroot = path.absolute(config.builddir(), projectdir)
        local srcdir = path.join(projectdir, "src/ext/glib")
        local mesonbuild = path.join(buildroot, "glib/meson")
        local installdir = path.join(buildroot, "glib/install")
        local targetdir = path.join(buildroot, "glib")

        -- Configure the meson build once.
        if not os.isfile(path.join(mesonbuild, "build.ninja")) then
            os.vrunv("meson", {"setup", mesonbuild, srcdir,
                "--vsenv",
                "--wrap-mode=forcefallback",
                "-Ddefault_library=static",
                "-Dtests=false",
                "-Dinstalled_tests=false",
                "-Dnls=disabled",
                "-Dintrospection=disabled",
                "-Dglib_assert=false",
                "-Dglib_checks=false",
                "-Dlibmount=disabled",
                "-Dselinux=disabled",
                "-Dxattr=false",
                "-Ddocumentation=false"})
        end

        -- Build and stage into a local prefix.
        os.vrunv("meson", {"compile", "-C", mesonbuild})
        os.vrunv("meson", {"install", "-C", mesonbuild, "--destdir", installdir, "--quiet"})

        -- Meson installs MSVC static archives with a .a suffix; rename them so
        -- xmake's MSVC linker resolves them as normal .lib files.
        local libs = {
            {"libglib-2.0.a", "glib-2.0.lib"},
            {"libpcre2-8.a",  "pcre2-8.lib"},
            {"libintl.a",     "intl.lib"},
            {"libffi.a",      "ffi.lib"},
            {"libz.a",        "z.lib"},
        }
        local install_libdir = path.join(installdir, "lib")
        for _, pair in ipairs(libs) do
            local srcfile = path.join(install_libdir, pair[1])
            local dstfile = path.join(targetdir, pair[2])
            if os.isfile(srcfile) then
                os.cp(srcfile, dstfile)
            end
        end
    end)
