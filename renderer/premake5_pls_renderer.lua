dofile('rive_build_config.lua')

RIVE_RUNTIME_DIR = path.getabsolute('..')
local dependency = require('dependency')

newoption({
    trigger = 'with_vulkan',
    description = 'compile with support for vulkan',
})
-- Guard this in an "if" (instead of filter()) so we don't download these repos when not building
-- for Vulkan.
if _OPTIONS['with_vulkan'] then
    -- Standardize on the same set of Vulkan headers on all platforms.
    vulkan_headers = dependency.github('KhronosGroup/Vulkan-Headers', 'vulkan-sdk-1.4.321')
    vulkan_memory_allocator =
        dependency.github('GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator', 'v3.3.0')
    defines({
        'RIVE_VULKAN',
        'VK_NO_PROTOTYPES',
        'VMA_STATIC_VULKAN_FUNCTIONS=0',
        'VMA_DYNAMIC_VULKAN_FUNCTIONS=1',
    })
end

if rive_target_os == 'windows' then
    dx12_headers = dependency.github('microsoft/DirectX-Headers', 'v1.615.0')
end

filter('system:windows or macosx or linux')
do
    -- Define RIVE_DESKTOP_GL outside of a project so that it also gets defined for consumers. It is
    -- the responsibility of consumers to call gladLoadCustomLoader() when RIVE_DESKTOP_GL is
    -- defined.
    defines({ 'RIVE_DESKTOP_GL' })
end

-- Ore backend selection — defined globally so tests and consumers also see them.
-- Only active when --with_rive_canvas is enabled.
-- RIVE_ORE is defined whenever any ore backend is active, so C++ code can guard
-- ore API calls without enumerating every backend.
filter({ 'system:macosx or ios', 'options:with_rive_canvas' })
do
    defines({ 'ORE_BACKEND_METAL', 'RIVE_ORE' })
end

-- Enable @try/@catch in Metal ORE for graceful error handling at runtime.
-- Not used in tools/fuzz builds where crashing on Metal exceptions is preferred.
filter({ 'system:macosx or ios', 'options:with_objc_exceptions' })
do
    defines({ 'RIVE_OBJC_EXCEPTIONS=1' })
end

-- macOS also gets ORE_BACKEND_GL so ore GMs work with ANGLE (GL on desktop).
filter({ 'system:macosx', 'options:with_rive_canvas', 'options:not no_gl' })
do
    defines({ 'ORE_BACKEND_GL' })
end

filter({ 'system:windows', 'options:with_rive_canvas' })
do
    defines({ 'ORE_BACKEND_D3D11', 'ORE_BACKEND_D3D12', 'RIVE_ORE' })
end

-- Windows + Dawn: ore routes through Dawn's WebGPU device. Supersedes D3D11
-- and D3D12 ore backends.
filter({ 'system:windows', 'options:with_rive_canvas', 'options:with-dawn' })
do
    defines({ 'ORE_BACKEND_WGPU' })
    removedefines({ 'ORE_BACKEND_D3D11' })
    removedefines({ 'ORE_BACKEND_D3D12' })
end

filter('system:android')
do
    defines({ 'RIVE_ANDROID' })
end

filter({ 'system:android', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:not no_gl' })
do
    defines({ 'ORE_BACKEND_GL', 'RIVE_ORE' })
end

-- Android + wagyu: Ore routes through the WebGPU/wagyu device instead of raw GL.
-- Runtime dispatch inside ORE_BACKEND_WGPU selects GLSLRAW (GLES) or GLSL (Vulkan).
filter({ 'system:android', 'options:with_rive_canvas', 'options:with_wagyu' })
do
    defines({ 'ORE_BACKEND_WGPU', 'RIVE_ORE' })
end

-- Android + native Vulkan (no wagyu): raw Vulkan ORE backend.
-- Takes SPIR-V directly; traditional VkRenderPass for TBDR tile memory efficiency.
-- When GL is also enabled (the default), both backends coexist in the same binary
-- via VK+GL dispatch files (src/ore/vulkan/ore_*_vk_gl.cpp).
filter({ 'system:android', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:with_vulkan' })
do
    defines({ 'ORE_BACKEND_VK' })
end

-- Linux: native Vulkan ORE backend (requires --with_vulkan for headers and VMA).
-- Also enables the ORE GL backend so the same binary can service `--backend gl`
-- via the VK+GL dispatch TUs, parallel to the Android VK+GL configuration.
filter({ 'system:linux', 'options:with_rive_canvas', 'options:with_vulkan' })
do
    defines({ 'ORE_BACKEND_VK', 'ORE_BACKEND_GL', 'RIVE_ORE' })
end


newoption({
    trigger = 'with_objc_exceptions',
    description = 'enable @try/@catch in Metal ORE (requires -fobjc-exceptions)',
})



newoption({
    trigger = 'with-dawn',
    description = 'compile in support for webgpu via dawn',
})

filter({ 'options:with-dawn' })
do
    defines({ 'RIVE_DAWN' })
end

filter('system:emscripten')
do
    defines({ 'RIVE_WEBGL' })
end

-- Emscripten + wagyu: Ore routes through the WebGPU/wagyu device.
-- Runtime dispatch inside ORE_BACKEND_WGPU selects GLSLRAW (GLES) or GLSL (Vulkan).
filter({ 'system:emscripten', 'options:with_rive_canvas', 'options:with_wagyu' })
do
    defines({ 'ORE_BACKEND_WGPU', 'RIVE_ORE' })
end

filter({ 'system:emscripten', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:not no_gl' })
do
    defines({ 'ORE_BACKEND_GL', 'RIVE_ORE' })
end

newoption({
    trigger = 'with-webgpu',
    description = 'compile in native support for webgpu',
})
newoption({
    trigger = 'webgpu-version',
    description = 'which version of webgpu to compile for',
    allowed = { { '1' }, { '2' } },
    default = '1',
})
filter({ 'options:with-webgpu' })
do
    defines({ 'RIVE_WEBGPU=' .. _OPTIONS['webgpu-version'] })
end

filter({})

newoption({
    trigger = 'with_wagyu',
    description = 'compile in support for wagyu webgpu extensions',
})
if _OPTIONS['with_wagyu'] then
    if _OPTIONS['webgpu-version'] < '2' then
        error('\nWagyu does not support webgpu-version < 2\n  ')
    end
    defines({ 'RIVE_WAGYU' })
    RIVE_WAGYU_PORT = '--use-port='
        .. RIVE_RUNTIME_DIR
        .. '/renderer/src/webgpu/wagyu-port/webgpu-port.py:wagyu=true'
end

newoption({
    trigger = 'no_gl',
    description = 'do not compile in support for opengl',
})

-- Minify and compile PLS shaders offline.
pls_generated_headers = RIVE_BUILD_OUT .. '/include'
local pls_shaders_absolute_dir = path.getabsolute(pls_generated_headers .. '/generated/shaders')
local nproc
if os.host() == 'windows' then
    nproc = os.getenv('NUMBER_OF_PROCESSORS')
elseif os.host() == 'macosx' then
    local handle = io.popen('sysctl -n hw.physicalcpu')
    nproc = handle:read('*a')
    handle:close()
else
    local handle = io.popen('nproc')
    nproc = handle:read('*a')
    handle:close()
end
nproc = nproc:gsub('%s+', '') -- remove whitespace
local python_ply = dependency.github('dabeaz/ply', '3.11')
local makecommand = 'make -C '
    .. path.getabsolute('src/shaders')
    .. ' -j'
    .. nproc
    .. ' OUT='
    .. pls_shaders_absolute_dir

local minify_flags = '-p ' .. python_ply
newoption({
    trigger = 'raw_shaders',
    description = 'don\'t rename shader variables, or remove whitespace or comments',
})

if _OPTIONS['raw_shaders'] then
    minify_flags = minify_flags .. ' --human-readable'
    defines({ 'RIVE_RAW_SHADERS' })

    -- MSVC cannot import the raw shaders as strings becasue they are too long,
    -- so signal to the minify script to use an alternate mode where it writes
    -- out as a char array instead.
    if _OPTIONS['toolset'] == 'msc' then
        minify_flags = minify_flags .. ' --msvc'
    end
end



makecommand = makecommand .. ' FLAGS="' .. minify_flags .. '"'

if os.host() == 'macosx' then
    if _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'system' then
        makecommand = makecommand .. ' rive_pls_ios_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'emulator' then
        makecommand = makecommand .. ' rive_pls_ios_simulator_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'xros' then
        makecommand = makecommand .. ' rive_renderer_xros_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'xrsimulator' then
        makecommand = makecommand .. ' rive_renderer_xros_simulator_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'appletvos' then
        makecommand = makecommand .. ' rive_renderer_appletvos_metallib'
    elseif _OPTIONS['os'] == 'ios' and _OPTIONS['variant'] == 'appletvsimulator' then
        makecommand = makecommand .. ' rive_renderer_appletvsimulator_metallib'
    else
        makecommand = makecommand .. ' rive_pls_macosx_metallib'
    end
end

if rive_target_os == 'windows' then
    makecommand = makecommand .. ' d3d'
end

if _OPTIONS['with_vulkan'] or _OPTIONS['with-dawn'] or _OPTIONS['with-webgpu'] then
    makecommand = makecommand .. ' spirv'
end

function execute_and_check(cmd)
    print(cmd)
    if not os.execute(cmd) then
        error('\nError executing command:\n  ' .. cmd)
    end
end

-- Build shaders.
execute_and_check(makecommand)

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
newoption({
    trigger = 'no_ffp_contract',
    description = 'exclue -ffp-contract=on and -fassociative-math from builoptions',
})

if _OPTIONS['with_optick'] then
    optick = dependency.github(RIVE_OPTICK_URL, RIVE_OPTICK_VERSION)
end

if _OPTIONS['with_microprofile'] then
    microprofile = dependency.github(RIVE_MICROPROFILE_URL, RIVE_MICROPROFILE_VERSION)
end

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
    externalincludedirs({'glad/include'})
    fatalwarnings({ 'All' })

    files({ 'src/*.cpp', 'src/*.hpp', 'src/*.h', 'src/shaders/*.glsl',
            'src/shaders/*.frag', 'src/shaders/*.vert', 'include/**.hpp', 'include/**.h',
            'src/ore/ore_binding_map.cpp',
            'src/ore/ore_bind_group_layout.cpp' })


    if _OPTIONS['with_optick'] then
        includedirs({optick .. '/src'})
    end

    if _OPTIONS['with_microprofile'] then
        includedirs({ microprofile })
    end

    if _OPTIONS['with_vulkan'] then
        externalincludedirs({
            vulkan_headers .. '/include',
            vulkan_memory_allocator .. '/include',
        })
        files({ 'src/vulkan/*.cpp' })
    end

    if rive_target_os == 'windows' then
        externalincludedirs({
            dx12_headers .. '/include/directx',
        })
    end

    filter({ 'toolset:not msc' })
    do
        buildoptions({ '-Wshorten-64-to-32' })
    end

    -- The Visual Studio clang toolset doesn't recognize -ffp-contract.
    filter({ 'system:not windows', 'options:not no_ffp_contract' })
    do
        buildoptions({
            '-ffp-contract=on',
            '-fassociative-math',
            -- Don't warn about simd vectors larger than 128 bits when AVX is not enabled.
            '-Wno-psabi',
        })
    end

    filter({ 'system:not ios', 'options:not no_gl' })
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
            'glad/src/egl.c',
            'glad/src/gles2.c',
            'glad/glad_custom.c',
        }) -- GL loader library for ANGLE.
    end

    filter('system:android')
    do
        files({
            'src/gl/load_gles_extensions.cpp',
            'src/gl/pls_impl_ext_native.cpp',
        })
    end

    filter({ 'system:macosx or ios', 'options:not nop-obj-c' })
    do
        files({ 'src/metal/*.mm' })
        buildoptions({ '-fobjc-arc' })
    end

    -- Shared cross-backend Ore sources. Compiled whenever canvas is on,
    -- regardless of which GPU backend(s) are active.
    filter({ 'options:with_rive_canvas' })
    do
        files({ 'src/ore/*.cpp' })
    end

    filter({ 'system:macosx or ios', 'options:not nop-obj-c', 'options:with_rive_canvas' })
    do
        files({ 'src/ore/metal/*.mm' })
    end

    -- Enable -fobjc-exceptions for .mm files when @try/@catch is active.
    -- Uses a files: filter so the flag doesn't hit .cpp compilations
    -- (clang warns on -fobjc-exceptions for non-ObjC++ sources).
    filter({
        'system:macosx or ios',
        'options:not nop-obj-c',
        'options:with_objc_exceptions',
        'files:**.mm',
    })
    do
        buildoptions({ '-fobjc-exceptions' })
    end

    -- Enable -fobjc-exceptions for .mm files when @try/@catch is active.
    -- Uses a files: filter so the flag doesn't hit .cpp compilations
    -- (clang warns on -fobjc-exceptions for non-ObjC++ sources).
    filter({
        'system:macosx or ios',
        'options:not nop-obj-c',
        'options:with_objc_exceptions',
        'files:**.mm',
    })
    do
        buildoptions({ '-fobjc-exceptions' })
    end


    -- macOS also compiles the ore GL backend (via ObjC++ wrappers) so ore
    -- works with ANGLE. Both METAL and GL backends coexist in the same binary.
    filter({ 'system:macosx', 'options:not nop-obj-c', 'options:with_rive_canvas', 'options:not no_gl' })
    do
        files({ 'src/ore/gl/*.mm' })
    end


    filter({ 'system:windows', 'options:with_rive_canvas' })
    do
        files({ 'src/ore/d3d11/*.cpp' })
        files({ 'src/ore/d3d12/*.cpp' })
    end

    -- Windows + Dawn: ore uses the WebGPU device exposed by Dawn. Supersedes
    -- D3D11 and D3D12 ore backends — Dawn provides the GPU abstraction.
    filter({ 'system:windows', 'options:with_rive_canvas', 'options:with-dawn' })
    do
        files({ 'src/ore/wgpu/*.cpp' })
        removefiles({ 'src/ore/d3d11/*.cpp' })
        removefiles({ 'src/ore/d3d12/*.cpp' })
    end


    filter({ 'system:android', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:not with_vulkan', 'options:not no_gl' })
    do
        files({ 'src/ore/gl/*.cpp' })
    end

    filter({ 'system:android', 'options:with_rive_canvas', 'options:with_wagyu' })
    do
        files({ 'src/ore/wgpu/*.cpp' })
    end

    filter({ 'system:emscripten', 'options:with_rive_canvas', 'options:with_wagyu' })
    do
        files({ 'src/ore/wgpu/*.cpp' })
    end

    -- Android VK: always include Vulkan backend files.
    filter({ 'system:android', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:with_vulkan' })
    do
        files({ 'src/ore/vulkan/*.cpp' })
    end

    -- Android VK+GL: both backends coexist. Standalone files compile to
    -- static-helper-only objects (method bodies guarded out by the other
    -- backend's define). VK+GL dispatch files (ore_*_vk_gl.cpp) in the
    -- vulkan/ directory provide the actual method implementations.
    filter({ 'system:android', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:with_vulkan', 'options:not no_gl' })
    do
        files({ 'src/ore/gl/*.cpp' })
    end

    if _OPTIONS['with_vulkan'] then
        filter({ 'system:linux', 'options:with_rive_canvas' })
        do
            externalincludedirs({
                vulkan_headers .. '/include',
                vulkan_memory_allocator .. '/include',
            })
            files({ 'src/ore/vulkan/*.cpp', 'src/ore/gl/*.cpp' })
        end
    end

    filter({ 'system:emscripten', 'options:with_rive_canvas', 'options:not with_wagyu', 'options:not no_gl' })
    do
        files({ 'src/ore/gl/*.cpp' })
    end

    filter({ 'options:with-dawn' })
    do
        includedirs({
            'dependencies/dawn/include',
            'dependencies/dawn/out/release/gen/include',
        })
    end

    filter({ 'options:with-webgpu or with-dawn' })
    do
        files({
--Don 't use "**.cpp" because we don' t want to compile files in-- wagyu - port/**
            'src/webgpu/*.cpp',
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
        files({ 'src/d3d11/*.cpp' })
        files({ 'src/d3d12/*.cpp' })
    end

    filter('system:emscripten')
    do
        files({ 'src/gl/pls_impl_webgl.cpp' })
    end

    filter({})

    if RIVE_WAGYU_PORT then
        buildoptions({ RIVE_WAGYU_PORT })
        linkoptions({ RIVE_WAGYU_PORT })
    end
end
