dofile('rive_build_config.lua')

RIVE_RUNTIME_DIR = path.getabsolute('..')

newoption({
    trigger = 'with_vulkan',
    description = 'compile with support for vulkan',
})
-- Guard this in an "if" (instead of filter()) so we don't download these repos when not building
-- for Vulkan.
if _OPTIONS['with_vulkan'] then
    local dependency = require('dependency')
    -- Standardize on the same set of Vulkan headers on all platforms.
    vulkan_headers = dependency.github('KhronosGroup/Vulkan-Headers', 'vulkan-sdk-1.3.283')
    vulkan_memory_allocator = dependency.github(
        'GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator',
        '7942b798289f752dc23b0a79516fd8545febd718'
    )
    defines({
        'RIVE_VULKAN',
        'VK_NO_PROTOTYPES',
        'VMA_STATIC_VULKAN_FUNCTIONS=0',
        'VMA_DYNAMIC_VULKAN_FUNCTIONS=1',
    })
end

filter('system:windows or macosx or linux')
do
    -- Define RIVE_DESKTOP_GL outside of a project so that it also gets defined for consumers. It is
    -- the responsibility of consumers to call gladLoadCustomLoader() when RIVE_DESKTOP_GL is
    -- defined.
    defines({ 'RIVE_DESKTOP_GL' })
end

filter('system:macosx')
do
    defines({ 'RIVE_MACOSX' })
end

filter('system:android')
do
    defines({ 'RIVE_ANDROID' })
end

filter({ 'system:ios', 'options:variant=system' })
do
    defines({ 'RIVE_IOS' })
end

newoption({
    trigger = 'with-dawn',
    description = 'compile in support for webgpu via dawn',
})
filter({ 'options:with-dawn' })
do
    defines({ 'RIVE_DAWN' })
end

newoption({
    trigger = 'with-webgpu',
    description = 'compile in native support for webgpu',
})
filter({ 'options:with-webgpu' })
do
    defines({ 'RIVE_WEBGPU' })
end

filter({ 'system:ios', 'options:variant=emulator' })
do
    defines({ 'RIVE_IOS_SIMULATOR' })
end

filter('system:emscripten')
do
    defines({ 'RIVE_WEBGL' })
end

filter({})

-- Minify and compile PLS shaders offline.
local pls_generated_headers = RIVE_BUILD_OUT .. '/include'
local pls_shaders_absolute_dir = path.getabsolute(pls_generated_headers .. '/generated/shaders')
local makecommand = 'make -C '
    .. path.getabsolute('src/shaders')
    .. ' OUT='
    .. pls_shaders_absolute_dir

newoption({
    trigger = 'raw_shaders',
    description = 'don\'t rename shader variables, or remove whitespace or comments',
})
if _OPTIONS['raw_shaders'] then
    makecommand = makecommand .. ' FLAGS=--human-readable'
end

if os.host() == 'macosx' then
    if _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'system' then
        makecommand = makecommand .. ' rive_pls_ios_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'emulator' then
        makecommand = makecommand .. ' rive_pls_ios_simulator_metallib'
    else
        makecommand = makecommand .. ' rive_pls_macosx_metallib'
    end
end

if _OPTIONS['with_vulkan'] or _OPTIONS['with-dawn'] or _OPTIONS['with-webgpu'] then
    makecommand = makecommand .. ' spirv'
end

function execute_and_check(cmd)
    if not os.execute(cmd) then
        error('\nError executing command:\n  ' .. cmd)
    end
end

-- Wipe out the shader directory if this make command differs from the one that built it.
local existing_makecommand = nil
local makecommand_filename = pls_shaders_absolute_dir .. '/./.makecommand'
local makecommand_file = io.open(makecommand_filename)
if makecommand_file then
    existing_makecommand = makecommand_file:read('*all')
    -- Trim whitespace
    existing_makecommand = string.gsub(existing_makecommand, '^%s*(.-)%s*$', '%1')
    makecommand_file:close()
end
if not existing_makecommand or existing_makecommand ~= makecommand then
    if existing_makecommand then
        print('"make" command for PLS shaders differs from before: cleaning...')
    end
    execute_and_check('rm -fr ' .. pls_shaders_absolute_dir)
end

-- Build shaders.
execute_and_check(makecommand)

-- Save the make command for incremental shader builds.
execute_and_check('echo ' .. makecommand .. ' > ' .. makecommand_filename)

newoption({
    trigger = 'nop-obj-c',
    description = 'include Metal classes, but as no-ops (for compilers that don\'t support Obj-C)',
})
newoption({
    trigger = 'no-rive-decoders',
    description = 'don\'t use the rive_decoders library (built-in image decoding will fail)',
})
newoption({
    trigger = 'universal-release',
    description = '(Apple only): build a universal binary to release to the store',
})
project('rive_pls_renderer')
do
    kind('StaticLib')
    includedirs({
        'include',
        'glad',
        'src',
        '../include',
        pls_generated_headers,
    })
    flags({ 'FatalWarnings' })

    files({ 'src/*.cpp', 'renderer/decoding/*.cpp' })

    if _OPTIONS['with_vulkan'] then
        externalincludedirs({
            vulkan_headers .. '/include',
            vulkan_memory_allocator .. '/include',
        })
        files({ 'src/vulkan/*.cpp' })
    end

    filter({ 'toolset:not msc' })
    do
        buildoptions({ '-Wshorten-64-to-32' })
    end

    filter({
        'system:windows',
        'options:toolset=msc',
        'options:with-dawn or with-webgpu or with_vulkan',
    })
    do
        -- Vulkan and WebGPU both make heavy use of designated initializers, which MSVC doesn't accept in C++17.
        cppdialect('c++latest')
        defines({
            '_SILENCE_CXX20_IS_POD_DEPRECATION_WARNING',
            '_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS',
        })
    end

    -- The Visual Studio clang toolset doesn't recognize -ffp-contract.
    filter('system:not windows')
    do
        buildoptions({
            '-ffp-contract=on',
            '-fassociative-math',
            -- Don't warn about simd vectors larger than 128 bits when AVX is not enabled.
            '-Wno-psabi',
        })
    end

    filter({ 'system:not ios' })
    do
        files({
            'src/gl/gl_state.cpp',
            'src/gl/gl_utils.cpp',
            'src/gl/load_store_actions_ext.cpp',
            'src/gl/render_buffer_gl_impl.cpp',
            'src/gl/render_context_gl_impl.cpp',
            'src/gl/render_target_gl.cpp',
        })
    end

    filter({ 'system:windows or macosx or linux' })
    do
        files({
            'src/gl/pls_impl_webgl.cpp', -- Emulate WebGL with ANGLE.
            'src/gl/pls_impl_rw_texture.cpp',
            'glad/glad.c',
            'glad/glad_custom.c',
        }) -- GL loader library for ANGLE.
    end

    filter('system:android')
    do
        files({
            'src/gl/load_gles_extensions.cpp',
            'src/gl/pls_impl_ext_native.cpp',
            'src/gl/pls_impl_framebuffer_fetch.cpp',
        })
    end

    filter({ 'system:macosx or ios', 'options:not nop-obj-c' })
    do
        files({ 'src/metal/*.mm' })
        buildoptions({ '-fobjc-arc' })
    end

    filter({ 'options:with-dawn' })
    do
        includedirs({
            'dependencies/dawn/include',
            'dependencies/dawn/out/release/gen/include',
        })
        files({ 'dependencies/dawn/out/release/gen/src/dawn/webgpu_cpp.cpp' })
    end

    filter({ 'options:with-webgpu or with-dawn' })
    do
        files({
            'src/webgpu/**.cpp',
            'src/gl/load_store_actions_ext.cpp',
        })
    end

    filter({ 'options:nop-obj-c' })
    do
        files({ 'src/metal/metal_nop.cpp' })
    end

    filter({ 'options:not no-rive-decoders' })
    do
        includedirs({ '../decoders/include' })
        defines({ 'RIVE_DECODERS' })
    end

    filter('system:windows')
    do
        architecture('x64')
        files({ 'src/d3d/*.cpp' })
    end

    filter('system:emscripten')
    do
        files({ 'src/gl/pls_impl_webgl.cpp' })
    end
end
