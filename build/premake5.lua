workspace('rive')
configurations({ 'debug', 'release' })

require('setup_compiler')

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

dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_harfbuzz.lua'))
dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_sheenbidi.lua'))
dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_miniaudio.lua'))
dofile(path.join(path.getabsolute('../dependencies/'), 'premake5_yoga.lua'))

project('rive')
do
    kind('StaticLib')
    language('C++')
    targetdir('%{cfg.system}/bin/%{cfg.buildcfg}')
    objdir('%{cfg.system}/obj/%{cfg.buildcfg}')
    includedirs({
        '../include',
        harfbuzz .. '/src',
        sheenbidi .. '/Headers',
        miniaudio,
        yoga,
    })

    defines({ 'YOGA_EXPORT=', '_RIVE_INTERNAL_' })

    includedirs({
        '../dependencies/',
    })
    forceincludes({ 'rive_yoga_renames.h' })

    files({ '../src/**.cpp' })

    flags({ 'FatalCompileWarnings' })

    filter({ 'options:for_unreal' })
    do
        cppdialect('C++17')
        defines({ '_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR' })
    end

    filter({ 'options:not for_unreal' })
    do
        cppdialect('C++11')
    end

    filter({ 'system:macosx' })
    do
        buildoptions({
            -- this triggers too much on linux, so just enable here for now
            '-Wimplicit-float-conversion',
        })
    end

    -- filter {'toolset:not msc', 'files:../src/audio/audio_engine.cpp'}
    filter({ 'system:not windows', 'files:../src/audio/audio_engine.cpp' })
    do
        buildoptions({ '-Wno-implicit-int-conversion' })
    end

    filter({ 'system:windows', 'files:../src/audio/audio_engine.cpp' })
    do
        -- Too many warnings from miniaudio.h
        removeflags({ 'FatalCompileWarnings' })
    end

    -- filter 'files:../src/audio/audio_engine.cpp'
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

    filter({ 'system:macosx', 'configurations:release' })
    do
        buildoptions({ '-flto=full' })
    end

    filter({ 'system:ios' })
    do
        buildoptions({ '-flto=full', '-Wno-implicit-int-conversion' })
        files({ '../src/audio/audio_engine.m' })
    end

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end

    filter({ 'system:ios', 'options:variant=system' })
    do
        buildoptions({
            '-mios-version-min=13.0 -fembed-bitcode -arch arm64 -isysroot '
                .. (os.getenv('IOS_SYSROOT') or ''),
        })
    end

    filter({ 'system:ios', 'options:variant=emulator' })
    do
        buildoptions({
            '--target=arm64-apple-ios13.0.0-simulator',
            '-mios-version-min=13.0 -arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('IOS_SYSROOT') or ''),
        })
        targetdir('%{cfg.system}_sim/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}_sim/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:ios', 'options:variant=xros' })
    do
        buildoptions({
            '--target=arm64-apple-xros1.0',
            '-fembed-bitcode -arch arm64 -isysroot '
                .. (os.getenv('XROS_SYSROOT') or ''),
        })
        targetdir('xros/bin/%{cfg.buildcfg}')
        objdir('xros/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:ios', 'options:variant=xrsimulator' })
    do
        buildoptions({
            '--target=arm64-apple-xros1.0-simulator',
            '-arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('XROS_SYSROOT') or ''),
        })
        targetdir('xrsimulator/bin/%{cfg.buildcfg}')
        objdir('xrsimulator/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:ios', 'options:variant=appletvos' })
    do
        buildoptions({
            '--target=arm64-apple-tvos',
            '-mappletvos-version-min=16.0',
            '-fembed-bitcode -arch arm64 -isysroot '
                .. (os.getenv('APPLETVOS_SYSROOT') or ''),
        })
        targetdir('appletvos/bin/%{cfg.buildcfg}')
        objdir('appletvos/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:ios', 'options:variant=appletvsimulator' })
    do
        buildoptions({
            '--target=arm64-apple-tvos-simulator',
            '-mappletvsimulator-version-min=16.0',
            '-arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('APPLETVOS_SYSROOT') or ''),
        })
        targetdir('appletvsimulator/bin/%{cfg.buildcfg}')
        objdir('appletvsimulator/obj/%{cfg.buildcfg}')
    end

    filter('system:macosx or system:ios')
    do
        files({ '../src/text/font_hb_apple.mm' })
    end

    filter({ 'system:android', 'configurations:release' })
    do
        buildoptions({ '-flto=full' })
    end

    -- Is there a way to pass 'arch' as a variable here?
    filter({ 'system:android', 'options:arch=x86' })
    do
        targetdir('%{cfg.system}/x86/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/x86/obj/%{cfg.buildcfg}')
        -- Ignore fatal warning for miniaudio on x86 devices.
        filter({ 'files:../src/audio/audio_engine.cpp' })
        do
            buildoptions({ '-Wno-atomic-alignment' })
        end
    end

    filter({ 'system:android', 'options:arch=x64' })
    do
        targetdir('%{cfg.system}/x64/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/x64/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'options:arch=arm' })
    do
        targetdir('%{cfg.system}/arm/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/arm/obj/%{cfg.buildcfg}')
    end

    filter({ 'system:android', 'options:arch=arm64' })
    do
        targetdir('%{cfg.system}/arm64/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}/arm64/obj/%{cfg.buildcfg}')
    end

    filter('configurations:debug')
    do
        defines({ 'DEBUG' })
        symbols('On')
    end

    filter('configurations:release')
    do
        defines({ 'RELEASE' })
        defines({ 'NDEBUG' })
        optimize('On')
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
        { 'xros', 'Builds for Apple Vision Pro' },
        { 'xrsimulator', 'Builds for Apple Vision Pro simulator' },
        { 'appletvos', 'Builds for Apple TV' },
        { 'appletvsimulator', 'Builds for Apple TV simulator' },
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
