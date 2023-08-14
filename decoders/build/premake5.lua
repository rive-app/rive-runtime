workspace 'rive'
configurations {"debug", "release"}

require 'setup_compiler'

rive = path.getabsolute("../../")

dofile(rive .. '/dependencies/premake5_libpng.lua')

project 'rive_decoders'
    dependson 'libpng'
    kind 'StaticLib'
    language "C++"
    cppdialect "C++17"
    exceptionhandling "Off"
    rtti "Off"
    targetdir "%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"
    flags { "FatalWarnings" }

    includedirs {
        '../include',
        '../../include',
        libpng,
    }

    files {
        '../src/**.cpp'
    }

    filter { "system:windows" }
    do
        architecture "x64"
    end

    filter "configurations:debug"
    do
        defines {"DEBUG"}
        symbols "On"
    end

    filter "configurations:release"
    do
        defines {"RELEASE"}
        defines {"NDEBUG"}
        optimize "On"
    end
