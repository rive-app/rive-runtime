dofile('rive_tools_project.lua')

newoption({
    trigger = 'no_tools_shader_hotloading',
    description = 'do not compile in support for shader hotloading',
})

if not _OPTIONS['for_unreal'] then
    rive_tools_project('bench', _OPTIONS['os'] == 'ios' and 'StaticLib' or _OPTIONS['all_tools_as_static'] and 'StaticLib' or 'ConsoleApp' )
    do
        files({ 'bench/*.cpp' })
    end
end

rive_tools_project('gms', 'RiveTool')
do
    files({ 'gm/*.cpp'})
    -- Ore GM tests need Obj-C++ on Apple (ore headers include <Metal/Metal.h>).
    -- .mm wrappers #include the .cpp files so every Apple generator compiles
    -- them as Obj-C++ without needing compileas or buildoptions hacks.
    -- On non-Apple, ore GMs are excluded from the gm/*.cpp glob but re-added
    -- per-platform as the corresponding Ore backend lands.
    removefiles({ 'gm/ore_*.cpp' })
    filter('system:macosx or ios')
    do
        files({ 'gm/*.mm' })
        buildoptions({ '-fobjc-arc' })
    end
    -- D3D11 Ore backend: include ore GMs on Windows when with_rive_canvas is on.
    filter({ 'system:windows', 'options:with_rive_canvas' })
    do
        files({ 'gm/ore_*.cpp' })
    end
    -- GL Ore backend: include ore GMs on Android/Emscripten/Linux.
    filter({ 'system:android or emscripten or linux', 'options:with_rive_canvas' })
    do
        files({ 'gm/ore_*.cpp' })
    end
    filter({})
    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
    filter('system:emscripten')
    do
        files({ 'gm/gms.html' })
    end
end

rive_tools_project('goldens', 'RiveTool')
do
    exceptionhandling('On')
    files({ 'goldens/goldens.cpp'})
    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
    filter('system:emscripten')
    do
        files({ 'goldens/goldens.html' })
    end
end

rive_tools_project('player', 'RiveTool')
do
    files({ 'player/player.cpp'})
    filter('system:emscripten')
    do
        files({ 'player/player.html' })
    end

    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
end
