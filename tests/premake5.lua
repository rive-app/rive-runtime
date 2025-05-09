dofile('rive_tools_project.lua')

if not _OPTIONS['for_unreal'] then
    rive_tools_project('bench', _OPTIONS['os'] == 'ios' and 'StaticLib' or 'ConsoleApp')
    do
        files({ 'bench/*.cpp' })
    end
end

rive_tools_project('gms', 'RiveTool')
do
    files({ 'gm/*.cpp', RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
end

rive_tools_project('goldens', 'RiveTool')
do
    exceptionhandling('On')
    files({ 'goldens/goldens.cpp', RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
end

rive_tools_project('player', 'RiveTool')
do
    files({ 'player/player.cpp', RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
end

rive_tools_project('command_buffer_example', 'RiveTool')
do
    files({
        'command_buffer_example/command_buffer_example.cpp',
        RIVE_PLS_DIR .. '/shader_hotload/**.cpp',
    })
end
