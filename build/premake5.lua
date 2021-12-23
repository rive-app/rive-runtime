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

    filter {"system:macosx" }
        buildoptions {"-flto=full"}

    filter {"system:ios" }
        buildoptions {"-flto=full"}

    filter "system:windows"
        defines {"_USE_MATH_DEFINES"}

    filter {"system:ios", "options:variant=system" }
        buildoptions {"-mios-version-min=10.0 -fembed-bitcode -arch armv7 -arch arm64 -arch arm64e -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
    
    filter {"system:ios", "options:variant=emulator" }
        buildoptions {"-mios-version-min=10.0 -arch arm64 -arch x86_64 -arch i386 -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
        targetdir "%{cfg.system}_sim/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}_sim/obj/%{cfg.buildcfg}"

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