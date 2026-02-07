dofile('rive_tools_project.lua')

newoption({
    trigger = 'no_tools_shader_hotloading',
    description = 'do not compile in support for shader hotloading',
})

if not _OPTIONS['for_unreal'] then
    rive_tools_project('bench', _OPTIONS['os'] == 'ios' and 'StaticLib' or 'ConsoleApp')
    do
        files({ 'bench/*.cpp' })
    end
end

rive_tools_project('gms', 'RiveTool')
do
    files({ 'gm/*.cpp'})
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
