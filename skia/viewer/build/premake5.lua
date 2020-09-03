workspace "rive"
configurations {"debug", "release"}

BASE_DIR = path.getabsolute("../../../build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

BASE_DIR = path.getabsolute("../../renderer/build")
location("./")
dofile(path.join(BASE_DIR, "premake5.lua"))

project "rive_viewer"
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
targetdir "bin/%{cfg.buildcfg}"
objdir "obj/%{cfg.buildcfg}"
includedirs {"../include", "../../../include", "../../renderer/include", "../../dependencies/glfw/include",
             "../../dependencies/skia", "../../dependencies/skia/include/core",
             "../../dependencies/skia/include/effects", "../../dependencies/skia/include/gpu",
             "../../dependencies/skia/include/config"}

links {"Cocoa.framework", "IOKit.framework", "CoreVideo.framework", "rive", "skia", "rive_skia_renderer", "glfw3"}
libdirs {"../../../build/bin/%{cfg.buildcfg}", "../../dependencies/glfw_build/src",
         "../../dependencies/skia/out/Static", "../../renderer/build/bin/%{cfg.buildcfg}"}

files {"../src/**.cpp"}

buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti"}

filter "configurations:debug"
defines {"DEBUG"}
symbols "On"

filter "configurations:release"
defines {"RELEASE"}
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
