dofile('rive_build_config.lua')

-- Are we in the "rive-renderer" or "rive" repository?
local handle = io.popen('git remote -v')
local git_remote = handle:read('*a')
handle:close()
if string.find(git_remote, 'rive%-renderer') then
    -- In rive-renderer. Rive runtime is a submodule.
    RIVE_RUNTIME_DIR = path.getabsolute('submodules/rive-cpp')
else
    -- In rive. Rive runtime is further up the tree.
    RIVE_RUNTIME_DIR = path.getabsolute('../runtime')
end

filter('system:windows or macosx or linux')
do
    -- Define RIVE_DESKTOP_GL outside of a project so that it also gets defined for consumers. It is
    -- the responsibility of consumers to call gladLoadCustomLoader() when RIVE_DESKTOP_GL is
    -- defined.
    defines({ 'RIVE_DESKTOP_GL' })
end

filter('system:android')
do
    defines({ 'RIVE_GLES' })
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

newoption({
    trigger = 'obfuscate',
    description = 'force-include obfuscator/pls_renames.h to obfuscate PLS variable names',
})
filter({ 'options:obfuscate' })
do
    forceincludes({ 'obfuscator/pls_renames.h' })
end

filter({})

-- Minify PLS shaders, and precompile them into metal libraries if targeting ios or macosx.
newoption({
    trigger = 'human-readable-shaders',
    description = 'don\'t minimize or obfuscate shaders',
})
project('rive_pls_shaders')
do
    kind('Makefile')
    local makecommand = '@make -C ' .. path.getabsolute('renderer/shaders')
    if _OPTIONS['human-readable-shaders'] then
        makecommand = makecommand .. ' FLAGS=--human-readable'
    end

    cleancommands({ makecommand .. ' clean' })
    buildcommands({ makecommand .. ' minify' })
    rebuildcommands({ makecommand .. ' minify' })

    filter('system:macosx')
    do
        buildcommands({ makecommand .. ' rive_pls_macosx_metallib' })
        rebuildcommands({ makecommand .. ' rive_pls_macosx_metallib' })
    end

    filter({ 'system:ios', 'options:variant=system' })
    do
        buildcommands({ makecommand .. ' rive_pls_ios_metallib' })
        rebuildcommands({ makecommand .. ' rive_pls_ios_metallib' })
    end

    filter({ 'system:ios', 'options:variant=emulator' })
    do
        buildcommands({ makecommand .. ' rive_pls_ios_simulator_metallib' })
        rebuildcommands({ makecommand .. ' rive_pls_ios_simulator_metallib' })
    end

    filter({ 'options:with-dawn or with-webgpu' })
    do
        buildcommands({ makecommand .. ' spirv' })
        rebuildcommands({ makecommand .. ' spirv' })
    end

    filter('system:windows')
    do
        architecture('x64')
    end
end

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
    dependson('rive_pls_shaders')
    kind('StaticLib')
    includedirs({ 'include', 'glad', 'renderer', RIVE_RUNTIME_DIR .. '/include' })
    flags({ 'FatalWarnings' })

    files({ 'renderer/*.cpp', 'renderer/decoding/*.cpp' })

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

    filter('system:not ios')
    do
        files({
            'renderer/gl/gl_state.cpp',
            'renderer/gl/gl_utils.cpp',
            'renderer/gl/load_store_actions_ext.cpp',
            'renderer/gl/pls_render_buffer_gl_impl.cpp',
            'renderer/gl/pls_render_context_gl_impl.cpp',
            'renderer/gl/pls_render_target_gl.cpp',
        })
    end

    filter('system:windows or macosx or linux')
    do
        files({
            'renderer/gl/pls_impl_webgl.cpp', -- Emulate WebGL with ANGLE.
            'renderer/gl/pls_impl_rw_texture.cpp',
            'glad/glad.c',
            'glad/glad_custom.c',
        }) -- GL loader library for ANGLE.
    end

    filter('system:android')
    do
        files({
            'renderer/gl/load_gles_extensions.cpp',
            'renderer/gl/pls_impl_ext_native.cpp',
            'renderer/gl/pls_impl_framebuffer_fetch.cpp',
        })
    end

    filter({ 'system:macosx or ios', 'options:not nop-obj-c' })
    do
        files({ 'renderer/metal/*.mm' })
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
            'renderer/webgpu/**.cpp',
            'renderer/gl/load_store_actions_ext.cpp',
        })
    end

    filter({ 'options:nop-obj-c' })
    do
        files({ 'renderer/metal/pls_metal_nop.cpp' })
    end

    filter({ 'options:not no-rive-decoders' })
    do
        includedirs({ RIVE_RUNTIME_DIR .. '/decoders/include' })
        defines({ 'RIVE_DECODERS' })
    end

    filter('system:windows')
    do
        architecture('x64')
        files({ 'renderer/d3d/*.cpp' })
    end

    filter('system:emscripten')
    do
        files({ 'renderer/gl/pls_impl_webgl.cpp' })
        buildoptions({ '-pthread' })
    end
end
