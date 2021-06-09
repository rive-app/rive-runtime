workspace "rive"
configurations {"debug", "release"}

BASE_DIR = path.getabsolute("../../../build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

BASE_DIR = path.getabsolute("../../renderer/build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

project "rive_recorder"
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
targetdir "bin/%{cfg.buildcfg}"
objdir "obj/%{cfg.buildcfg}"
includedirs {
    "../include",
    "../../../include",
    "../../renderer/include",
    "../../dependencies/skia",
    "../../dependencies/skia/include/core",
    "../../dependencies/skia/include/effects",
    "../../dependencies/skia/include/gpu",
    "../../dependencies/skia/include/config",
    "../../dependencies/FFmpeg",
    "../../dependencies/x264/include",
    "../../dependencies/libzip/lib",
    "/usr/local/include",
    "/usr/include",
}

if os.host() == 'macosx' then 
    links {
        "AudioToolbox.framework",
        "AudioUnit.framework",
        "Cocoa.framework",
        "CoreFoundation.framework",
        "CoreMedia.framework",
        "CoreServices.framework",
        "CoreVideo.framework",
        "IOKit.framework",
        "Security.framework",
        "VideoToolbox.framework",
        "avcodec",
        "avformat",
        "avutil",
        "bz2",
        "iconv",
        "lzma",
        "rive_skia_renderer",
        "rive",
        "skia",
        "swresample",
        "swscale", -- lib av format 
        "x264",
        "z",  -- lib av format 
        "zip"
    }
else
    links {
        "avformat",
        "avfilter",
        "avcodec",
        "avutil",
        -- included in gc6?!
        -- "bz2",
        -- "iconv",
        -- "lzma",
        "m",
        "rive_skia_renderer",
        "rive",
        "skia",
        "swresample",
        "swscale",
        "x264",
        "z",
        "dl",
        "zip"
    }
end 

libdirs {
    "../../../build/bin/%{cfg.buildcfg}",
    "../../dependencies/FFmpeg/libavcodec",
    "../../dependencies/FFmpeg/libavformat",
    "../../dependencies/FFmpeg/libavfilter",
    "../../dependencies/FFmpeg/libavutil",
    "../../dependencies/FFmpeg/libswscale",
    "../../dependencies/FFmpeg/libswresample",
    "../../dependencies/x264/lib",
    "../../dependencies/skia/out/Static",
    "../../dependencies/libzip_build/lib",
    "../../renderer/build/bin/%{cfg.buildcfg}",
    "/usr/local/lib",
    "/usr/lib",
}

files {"../src/**.cpp"}

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
