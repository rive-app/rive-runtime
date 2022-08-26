-- require "lfs"
-- Clean Function --
newaction {
    trigger = "clean",
    description = "clean the build",
    execute = function()
        print("clean the build...")
        os.rmdir("build")
        os.remove("Makefile")
        -- no wildcards in os.remove, so use shell
        os.execute("rm *.make")
        print("build cleaned")
    end
}


WINDOWS_CLANG_CL_SUPPRESSED_WARNINGS = {
    "-Wno-c++98-compat",
    "-Wno-c++98-compat-pedantic",
    "-Wno-reserved-macro-identifier",
    "-Wno-newline-eof",
    "-Wno-old-style-cast",
    "-Wno-unused-parameter",
    "-Wno-float-equal",
    "-Wno-implicit-float-conversion",
    "-Wno-shadow",
    "-Wno-sign-conversion",
    "-Wno-inconsistent-missing-destructor-override",
    "-Wno-nested-anon-types",
    "-Wno-suggest-destructor-override",
    "-Wno-non-virtual-dtor",
    "-Wno-unknown-argument",
    "-Wno-gnu-anonymous-struct",
    "-Wno-extra-semi",
    "-Wno-cast-qual",
    "-Wno-ignored-qualifiers",
    "-Wno-double-promotion",
    "-Wno-tautological-unsigned-zero-compare",
    "-Wno-unreachable-code-break",
    "-Wno-global-constructors",
    "-Wno-switch-enum",
    "-Wno-shorten-64-to-32",
    "-Wno-missing-prototypes",
    "-Wno-implicit-int-conversion",
    "-Wno-unused-macros",
    "-Wno-deprecated-copy-with-user-provided-dtor",
    "-Wno-missing-variable-declarations",
    "-Wno-ctad-maybe-unsupported",
    "-Wno-vla-extension",
    "-Wno-float-conversion",
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-undef",
    "-Wno-documentation",
    "-Wno-documentation-pedantic",
    "-Wno-documentation-unknown-command",
    "-Wno-suggest-override",
    "-Wno-unused-exception-parameter",
    "-Wno-cast-align",
    "-Wno-deprecated-declarations",
    "-Wno-shadow-field",
    "-Wno-nonportable-system-include-path",
    "-Wno-reserved-identifier",
    "-Wno-thread-safety-negative",
    "-Wno-exit-time-destructors",
    "-Wno-unreachable-code",
    "-Wno-zero-as-null-pointer-constant",
    "-Wno-pedantic",
    "-Wno-sign-compare",
}


workspace "rive_tests"
configurations {"debug"}

project("tests")
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
toolset "clang"
targetdir "build/bin/%{cfg.buildcfg}"
objdir "build/obj/%{cfg.buildcfg}"

buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti"}

includedirs {"./include", "../../include"}

files {
    "../../src/**.cpp",   -- the Rive runtime source
    "../../test/**.cpp",  -- the tests
    "../../utils/**.cpp", -- no_op utils
}

defines {"TESTING", "ENABLE_QUERY_FLAT_VERTICES", "WITH_RIVE_TOOLS"}

filter "configurations:debug"
defines {"DEBUG"}
symbols "On"

filter "system:windows"
    architecture "x64"
    defines {"_USE_MATH_DEFINES"}
    buildoptions {WINDOWS_CLANG_CL_SUPPRESSED_WARNINGS}
