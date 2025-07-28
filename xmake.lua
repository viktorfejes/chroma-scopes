add_rules("mode.debug", "mode.release")

set_languages("c99")
set_toolchains("clang")
set_warnings("all", "error", "extra", "pedantic")

if is_mode("debug") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
end

target("chroma-scopes")
    set_kind("binary")
    add_includedirs("libs/stb")
    add_files("src/*.c")
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "uuid", "dxguid", "shcore", "winmm")
    -- add_cxflags("-fno-sanitize=vptr")

    add_defines("WINVER=0x0A00", "_WIN32_WINNT=0x0A00")
    if is_mode("debug") then
        add_defines("_DEBUG")
    end

    set_rundir(os.projectdir())
