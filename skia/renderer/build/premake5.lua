workspace "rive"
configurations {"debug", "release"}

project "rive_skia_renderer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    toolset "clang"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {"../include", "../../../include", "../../dependencies/skia", "../../dependencies/skia/include/core",
             "../../dependencies/skia/include/effects", "../../dependencies/skia/include/gpu",
             "../../dependencies/skia/include/config"}

    if os.host() == "macosx" then
        links {"Cocoa.framework", "rive", "skia"}
    else
        links {"rive", "skia"}
    end

    libdirs {"../../../build/%{cfg.system}/bin/%{cfg.buildcfg}", "../../dependencies/skia/out/Static"}

    files {"../src/**.cpp"}

    buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti", "-Werror=format"}

    filter {"system:ios", "options:variant=system" }
        buildoptions {"-arch armv7 -arch arm64 -arch arm64e -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
    
    filter {"system:ios", "options:variant=emulator" }
        buildoptions {"-arch x86_64"}
        targetdir "%{cfg.system}_sim/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}_sim/obj/%{cfg.buildcfg}"

    filter "configurations:debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:release"
        defines {"RELEASE"}
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