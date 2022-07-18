workspace "rive"
configurations {"debug", "release"}

dependencies = os.getenv("DEPENDENCIES")

rive = "../../";

dofile(path.join(path.getabsolute(rive) .. "/build", "premake5.lua"))

project "rive_tess_renderer"
    kind "StaticLib"
    language "C++"
    cppdialect "C++17"
    toolset "clang"
    targetdir "%{cfg.system}/bin/%{cfg.buildcfg}"
    objdir "%{cfg.system}/obj/%{cfg.buildcfg}"
    includedirs {
        "../include",
        rive .. "/include",
        dependencies .. "/sokol"
    }
    files {
        "../src/**.cpp",
    }
    buildoptions {"-Wall", "-fno-exceptions", "-fno-rtti", "-Werror=format"}
    defines {"CONTOUR_RECURSIVE"}

    filter "configurations:debug"
        buildoptions {"-g"}
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:release"
        buildoptions {"-flto=full"}
        defines {"RELEASE", "NDEBUG"}
        optimize "On"

    filter { "options:graphics=gl" }
        defines {"SOKOL_GLCORE33"}
    
    filter { "options:graphics=metal" }
        defines {"SOKOL_METAL"}
    
    filter { "options:graphics=d3d" }
        defines {"SOKOL_D3D11"}

    newoption {
        trigger = "graphics",
        value = "gl",
        description = "The graphics api to use.",
        allowed = {
            { "gl" },
            { "metal" },
            { "d3d" }
        }
    }