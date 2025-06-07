dofile('rive_build_config.lua')

local dependency = require('dependency')
sokol = dependency.github('luigi-rosso/sokol', 'support_transparent_framebuffer')
libtess2 = dependency.github('memononen/libtess2', 'master')
earcut = dependency.github('mapbox/earcut.hpp', 'master')

rive = '../'

dofile(path.join(path.getabsolute(rive), 'premake5_v2.lua'))

project('rive_tess_renderer')
do
    kind('StaticLib')
    includedirs({
        'include',
        rive .. '/include',
        sokol,
        earcut .. '/include/mapbox',
        libtess2 .. '/Include',
    })
    files({ 'src/**.cpp', libtess2 .. '/Source/**.c' })
    buildoptions({ '-Wall', '-fno-exceptions', '-fno-rtti', '-Werror=format' })

    filter({ 'system:emscripten' })
    do
        defines({ 'SOKOL_GLCORE33' })
    end

    filter({ 'system:macosx' })
    do
        defines({ 'SOKOL_METAL' })
    end

    filter({ 'system:windows' })
    do
        defines({ 'SOKOL_D3D11' })
    end
end

project('rive_tess_tests')
do
    dependson('rive_tess_renderer')
    dependson('rive')
    kind('ConsoleApp')
    includedirs({
        rive .. 'tests/include', -- for catch.hpp and for things rive_file_reader.hpp
        'include',
        rive .. '/include',
        sokol,
        earcut .. '/include/mapbox',
        libtess2 .. '/Include',
    })
    files({ 'test/**.cpp', rive .. 'utils/no_op_factory.cpp' })
    links({ 'rive_tess_renderer', 'rive', 'rive_harfbuzz', 'rive_sheenbidi', 'rive_yoga' })
    -- defines({ 'TESTING', 'YOGA_EXPORT=' })

    filter({ 'system:macosx' })
    do
        links({
            'Cocoa.framework',
            'IOKit.framework',
            'CoreVideo.framework',
            'OpenGL.framework',
        })
    end
end
