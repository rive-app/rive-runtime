dofile('rive_build_config.lua')

defines({
    'TESTING',
    'ENABLE_QUERY_FLAT_VERTICES',
    'WITH_RIVE_TOOLS',
    'WITH_RIVE_TEXT',
    'WITH_RIVE_AUDIO',
})

dofile(path.join(path.getabsolute('../../'), 'premake5_v2.lua'))

project('tests')
do
    kind('ConsoleApp')
    exceptionhandling('On')

    includedirs({
        './include',
        '../../include',
        miniaudio,
    })

    links({ 'rive', 'rive_harfbuzz', 'rive_sheenbidi' })

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
end
