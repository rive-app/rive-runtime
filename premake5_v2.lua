dofile('rive_build_config.lua')

filter({ 'options:with_rive_tools' })
do
    defines({ 'WITH_RIVE_TOOLS' })
end
filter({ 'options:with_rive_text' })
do
    defines({ 'WITH_RIVE_TEXT' })
end
filter({ 'options:with_rive_scripting' })
do
    defines({ 'WITH_RIVE_SCRIPTING' })
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

if _OPTIONS['with_optick'] then
    dofile(path.join(dependencies, 'premake5_optick.lua'))
end

if _OPTIONS['with_rive_scripting'] then
    luau = require(path.join(path.getabsolute('scripting/'), 'premake5')).luau
else
    luau = ''
end

project('rive')
do
    kind('StaticLib')
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

    defines({ 'YOGA_EXPORT=', '_RIVE_INTERNAL_' })

    files({ 'src/**.cpp', 'include/**.h', 'include/**.hpp' })

    filter('options:not for_unreal')
    do
        cppdialect('C++11')
        fatalwarnings { "All" }
    end

    filter({ 'options:for_unreal' })
    do
        cppdialect('C++17')
        defines({ '_DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR' })
    end

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

    filter({ 'system:macosx', 'options:variant=runtime' })
    do
        buildoptions({
            '-Wimplicit-float-conversion -fembed-bitcode -arch arm64 -arch x86_64 -isysroot '
                .. (os.getenv('MACOS_SYSROOT') or ''),
        })
    end

    filter('system:windows')
    do
        architecture('x64')
        defines({ '_USE_MATH_DEFINES' })
    end
    
    if _OPTIONS['with_optick'] then
      includedirs({optick .. '/src' })
    end

    filter('system:macosx or system:ios')
    do
        files({ 'src/text/font_hb_apple.mm' })
    end

    if TESTING == true then
        filter({ 'toolset:not msc' })
        do
            buildoptions({ '-Wshorten-64-to-32', '-fprofile-instr-generate', '-fcoverage-mapping' })
        end
    end
    filter({ 'options:with_rive_scripting' })
    do
        includedirs({
            luau .. '/VM/include',
        })
    end
end

newoption({
    trigger = 'with_rive_tools',
    description = 'Enables tools usually not necessary for runtime.',
})

newoption({
    trigger = 'with_rive_scripting',
    description = 'Enables scripting for the runtime.',
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
