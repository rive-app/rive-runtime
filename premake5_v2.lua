dofile('rive_build_config.lua')

filter({ 'options:with_rive_tools' })
do
    defines({ 'WITH_RIVE_TOOLS' })
end
filter({ 'options:with_rive_text' })
do
    defines({ 'WITH_RIVE_TEXT' })
end
filter({ 'options:with_rive_audio=system' })
do
    defines({ 'WITH_RIVE_AUDIO', 'MA_NO_RESOURCE_MANAGER' })
end

filter({ 'options:with_rive_audio=external' })
do
    defines({
        'WITH_RIVE_AUDIO',
        'EXTERNAL_RIVE_AUDIO_ENGINE',
        'MA_NO_DEVICE_IO',
        'MA_NO_RESOURCE_MANAGER',
    })
end
filter({ 'options:with_rive_layout' })
do
    defines({ 'WITH_RIVE_LAYOUT' })
end
filter({})

dependencies = path.getabsolute('dependencies/')
dofile(path.join(dependencies, 'premake5_harfbuzz_v2.lua'))
dofile(path.join(dependencies, 'premake5_sheenbidi_v2.lua'))
dofile(path.join(dependencies, 'premake5_miniaudio_v2.lua'))
dofile(path.join(dependencies, 'premake5_yoga_v2.lua'))

project('rive')
do
    kind('StaticLib')
    cppdialect('C++11')
    includedirs({
        'include',
        harfbuzz .. '/src',
        sheenbidi .. '/Headers',
        miniaudio,
        yoga,
    })

    filter('action:xcode4')
    do
        -- xcode doesnt like angle brackets except for -isystem
        -- should use externalincludedirs but GitHub runners dont have latest premake5 binaries
        buildoptions({ '-isystem' .. yoga })
    end
    filter({})

    defines({ 'YOGA_EXPORT=' })

    files({ 'src/**.cpp' })

    flags({ 'FatalCompileWarnings' })

    filter({ 'options:with_rive_text', 'options:not no-harfbuzz-renames' })
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

    filter({ 'system:linux' })
    do
        defines({ 'MA_NO_RUNTIME_LINKING' })
    end

    filter({ 'system:macosx' })
    do
        buildoptions({
            -- this triggers too much on linux, so just enable here for now
            '-Wimplicit-float-conversion',
        })
    end

    -- filter {'toolset:not msc', 'files:src/audio/audio_engine.cpp'}
    filter({ 'system:not windows', 'files:src/audio/audio_engine.cpp' })
    do
        buildoptions({ '-Wno-implicit-int-conversion' })
    end

    filter({ 'system:windows', 'files:src/audio/audio_engine.cpp' })
    do
        -- Too many warnings from miniaudio.h
        removeflags({ 'FatalCompileWarnings' })
    end

    filter({ 'system:windows', 'toolset:clang', 'files:src/audio/audio_engine.cpp' })
    do
        buildoptions({
            '-Wno-nonportable-system-include-path',
            '-Wno-zero-as-null-pointer-constant',
            '-Wno-missing-prototypes',
            '-Wno-cast-qual',
            '-Wno-format-nonliteral',
            '-Wno-cast-align',
            '-Wno-covered-switch-default',
            '-Wno-comma',
            '-Wno-tautological-type-limit-compare',
            '-Wno-extra-semi-stmt',
            '-Wno-tautological-constant-out-of-range-compare',
            '-Wno-implicit-fallthrough',
            '-Wno-implicit-int-conversion',
            '-Wno-undef',
            '-Wno-unused-function',
        })
    end

    -- filter 'files:src/audio/audio_engine.cpp'
    -- do
    --     buildoptions {
    --         '-Wno-implicit-int-conversion'
    --     }
    -- end

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        buildoptions({
            '-Wimplicit-float-conversion -fembed-bitcode -arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('MACOS_SYSROOT') or ''),
        })
    end

    filter({ 'system:ios' })
    do
        buildoptions({ '-flto=full', '-Wno-implicit-int-conversion' })
        files({ 'src/audio/audio_engine.m' })
    end

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end

    filter('system:macosx or system:ios')
    do
        files({ 'src/text/font_hb_apple.mm' })
    end
end

newoption({
    trigger = 'variant',
    value = 'type',
    description = 'Choose a particular variant to build',
    allowed = {
        { 'system', 'Builds the static library for the provided system' },
        { 'emulator', 'Builds for an emulator/simulator for the provided system' },
        {
            'runtime',
            'Build the static library specifically targeting our runtimes',
        },
    },
    default = 'system',
})

newoption({
    trigger = 'with_rive_tools',
    description = 'Enables tools usually not necessary for runtime.',
})

newoption({
    trigger = 'with_rive_text',
    description = 'Compiles in text features.',
})

newoption({
    trigger = 'with_rive_audio',
    value = 'disabled',
    description = 'The audio mode to use.',
    allowed = { { 'disabled' }, { 'system' }, { 'external' } },
})

newoption({
    trigger = 'with_rive_layout',
    description = 'Compiles in layout features.',
})
