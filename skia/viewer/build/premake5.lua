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
toolset "clang"
targetdir "bin/%{cfg.buildcfg}"
objdir "obj/%{cfg.buildcfg}"
includedirs {"../include", "../../../include", "../../renderer/include", "../../dependencies/glfw/include",
             "../../dependencies/skia", "../../dependencies/skia/include/core",
             "../../dependencies/skia/include/effects", "../../dependencies/skia/include/gpu",
             "../../dependencies/skia/include/config", "../../dependencies/imgui", "../../dependencies",
             "../../dependencies/gl3w/build/include"}

links {"Cocoa.framework", "IOKit.framework", "CoreVideo.framework", "rive", "skia", "rive_skia_renderer", "glfw3"}
libdirs {"../../../build/bin/%{cfg.buildcfg}", "../../dependencies/glfw_build/src",
         "../../dependencies/skia/out/Static", "../../renderer/build/bin/%{cfg.buildcfg}"}

files {"../src/**.cpp", "../../dependencies/gl3w/build/src/gl3w.c",
       "../../dependencies/imgui/backends/imgui_impl_glfw.cpp",
       "../../dependencies/imgui/backends/imgui_impl_opengl3.cpp", "../../dependencies/imgui/imgui_widgets.cpp",
       "../../dependencies/imgui/imgui.cpp", "../../dependencies/imgui/imgui_tables.cpp",
       "../../dependencies/imgui/imgui_draw.cpp"}

buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti"}

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
