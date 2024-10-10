dofile('rive_tools_project.lua')

project('imagediff')
do
    kind('ConsoleApp')
    cppdialect('C++17')
    exceptionhandling('On')
    defines({ 'RIVE_TOOLS_NO_GLFW', 'RIVE_TOOLS_NO_GL' })
    includedirs({
        'include',
        '.',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME,
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/include/core',
        libpng,
        zlib,
    })
    files({
        'imagediff/*.cpp',
        'common/write_png_file.cpp',
    })
    libdirs({
        RIVE_RUNTIME_DIR .. '/skia/dependencies/' .. SKIA_DIR_NAME .. '/out/' .. SKIA_OUT_NAME,
    })
    links({
        'skia',
        'libpng',
        'zlib',
    })
    filter({ 'system:windows' })
    do
        links({ 'opengl32' })
    end
    filter({ 'system:windows', 'options:for_unreal' })
    do
        kind('None')
    end
    filter('system:linux')
    do
        links({ 'GL' })
    end
    filter('system:android or ios')
    do
        kind('None') -- Don't build imagediff on mobile.
    end
end

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
