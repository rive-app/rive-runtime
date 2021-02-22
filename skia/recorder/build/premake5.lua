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
includedirs {"../include", "../../../include", "../../renderer/include", "../../dependencies/glfw/include",
             "../../dependencies/skia", "../../dependencies/skia/include/core",
             "../../dependencies/skia/include/effects", "../../dependencies/skia/include/gpu",
             "../../dependencies/skia/include/config", 
             "../../dependencies/FFmpeg", 
             "../../dependencies/x264/include", 
             "/usr/local/include",
            }


links {
    "avcodec", 
    "avformat", 
    "avutil", 
    "glfw3", 
    "z",
    "bz2",
    "iconv",
    "lzma",
    "m",
    "rive_skia_renderer", 
    "rive", 
    "skia", 
    "swscale", 
    "swresample", 
    "AudioToolbox.framework", 
    "AudioUnit.framework", 
    "Cocoa.framework", 
    "CoreFoundation.framework", 
    "CoreServices.framework", 
    "CoreVideo.framework", 
    "CoreMedia.framework",
    "Security.framework",
    "IOKit.framework", 
    "VideoToolbox.framework",
    "x264",
}

libdirs {
    "../../../build/bin/%{cfg.buildcfg}", 
    "../../dependencies/FFmpeg/libavcodec", 
    "../../dependencies/FFmpeg/libavformat", 
    "../../dependencies/FFmpeg/libavutil", 
    "../../dependencies/FFmpeg/libswscale",
    "../../dependencies/FFmpeg/libswresample",
    "../../dependencies/x264/lib",
    "../../dependencies/glfw_build/src",
    "../../dependencies/skia/out/Static", 
    
    "../../renderer/build/bin/%{cfg.buildcfg}", 
    "/usr/local/lib",
}

files {"../src/**.cpp"}

buildoptions {"-Wall", "-fno-rtti"}

filter "configurations:debug"
defines {"DEBUG"}
symbols "On"

filter "configurations:release"
defines {"RELEASE"}
defines { "NDEBUG" }
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
