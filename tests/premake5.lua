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
end

rive_tools_project('goldens', 'RiveTool')
do
    exceptionhandling('On')
    files({ 'goldens/goldens.cpp' })
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
end

rive_tools_project('player', 'RiveTool')
do
    files({ 'player/player.cpp' })
end
