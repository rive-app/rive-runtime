workspace "rive"
configurations {"debug", "release"}

project "font_converter"
kind "ConsoleApp"
language "C++"
cppdialect "C++17"
targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
includedirs {
    "../include",
    "../../../include",
    "../../renderer/include",
    "../../dependencies/skia",
    "/usr/local/include",
    "/usr/include",
}

if os.host() == 'macosx' then 
    links {
        "Cocoa.framework",
        "CoreFoundation.framework",
        "IOKit.framework",
        "Security.framework",
        "bz2",
        "iconv",
        "lzma",
        "skia",
        "z",  -- lib av format 
    }
else
    links {
        "m",
        "skia",
        "z",
        "dl",
        "fontconfig"
    }
end 

libdirs {
    "../../dependencies/skia/out/static",
    "/usr/local/lib",
    "/usr/lib",
}

files {"../src/**.cpp", "../src/**.c"}

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
