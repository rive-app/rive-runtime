workspace "rive_skia_renderer"
configurations {"debug", "release"}

project "rive_skia_renderer"
kind "StaticLib"
language "C++"
cppdialect "C++17"
targetdir "bin/%{cfg.buildcfg}"
objdir "obj/%{cfg.buildcfg}"
includedirs {"../include", "../../../rive/include", "../../dependencies/skia", "../../dependencies/skia/include/core",
             "../../dependencies/skia/include/effects", "../../dependencies/skia/include/gpu",
             "../../dependencies/skia/include/config"}


if os.host() == "macosx" then
    links {"Cocoa.framework",  "rive", "skia"}
else 
    links {"rive", "skia"}
end

libdirs {"../../../rive/build/bin/%{cfg.buildcfg}", "../../dependencies/skia/out/Static"}

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
