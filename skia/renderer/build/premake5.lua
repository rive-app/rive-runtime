workspace "rive"
configurations {"debug", "release"}

SKIA_DIR = os.getenv('SKIA_DIR') or 'skia'
dependencies = os.getenv('DEPENDENCIES')

if dependencies ~= nil then
    SKIA_DIR = dependencies .. '/skia'
else
    SKIA_DIR = "../../dependencies/" .. SKIA_DIR
end

project "rive_skia_renderer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    toolset "clang"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {
        "../../../../../third_party/externals/harfbuzz/src",
        "../include",
        "../../../include"
    }

    if os.host() == "macosx" then
        links {"Cocoa.framework", "rive", "skia"}
    elseif os.host() == "windows" then
        architecture "x64"
        links {
            "rive",
            "skia.lib",
            "opengl32.lib"
        }
        defines {"_USE_MATH_DEFINES"}
        staticruntime "on"  -- Match Skia's /MT flag for link compatibility
        runtime "Release"  -- Use /MT even in debug (/MTd is incompatible with Skia)
    else
        links {"rive", "skia"}
    end

    libdirs {"../../../build/%{cfg.system}/bin/%{cfg.buildcfg}"}

    files {
        "../src/**.cpp"
    }

    buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti", "-Werror=format"}

    filter {"system:macosx" }
        includedirs {SKIA_DIR}
        libdirs {SKIA_DIR.. "/out/static"}

    filter {"system:linux or windows" }
        includedirs {SKIA_DIR}
        libdirs {SKIA_DIR.. "/out/static"}

    filter {"system:ios" }
        includedirs {SKIA_DIR}
        libdirs {SKIA_DIR.. "/out/static"}

    filter {"system:ios", "options:variant=system" }
        buildoptions {"-mios-version-min=10.0 -fembed-bitcode -arch armv7 -arch arm64 -arch arm64e -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
    
    filter {"system:ios", "options:variant=emulator" }
        buildoptions {"-mios-version-min=10.0 -arch x86_64 -arch arm64 -arch i386 -isysroot " .. (os.getenv("IOS_SYSROOT") or "")}
        targetdir "%{cfg.system}_sim/bin/%{cfg.buildcfg}"
        objdir "%{cfg.system}_sim/obj/%{cfg.buildcfg}"

    -- Is there a way to pass 'arch' as a variable here?
    filter { "system:android" }
        includedirs {SKIA_DIR}
        
        filter { "system:android", "options:arch=x86" }
            targetdir "%{cfg.system}/x86/bin/%{cfg.buildcfg}"
            objdir "%{cfg.system}/x86/obj/%{cfg.buildcfg}"
            libdirs {SKIA_DIR.. "/out/x86"}

        filter { "system:android", "options:arch=x64" }
            targetdir "%{cfg.system}/x64/bin/%{cfg.buildcfg}"
            objdir "%{cfg.system}/x64/obj/%{cfg.buildcfg}"
            libdirs {SKIA_DIR.. "/out/x64"}

        filter { "system:android", "options:arch=arm" }
            targetdir "%{cfg.system}/arm/bin/%{cfg.buildcfg}"
            objdir "%{cfg.system}/arm/obj/%{cfg.buildcfg}"
            libdirs {SKIA_DIR.. "/out/arm"}

        filter { "system:android", "options:arch=arm64" }
            targetdir "%{cfg.system}/arm64/bin/%{cfg.buildcfg}"
            objdir "%{cfg.system}/arm64/obj/%{cfg.buildcfg}"
            libdirs {SKIA_DIR.. "/out/arm64"}

    filter { "configurations:release", "system:macosx" }
        buildoptions {"-flto=full"}

    filter { "configurations:release", "system:android" }
        buildoptions {"-flto=full"}

    filter { "configurations:release", "system:ios" }
        buildoptions {"-flto=full"}

    filter "configurations:debug"
        buildoptions {"-g"}
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:release"
        defines {"RELEASE", "NDEBUG"}
        optimize "On"

    filter {"options:with-text" }
        defines {"RIVE_TEXT"}
    
newoption {
    trigger = "with-text",
    description = "Enables text experiments"
}

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