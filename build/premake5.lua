workspace "rive"
    configurations {"debug", "release"}

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

project "rive"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    toolset "clang"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {"../include"}

    files {"../src/**.cpp"}

    buildoptions {
        "-Wall", "-fno-exceptions",
        "-fno-rtti",
        "-Werror=format",
        "-Wimplicit-int-conversion",
        "-Werror=vla",
    }

    filter {"system:macosx" }
        buildoptions {"-flto=full"}

    filter {"system:ios" }
        buildoptions {"-flto=full"}

    filter "system:windows"
        architecture "x64"
        defines {"_USE_MATH_DEFINES"}
        buildoptions {WINDOWS_CLANG_CL_SUPPRESSED_WARNINGS}
        staticruntime "on"  -- Match Skia's /MT flag for link compatibility
        runtime "Release"  -- Use /MT even in debug (/MTd is incompatible with Skia)

    filter {"system:ios", "options:variant=system" }
        buildoptions {"-mios-version-min=10.0 -fembed-bitcode -arch armv7 -arch arm64 -arch arm64e -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
    
    filter {"system:ios", "options:variant=emulator" }
        buildoptions {"-mios-version-min=10.0 -arch arm64 -arch x86_64 -arch i386 -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
        targetdir "%{cfg.system}_sim/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}_sim/obj/%{cfg.buildcfg}"

    filter { "system:android", "configurations:release" }
        buildoptions {"-flto=full"}

    -- Is there a way to pass 'arch' as a variable here?
    filter { "system:android", "options:arch=x86" }
        targetdir "%{cfg.system}/x86/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}/x86/obj/%{cfg.buildcfg}"

    filter { "system:android", "options:arch=x64" }
        targetdir "%{cfg.system}/x64/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}/x64/obj/%{cfg.buildcfg}"

    filter { "system:android", "options:arch=arm" }
        targetdir "%{cfg.system}/arm/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}/arm/obj/%{cfg.buildcfg}"

    filter { "system:android", "options:arch=arm64" }
        targetdir "%{cfg.system}/arm64/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}/arm64/obj/%{cfg.buildcfg}"

    filter "configurations:debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:release"
        defines {"RELEASE"}
        defines {"NDEBUG"}
        optimize "On"

newoption {
    trigger = "variant",
    value = "type",
    description = "Choose a particular variant to build",
    allowed = {
        { "system",   "Builds the static library for the provided system" },
        { "emulator",  "Builds for an emulator/simulator for the provided system" }
    },
    default = "system"
}

newoption {
    trigger = "arch",
    value = "ABI",
    description = "The ABI with the right toolchain for this build, generally with Android",
    allowed = {
        { "x86" },
        { "x64" },
        { "arm" },
        { "arm64" }
    }

}
