dofile('rive_build_config.lua')

filter({ 'options:with_rive_tools' })
do
    defines({ 'WITH_RIVE_TOOLS' })
end
filter({ 'options:with_rive_text' })
do
    defines({ 'WITH_RIVE_TEXT' })
end
filter({ 'options:with_rive_canvas' })
do
    defines({ 'RIVE_CANVAS' })
end
filter({ 'options:with_rive_scripting' })
do
    defines({ 'WITH_RIVE_SCRIPTING' })
end
filter({ 'options:with_rive_test_signature' })
do
    -- Swaps `g_scriptVerificationPublicKey` for the public key that
    -- corresponds to `SampleSigningContext._samplePrivateKey` in Dart,
    -- so .riv files signed locally via the sample keypair verify.
    -- NEVER enable on shipping builds — it accepts .rivs any attacker
    -- could produce.
    defines({ 'WITH_RIVE_TEST_SIGNATURE' })
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

if _OPTIONS['with_microprofile'] then
    dofile(path.join(dependencies, 'premake5_microprofile.lua'))
end

if _OPTIONS['with_rive_scripting'] then
    local scripting = require(path.join(path.getabsolute('scripting/'), 'premake5'))
    luau = scripting.luau
    libhydrogen = scripting.libhydrogen
else
    project('luau_vm')
    do
        kind('StaticLib')
        files({ 'dummy.cpp' })
    end
    luau = ''
    libhydrogen = ''
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

    if _OPTIONS['with_microprofile'] then
      includedirs({microprofile})
    end

    filter('action:xcode4')
    do
        -- xcode doesnt like angle brackets except for -isystem
        -- should use externalincludedirs but GitHub runners dont have latest premake5 binaries
        buildoptions({ '-isystem' .. yoga })
    end
    filter({})

    defines({ 'YOGA_EXPORT=', '_RIVE_INTERNAL_' })

    files({ 'src/**.cpp', 'include/**.h', 'include/**.hpp' })

    filter({'toolset:msc' })
    do
        -- add a debug visualizer for various runtime types for MSVC debugging.
        -- (the visualization only works with MSVC-compiled projects, Clang-
        -- built projects don't work)
        files({ 'runtime.natvis' })
    end

    filter('options:not for_unreal')
    do
        fatalwarnings({ 'All' })
    end

    filter({'options:for_unreal'})
    do
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

    filter({})
    
    if _OPTIONS['with_optick'] then
        includedirs({ optick .. '/src' })
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
    filter({ 'options:with_rive_scripting', 'options:not no-rive-decoders' })
    do
        defines({ 'RIVE_DECODERS' })
        includedirs({ 'decoders/include' })
    end
    filter({ 'options:with_rive_scripting' })
    do
        includedirs({
            luau .. '/VM/include',
            libhydrogen,
        })
        files({
            libhydrogen .. '/libhydrogen.c',
        })
    end
    filter({ 'options:with_rive_canvas' })
    do
        -- lua_gpu.cpp and lua_scripted_context.cpp include renderer (C++17) headers.
        includedirs({ 'renderer/include' })
        cppdialect('C++17')
    end
    -- On Apple, ore_context.hpp imports Metal.h (ORE_BACKEND_METAL is globally
    -- defined), which requires ObjC++ compilation. Swap .cpp files for .mm
    -- wrappers so they are compiled as ObjC++.
    -- Also undefine the GL globals that bleed in from pls_renderer.lua —
    -- the runtime rive project only uses Metal on macOS/iOS.
    filter({ 'system:macosx or system:ios', 'options:with_rive_canvas' })
    do
        -- Swap .cpp for .mm wrappers: ore_context.hpp imports <Metal/Metal.h>
        -- (via ORE_BACKEND_METAL) which is only valid in ObjC++ files.
        removefiles({ 'src/lua/lua_scripted_context.cpp' })
        removefiles({ 'src/lua/renderer/lua_gpu.cpp' })
        files({ 'src/lua/lua_scripted_context_apple.mm' })
        files({ 'src/lua/renderer/lua_gpu_apple.mm' })
    end
    filter({ 'options:with_rive_scripting', 'options:not with_rive_tools' })
    do
        -- at runtime we only need signature verification
        defines({ 'HYDRO_SIGN_VERIFY_ONLY' })
    end
    filter({
        'options:with_rive_scripting',
        'options:config=release',
        'system:android',
        'options:arch=arm',
        'files:**/libhydrogen.c'
    })
    do
        -- Android ARMv7 devices running API < 29 cannot load ELF TLS relocations
        -- (e.g. R_ARM_TLS_*). In release builds with scripting, libhydrogen.c can
        -- introduce these TLS relocations (through `__thread` for RNG) when LTO
        -- is enabled, which then fails to link at runtime (`unknown reloc type 17`).

        -- We want to keep global LTO for performance, but compile only this
        -- translation unit without LTO, forcing emulated TLS so that the final
        -- ARMv7 librive-android.so does not include unsupported ELF TLS relocations.
        buildoptions({'-fno-lto', '-femulated-tls'})
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
    trigger = 'with_rive_test_signature',
    description = 'Test-only: accept .riv files signed by the Dart '
        .. 'SampleSigningContext keypair. Do not enable on shipping builds.',
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

newoption({
    trigger = 'with_rive_canvas',
    description = 'Compiles in RenderCanvas and Ore GPU abstraction layer.',
})

newoption({
    trigger = 'with_rive_docs',
    description = 'Indicates building for use with the docs generator.',
})
