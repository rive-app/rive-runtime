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
-- The wasm backend carries no Luau in any capacity; every other backend
-- keeps it, and 'both' runs the two side by side for differential tools.
if _OPTIONS['scripting_vm'] ~= 'wasm' then
    filter({ 'options:with_rive_scripting' })
    do
        defines({ 'WITH_RIVE_SCRIPTING_LUAU' })
    end
    filter({})
end
-- Workspace scope: gated members change class layout, so every project
-- must agree on the define.
if _OPTIONS['scripting_vm'] == 'wasm' or _OPTIONS['scripting_vm'] == 'both'
then
    filter({ 'options:with_rive_scripting' })
    do
        defines({ 'WITH_RIVE_SCRIPTING_WASM' })
    end
    filter({})
    if _OPTIONS['wasm_hw_bounds'] then
        filter({ 'options:with_rive_scripting' })
        do
            defines({ 'RIVE_WASM_HW_BOUNDS' })
        end
        filter({})
    end
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
filter({ 'options:track_rive_shader_id' })
do
    -- Stores ShaderAsset::assetId() on each ore::ShaderModule.
    defines({ 'TRACK_RIVE_SHADER_ID' })
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
filter({ 'options:with_rive_editor' })
do
    defines({ 'WITH_RIVE_EDITOR' })
    -- Generated runtime bases include their editor extension `.inl` from the
    -- kernel tree, so every project that sees a generated header needs the
    -- kernel include root, not just the `rive` library.
    includedirs({ path.getabsolute('../editor_native/kernel/include') })
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
    if _OPTIONS['scripting_vm'] == 'wasm' or _OPTIONS['scripting_vm'] == 'both'
    then
        local wamrLib =
            require(path.join(path.getabsolute('scripting/'), 'premake5_wamr'))
        wamr = wamrLib.wamr
        wamrConfigDefines = wamrLib.configDefines
        wamrInternalIncludes = wamrLib.internalIncludes
    end
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

    -- ORE headers conditionally include <vulkan/vulkan.h> when
    -- ORE_BACKEND_VK is defined; the rive base library transitively pulls
    -- those headers in, so the Vulkan SDK paths need to be visible here too.
    if _OPTIONS['with_vulkan'] and _OPTIONS['with_rive_canvas'] then
        local dependency = require('dependency')
        local vh = dependency.github('KhronosGroup/Vulkan-Headers',
                                     'vulkan-sdk-1.4.321')
        local vma = dependency.github(
            'GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator',
            'v3.3.0')
        externalincludedirs({ vh .. '/include', vma .. '/include' })
    end

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

    -- TODO: remove once simple_array.hpp migrates off std::is_pod.
    -- Console toolchains OOM on the ~18k deprecation warnings otherwise.
    filter({ 'options:_console_only_ore_vk' })
    do
        buildoptions({ '-Wno-deprecated-declarations' })
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
            libhydrogen,
        })
        files({
            libhydrogen .. '/libhydrogen.c',
        })
    end
    if _OPTIONS['scripting_vm'] ~= 'wasm' then
        filter({ 'options:with_rive_scripting' })
        do
            includedirs({ luau .. '/VM/include' })
        end
    end
    if wamr then
        filter({ 'options:with_rive_scripting' })
        do
            includedirs({ wamr .. '/core/iwasm/include' })
            -- The tier transplant reads instance internals; layout hinges
            -- on the same config defines the wamr lib builds with.
            includedirs(wamrInternalIncludes)
            defines(wamrConfigDefines)
            filter({ 'options:with_rive_scripting', 'system:macosx' })
            defines({ 'BH_PLATFORM_DARWIN' })
            filter({ 'options:with_rive_scripting', 'system:linux' })
            defines({ 'BH_PLATFORM_LINUX' })
            filter({ 'options:with_rive_scripting', 'system:windows' })
            defines({ 'BH_PLATFORM_WINDOWS', 'HAVE_STRUCT_TIMESPEC' })
            filter({
                'options:with_rive_scripting',
                'system:windows',
                'files:**/wamr_state_transplant.cpp',
            })
            -- platform_common.h hardcodes __declspec on BH_MALLOC while our
            -- static-link wasm_export.h declares it plain.
            buildoptions({ '-Wno-dll-attribute-on-redeclaration' })
            filter({ 'options:with_rive_scripting' })
        end
        filter({})
    end
    filter({ 'options:with_rive_canvas' })
    do
        -- lua_gpu.cpp and lua_scripted_context.cpp include renderer (C++17) headers.
        includedirs({ 'renderer/include' })
    end
    filter({ 'options:with_rive_canvas', 'options:not cpp20' })
    do
        -- We need to also set the dialect to C++17 when building with Rive
        -- canvas, unless we're also setting it to C++20 (in which case we
        -- want to keep that preference)
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
        -- =1 required; an empty define evaluates to 0 on strict preprocessors.
        defines({ 'HYDRO_SIGN_VERIFY_ONLY=1' })
    end
    if _OPTIONS['scripting_vm'] == 'wasm' then
        filter({})
        removefiles({ 'src/lua/**' })
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
    trigger = 'scripting_vm',
    value = 'VM',
    description = 'Scripting execution backend.',
    allowed = {
        { 'luau', 'Native Luau VM (default)' },
        { 'wasm', 'WAMR executing wasm script modules, no Luau in the build' },
        { 'both', 'Both backends, for differential tools' },
    },
    default = 'luau',
})

newoption({
    trigger = 'wasm_hw_bounds',
    description = 'WAMR hardware bounds checks for wasm modules compiled '
        .. 'without sw bounds (AssemblyScript); Luau artifacts stay sw. '
        .. '64-bit desktop/mobile only: reserves 8GB address space per '
        .. 'module memory and installs a SIGSEGV/SIGBUS handler.',
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
    trigger = 'with_rive_editor',
    description = 'Enables editor-mode hooks (onPropertyChanging, applyChange, arena). '
        .. 'Defined only by editor_native — never by runtime SDK consumers.',
})

newoption({
    trigger = 'with_rive_docs',
    description = 'Indicates building for use with the docs generator.',
})

newoption({
    trigger = 'track_rive_shader_id',
    description = 'Stores ShaderAsset::assetId() on each ore::ShaderModule.',
})
