workspace "rive"
    configurations {"debug", "release"}

project "rive"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {"../include"}

    files {"../src/**.cpp"}

    buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti", "-Werror=format"}

    filter "system:windows"
        defines {"_USE_MATH_DEFINES"}

    filter {"system:ios", "options:variant=system" }
        buildoptions {"-mios-version-min=10.0 -fembed-bitcode -arch armv7 -arch arm64 -arch arm64e -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
    
    filter {"system:ios", "options:variant=emulator" }
        buildoptions {"-arch x86_64"}
        targetdir "%{cfg.system}_sim/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}_sim/obj/%{cfg.buildcfg}"

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