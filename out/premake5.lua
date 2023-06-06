workspace "rive"
configurations {"debug", "release"}

require 'setup_compiler'

dofile("premake5_pls_renderer.lua")
dofile(RIVE_RUNTIME_DIR .. "/build/premake5.lua")

newoption {
    trigger = "with-skia",
    description = "use skia",
}
if _OPTIONS["with-skia"]
then
    dofile(RIVE_RUNTIME_DIR .. "/skia/renderer/build/premake5.lua")
end

if _OPTIONS["wasm"]
then
    dofile("premake5_canvas2d_renderer.lua")
end

project "path_fiddle"
do
    dependson 'rive'
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    exceptionhandling "Off"
    rtti "Off"
    targetdir "%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"
    includedirs {"../include",
                 RIVE_RUNTIME_DIR .. "/include",
                 "../glad",
                 "../include",
                 RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw/include"}
    flags { "FatalWarnings" }

    files {"../path_fiddle/*.cpp"}

    links {"rive", "rive_pls_renderer", "rive_harfbuzz", "rive_sheenbidi"}

    filter "options:with-skia"
    do
        includedirs {RIVE_RUNTIME_DIR .. "/skia/renderer/include",
                     RIVE_RUNTIME_DIR .. "/skia/dependencies",
                     RIVE_RUNTIME_DIR .. "/skia/dependencies/skia"}
        defines {"RIVE_SKIA", "SK_GL"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/skia/out/static"}
        links {"skia", "rive_skia_renderer"}
    end

    filter "system:windows"
    do
        architecture "x64"
        defines {"RIVE_WINDOWS", "_CRT_SECURE_NO_WARNINGS"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src/Release"}
        links {"glfw3", "opengl32"}
    end

    filter "system:macosx"
    do
        files {"../path_fiddle/*.mm"}
        buildoptions {"-fobjc-arc"}
        links {"glfw3",
               "Cocoa.framework",
               "Metal.framework",
               "QuartzCore.framework",
               "IOKit.framework"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src"}
    end

    filter "system:emscripten"
    do
        targetname "path_fiddle.js"
        targetdir "wasm_%{cfg.buildcfg}/path_fiddle"
        objdir "obj/wasm_%{cfg.buildcfg}"
        linkoptions {"-sUSE_GLFW=3",
                     "--preload-file ../../../gold/rivs@/"}
        files {"../path_fiddle/index.html"}
    end

    filter 'files:**.html'
    do
        buildmessage "Copying %{file.relpath} to %{cfg.targetdir}"
        buildcommands {"cp %{file.relpath} %{cfg.targetdir}/%{file.name}"}
        buildoutputs { "%{cfg.targetdir}/%{file.name}" }
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
end

project "bubbles"
do
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++17"
    exceptionhandling "Off"
    rtti "Off"
    targetdir "%{cfg.buildcfg}"
    objdir "obj/%{cfg.buildcfg}"
    includedirs {"../",
                 "../glad",
                 "../include",
                 RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw/include"}
    flags { "FatalWarnings" }
    files {"../bubbles/bubbles.cpp",
           "../glad/glad.c"}

    filter "system:windows"
    do
        architecture "x64"
        defines {"RIVE_WINDOWS"}
        links {"glfw3", "opengl32"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src/Release"}
    end

    filter "system:macosx"
    do
        links {'glfw3', 'Cocoa.framework', 'IOKit.framework'}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src"}
    end

    filter "system:emscripten"
    do
        targetname "bubbles.js"
        targetdir "wasm_%{cfg.buildcfg}/bubbles"
        objdir "obj/wasm_%{cfg.buildcfg}"
        linkoptions {"-sUSE_GLFW=3"}
        files {"../bubbles/index.html"}
    end

    filter 'files:**.html'
    do
        buildmessage "Copying %{file.relpath} to %{cfg.targetdir}"
        buildcommands {"cp %{file.relpath} %{cfg.targetdir}/%{file.name}"}
        buildoutputs { "%{cfg.targetdir}/%{file.name}" }
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
end
