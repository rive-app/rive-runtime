workspace "rive_thumbnail_generator"
configurations {"debug", "release"}

project "rive_thumbnail_generator"
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
targetdir "bin/%{cfg.buildcfg}"
objdir "obj/%{cfg.buildcfg}"
includedirs {"../include", "../../../rive/include", "../../renderer/include", "../../dependencies/skia",
             "../../dependencies/skia/include/core", "../../dependencies/skia/include/effects",
             "../../dependencies/skia/include/gpu", "../../dependencies/skia/include/config"}

if os.host() == "macosx" then
    links {"Cocoa.framework",  "rive", "skia", "rive_skia_renderer"}
else 
    links {"rive", "skia", "rive_skia_renderer"}
end

libdirs {"../../../rive/build/bin/%{cfg.buildcfg}", "../../dependencies/skia/out/Static",
         "../../renderer/build/bin/%{cfg.buildcfg}"}

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
