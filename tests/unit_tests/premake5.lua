filter({ 'system:not windows' })
do
    buildoptions({ '-ffp-model=strict' })
end
filter({ 'system:windows' })
do
    buildoptions({ '/fp:strict' })
end
filter({ 'system:windows', 'options:toolset=clang' })
do
    buildoptions({
        '-Wno-overriding-option',
    })
end
filter({})

dofile('rive_build_config.lua')
TESTING = true
defines({
    'TESTING',
    'ENABLE_QUERY_FLAT_VERTICES',
    'WITH_RIVE_TOOLS',
    'WITH_RIVE_TEXT',
    'WITH_RIVE_AUDIO',
    'WITH_RIVE_AUDIO_TOOLS',
    'WITH_RIVE_LAYOUT',
    'YOGA_EXPORT=',
    'RIVE_NO_CORETEXT',
})

if _OPTIONS['toolset'] == 'msc' then
    defines({
        'MA_NO_MP3', -- miniaudio's mp3 decoder + fp:strict causes C2099
    })
end
dofile(path.join(path.getabsolute('../../'), 'premake5_v2.lua'))
dofile(path.join(path.getabsolute('../../decoders/'), 'premake5_v2.lua'))

-- Force "--raw_shaders" so that we can include the generated shader files and
-- compile them as C++ for unit tests.
if _OPTIONS['toolset'] ~= 'msc' then
    _OPTIONS['raw_shaders'] = true
end

dofile(path.join(path.getabsolute('../../renderer/'), 'premake5_pls_renderer.lua'))

dofile('../rive_tools_project.lua')

rive_tools_project('unit_tests', 'ConsoleApp')
do
    kind('ConsoleApp')
    exceptionhandling('On')

    includedirs({
        '..',
        '../include',
        '../../include',
        '../../decoders/include',
        '../../renderer/include',
        '../../renderer/src',
        harfbuzz .. '/src',
        miniaudio,
        yoga,
        pls_generated_headers,
        luau .. '/VM/include',
        -- we need these for testing so we can compile
        luau .. '/Compiler/include',
        luau .. '/Ast/include',
        luau .. '/Common/include',
    })

    links({
        'rive',
        'rive_harfbuzz',
        'rive_sheenbidi',
        'rive_yoga',
        'rive_decoders',
        'libpng',
        'zlib',
        'libjpeg',
        'libwebp',
        'miniaudio',
        'luau_vm',
        'luau_compiler',
    })

    files({
        'runtime/**.cpp', -- the runtime tests
        'renderer/**.cpp', -- the renderer tests
        '../../utils/**.cpp', -- no_op utils
        '../common/render_context_null.cpp',
    })

    filter('system:linux')
    do
        links({ 'dl', 'pthread' })
    end
    filter({ 'options:not no-harfbuzz-renames' })
    do
        includedirs({
            dependencies,
        })
        forceincludes({ 'rive_harfbuzz_renames.h' })
    end

    filter({ 'options:not no-yoga-renames' })
    do
        includedirs({
            dependencies,
        })
        forceincludes({ 'rive_yoga_renames.h' })
    end

    filter({ 'system:macosx' })
    do
        links({
            'Foundation.framework',
            'ImageIO.framework',
            'CoreGraphics.framework',
            'CoreText.framework',
        })
    end

    filter({ 'toolset:not msc' })
    do
        linkoptions({ '-fprofile-instr-generate', '-fcoverage-mapping' })
    end

    filter({})
end
