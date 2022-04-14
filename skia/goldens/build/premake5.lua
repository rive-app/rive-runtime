workspace "rive"
configurations {"debug", "release"}

BASE_DIR = path.getabsolute("../../../build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

BASE_DIR = path.getabsolute("../../renderer/build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

project "goldens"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    toolset "clang"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"
    includedirs {
        "../../../include",
        "../../utils",
        "../../renderer/include",
        "../../dependencies/skia",
        "../../dependencies/skia/include/core",
        "../../dependencies/skia/include/effects",
        "../../dependencies/skia/include/gpu",
        "../../dependencies/skia/include/config",
        "/usr/local/include",
        "/usr/include",
    }

    if os.host() == 'macosx' then
        links {
            "Cocoa.framework",
            "CoreFoundation.framework",
--            "CoreMedia.framework",
--            "CoreServices.framework",
--            "IOKit.framework",
--            "OpenGL.framework",
--            "Security.framework",
            "bz2",
            "iconv",
            "lzma",
            "rive_skia_renderer",
            "rive",
            "skia",
        }
    elseif os.host() == "windows" then
        architecture "x64"
        links {
            "rive_skia_renderer",
            "rive",
            "skia.lib",
            "opengl32.lib"
        }
        defines {"_USE_MATH_DEFINES"}
        buildoptions {WINDOWS_CLANG_CL_SUPPRESSED_WARNINGS}
        staticruntime "on"  -- Match Skia's /MT flag for link compatibility
        runtime "Release"  -- Use /MT even in debug (/MTd is incompatible with Skia)
    else
        links {
            "m",
            "rive_skia_renderer",
            "rive",
            "skia",
            "dl",
            "pthread",
        }
    end

    libdirs {
        "../build/%{cfg.system}/bin/%{cfg.buildcfg}",
        "../../dependencies/skia/out/static",
        "../../renderer/build/%{cfg.system}/bin/%{cfg.buildcfg}",
        "/usr/local/lib",
        "/usr/lib",
    }

    files {
        "../../utils/rive_mgr.cpp",
        "../src/goldens.cpp",
        "../src/goldens_grid.cpp"
    }

    buildoptions {"-Wall", "-fno-rtti"}

    filter "configurations:debug"
    defines {"DEBUG"}
    symbols "On"

    filter "configurations:release"
    defines {"RELEASE"}
    defines {"NDEBUG"}
    optimize "On"


-- Clean Function --
newaction {
    trigger = "clean",
    description = "clean the build",
    execute = function()
        print("clean the build...")
        os.rmdir("./bin")
        os.rmdir("./obj")
        os.remove("Makefile")
        -- no wildcards in os.remove, so use shell
        os.execute("rm *.make")
        print("build cleaned")
    end
}
