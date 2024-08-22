dofile('rive_build_config.lua')

defines({
    'TESTING',
    'ENABLE_QUERY_FLAT_VERTICES',
    'WITH_RIVE_TOOLS',
    'WITH_RIVE_TEXT',
    'WITH_RIVE_AUDIO',
    'WITH_RIVE_AUDIO_TOOLS',
    'WITH_RIVE_LAYOUT',
    'YOGA_EXPORT=',
})

dofile(path.join(path.getabsolute('../../'), 'premake5_v2.lua'))
dofile(path.join(path.getabsolute('../../decoders/'), 'premake5_v2.lua'))

project('tests')
do
    kind('ConsoleApp')
    exceptionhandling('On')

    includedirs({
        './include',
        '../../include',
        '../../decoders/include',
        miniaudio,
        yoga,
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
    })

    files({
        '../../test/**.cpp', -- the tests
        '../../utils/**.cpp', -- no_op utils
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
end
