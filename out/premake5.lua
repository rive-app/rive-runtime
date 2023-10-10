workspace "rive"
configurations {"debug", "release"}

require 'setup_compiler'

dofile("premake5_pls_renderer.lua")
dofile(RIVE_RUNTIME_DIR .. "/build/premake5.lua")
dofile(RIVE_RUNTIME_DIR .. '/decoders/build/premake5.lua')

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

    files {
        "../path_fiddle/**.cpp",
    }

    links {
        "rive",
        "rive_pls_renderer",
        "rive_decoders",
        "libpng",
        "zlib",
        "rive_harfbuzz",
        "rive_sheenbidi"
    }

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
        links {"glfw3", "opengl32", "d3d11", "dxgi", "d3dcompiler"}
    end

    filter "system:macosx"
    do
        files {"../path_fiddle/**.mm"}
        buildoptions {"-fobjc-arc"}
        links {"glfw3",
               "Cocoa.framework",
               "Metal.framework",
               "QuartzCore.framework",
               "IOKit.framework"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src"}
    end

    filter "options:with-dawn"
    do
        includedirs {
            "../dependencies/dawn/include",
            "../dependencies/dawn/out/release/gen/include",
        }
        libdirs {
            "../dependencies/dawn/out/release/obj/src/dawn",
            "../dependencies/dawn/out/release/obj/src/dawn/native",
            "../dependencies/dawn/out/release/obj/src/dawn/platform",
            "../dependencies/dawn/out/release/obj/src/dawn/platform",
        }
        links {
            "dawn_native_static",
            "webgpu_dawn",
            "dawn_platform_static",
            "dawn_proc_static",
        }
    end

    filter {"options:with-dawn", "system:windows"}
    do
        links {
            "dxguid",
        }
    end

    filter {"options:with-dawn", "system:macosx"}
    do
        links {
            "IOSurface.framework",
        }
    end

    filter "system:emscripten"
    do
        targetname "path_fiddle.js"
        targetdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        objdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
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

-- Embed some .rivs into .c files so we don't have to implement file loading.
project "webgpu_player_rivs"
do
    dependson 'rive_pls_shaders' -- to create the out/obj/generated directory
    kind 'Makefile'

    buildcommands {
        'mkdir -p obj/generated',
        'test -f obj/generated/marty.riv.c || xxd -n marty_riv -i ../../../gold/rivs/marty.riv obj/generated/marty.riv.c',
        'test -f obj/generated/Knight_square_2.riv.c || xxd -n Knight_square_2_riv -i ../../../gold/rivs/Knight_square_2.riv obj/generated/Knight_square_2.riv.c',
        'test -f obj/generated/Rope.riv.c || xxd -n Rope_riv -i ../../../gold/rivs/Rope.riv obj/generated/Rope.riv.c',
        'test -f obj/generated/Skills.riv.c || xxd -n Skills_riv -i ../../../gold/rivs/Skills.riv obj/generated/Skills.riv.c',
        'test -f obj/generated/falling.riv.c || xxd -n falling_riv -i ../../../gold/rivs/falling.riv obj/generated/falling.riv.c',
        'test -f obj/generated/tape.riv.c || xxd -n tape_riv -i ../../../gold/rivs/tape.riv obj/generated/tape.riv.c',
    }

    filter 'system:windows'
    do
        architecture 'x64'
    end
end

project "webgpu_player"
do
    dependson 'webgpu_player_rivs'
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

    files {
        "../webgpu_player/webgpu_player.cpp",
        "../webgpu_player/index.html",
    }

    links {
        "rive",
        "rive_pls_renderer",
        "rive_decoders",
        "libpng",
        "zlib",
        "rive_harfbuzz",
        "rive_sheenbidi"
    }

    filter "system:windows"
    do
        architecture "x64"
        defines {"RIVE_WINDOWS", "_CRT_SECURE_NO_WARNINGS"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src/Release"}
        links {"glfw3", "opengl32", "d3d11", "dxgi", "d3dcompiler"}
    end

    filter "system:macosx"
    do
        files {"../path_fiddle/fiddle_context_dawn_helper.mm"}
        buildoptions {"-fobjc-arc"}
        links {"glfw3",
               "Cocoa.framework",
               "Metal.framework",
               "QuartzCore.framework",
               "IOKit.framework"}
        libdirs {RIVE_RUNTIME_DIR .. "/skia/dependencies/glfw_build/src"}
    end

    filter "options:with-dawn"
    do
        includedirs {
            "../dependencies/dawn/include",
            "../dependencies/dawn/out/release/gen/include",
        }
        libdirs {
            "../dependencies/dawn/out/release/obj/src/dawn",
            "../dependencies/dawn/out/release/obj/src/dawn/native",
            "../dependencies/dawn/out/release/obj/src/dawn/platform",
            "../dependencies/dawn/out/release/obj/src/dawn/platform",
        }
        links {
            "dawn_native_static",
            "webgpu_dawn",
            "dawn_platform_static",
            "dawn_proc_static",
        }
    end

    filter {"options:with-dawn", "system:windows"}
    do
        links {
            "dxguid",
        }
    end

    filter {"options:with-dawn", "system:macosx"}
    do
        links {
            "IOSurface.framework",
        }
    end

    filter "system:emscripten"
    do
        targetname "webgpu_player.js"
        targetdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        objdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        linkoptions {
            "-sEXPORTED_FUNCTIONS=_riveInitPlayer,_riveMainLoop",
            "-sEXPORTED_RUNTIME_METHODS=ccall,cwrap",
            "-sSINGLE_FILE",
            "-sUSE_WEBGPU",
            "-sENVIRONMENT=web,shell",
        }
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
           "../glad/glad.c",
           "../glad/glad_custom.c"}

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
        targetdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
        objdir(_OPTIONS['emsdk'] .. '_%{cfg.buildcfg}')
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
