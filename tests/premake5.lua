dofile('rive_tools_project.lua')

if not _OPTIONS['for_unreal'] then
    rive_tools_project('bench', _OPTIONS['os'] == 'ios' and 'StaticLib' or 'ConsoleApp')
    do
        files({ 'bench/*.cpp' })
    end
end

rive_tools_project('gms', 'RiveTool')
do
    files({ 'gm/*.cpp' })
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
    files({ 'goldens/goldens.cpp' })
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
    files({ 'player/player.cpp' })
    filter('system:emscripten')
    do
        files({ 'player/player.html' })
    end
end

rive_tools_project('command_buffer_example', 'RiveTool')
do
    files({
        'command_buffer_example/command_buffer_example.cpp',
    })
end
