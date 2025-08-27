workspace('rive')
configurations({ 'debug', 'release' })

require('setup_compiler')

SKIA_DIR = os.getenv('SKIA_DIR')
dependencies = os.getenv('DEPENDENCIES')

if SKIA_DIR == nil and dependencies ~= nil then
    SKIA_DIR = dependencies .. '/skia'
else
    if SKIA_DIR == nil then
        SKIA_DIR = 'skia'
    end
    SKIA_DIR = '../../dependencies/' .. SKIA_DIR
end

project('rive_skia_renderer')
do
    kind('StaticLib')
    language('C++')
    cppdialect('C++17')
    targetdir('%{cfg.system}/bin/%{cfg.buildcfg}')
    objdir('%{cfg.system}/obj/%{cfg.buildcfg}')
    includedirs({
        '../include',
        '../../../cg_renderer/include',
        '../../../include',
    })

    libdirs({ '../../../build/%{cfg.system}/bin/%{cfg.buildcfg}' })

    files({ '../src/**.cpp' })

    fatalwarnings { "All" }

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end

    filter({ 'system:macosx' })
    do
        includedirs({ SKIA_DIR })
        libdirs({ SKIA_DIR .. '/out/static' })
        links({ 'Cocoa.framework', 'rive', 'skia' })
    end

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        links({})
        buildoptions({
            '-fembed-bitcode -arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('MACOS_SYSROOT') or ''),
        })
    end

    filter({ 'system:linux or windows' })
    do
        includedirs({ SKIA_DIR })
        libdirs({ SKIA_DIR .. '/out/static' })
        links({ 'rive', 'skia' })
    end

    filter({ 'system:ios' })
    do
        includedirs({ SKIA_DIR })
        libdirs({ SKIA_DIR .. '/out/static' })
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
            '--target=arm64-apple-ios13.0.0-simulator -mios-version-min=13.0 -arch x86_64 -arch arm64 -isysroot '
                .. (os.getenv('IOS_SYSROOT') or ''),
        })
        targetdir('%{cfg.system}_sim/bin/%{cfg.buildcfg}')
        objdir('%{cfg.system}_sim/obj/%{cfg.buildcfg}')
    end

    -- Is there a way to pass 'arch' as a variable here?
    filter({ 'system:android' })
    do
        includedirs({ SKIA_DIR })

        filter({ 'system:android', 'options:arch=x86' })
        do
            targetdir('%{cfg.system}/x86/bin/%{cfg.buildcfg}')
            objdir('%{cfg.system}/x86/obj/%{cfg.buildcfg}')
            libdirs({ SKIA_DIR .. '/out/x86' })
        end

        filter({ 'system:android', 'options:arch=x64' })
        do
            targetdir('%{cfg.system}/x64/bin/%{cfg.buildcfg}')
            objdir('%{cfg.system}/x64/obj/%{cfg.buildcfg}')
            libdirs({ SKIA_DIR .. '/out/x64' })
        end

        filter({ 'system:android', 'options:arch=arm' })
        do
            targetdir('%{cfg.system}/arm/bin/%{cfg.buildcfg}')
            objdir('%{cfg.system}/arm/obj/%{cfg.buildcfg}')
            libdirs({ SKIA_DIR .. '/out/arm' })
        end

        filter({ 'system:android', 'options:arch=arm64' })
        do
            targetdir('%{cfg.system}/arm64/bin/%{cfg.buildcfg}')
            objdir('%{cfg.system}/arm64/obj/%{cfg.buildcfg}')
            libdirs({ SKIA_DIR .. '/out/arm64' })
        end
    end

    filter({ 'configurations:release', 'system:macosx' })
    do
        buildoptions({ '-flto=full' })
    end

    filter({ 'configurations:release', 'system:android' })
    do
        buildoptions({ '-flto=full' })
    end

    filter({ 'configurations:release', 'system:ios' })
    do
        buildoptions({ '-flto=full' })
    end

    filter('configurations:debug')
    do
        defines({ 'DEBUG' })
        symbols('On')
    end

    filter('configurations:release')
    do
        defines({ 'RELEASE', 'NDEBUG' })
        optimize('On')
    end

    filter({ 'options:with_rive_text' })
    do
        defines({ 'WITH_RIVE_TEXT' })
    end
    filter({ 'options:with_rive_audio=system' })
    do
        defines({ 'WITH_RIVE_AUDIO' })
    end
    filter({ 'options:with_rive_audio=external' })
    do
        defines({
            'WITH_RIVE_AUDIO',
            'EXTERNAL_RIVE_AUDIO_ENGINE',
            'MA_NO_DEVICE_IO',
        })
    end
end

newoption({ trigger = 'with_rive_text', description = 'Enables text experiments' })

newoption({
    trigger = 'with_rive_audio',
    value = 'disabled',
    description = 'The audio mode to use.',
    allowed = { { 'disabled' }, { 'system' }, { 'external' } },
})

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
    trigger = 'arch',
    value = 'ABI',
    description = 'The ABI with the right toolchain for this build, generally with Android',
    allowed = { { 'x86' }, { 'x64' }, { 'arm' }, { 'arm64' } },
})
