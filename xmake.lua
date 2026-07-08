add_rules("mode.release", "mode.debug")

-- AddressSanitizer is enabled automatically for debug builds.
-- set_policy("build.sanitizer.address", true)

-- Suppress MSVC deprecation warnings for POSIX names (strdup, etc.)
if is_plat("windows") then
    add_defines("_CRT_NONSTDC_NO_DEPRECATE", "_CRT_SECURE_NO_WARNINGS")
end

-- Delegate target definitions to the source tree.
includes("src/xmake.lua")
