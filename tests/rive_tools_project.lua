dofile('rive_build_config.lua')
defines({ 'WITH_RIVE_TOOLS' })

RIVE_RUNTIME_DIR = path.getabsolute('..')
RIVE_PLS_DIR = path.getabsolute('../renderer')

dofile(RIVE_RUNTIME_DIR .. '/premake5_v2.lua')
dofile(RIVE_RUNTIME_DIR .. '/cg_renderer/premake5.lua')
dofile(RIVE_RUNTIME_DIR .. '/dependencies/premake5_libpng_v2.lua')
dofile(RIVE_RUNTIME_DIR .. '/decoders/premake5_v2.lua')
dofile(RIVE_PLS_DIR .. '/premake5_pls_renderer.lua')

newoption({ trigger = 'with-skia', description = 'use skia' })
if _OPTIONS['with-skia'] then
    dofile(RIVE_RUNTIME_DIR .. '/skia/renderer/premake5_v2.lua')
end

function rive_tools_project(name, project_kind)
    project(name)
    if project_kind == 'RiveTool' then
        kind(
            _OPTIONS['for_unreal'] and 'StaticLib'
                or _OPTIONS['for_android'] and 'SharedLib'
                or _OPTIONS['os'] == 'ios' and 'StaticLib'
                or 'ConsoleApp'
        )

        dependson({
            'tools_common',
            'rive_pls_renderer',
            'rive_cg_renderer',
            'rive_decoders',
            'rive',
            'libpng',
            'zlib',
            'libjpeg',
            'libwebp',
            'rive_yoga',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'miniaudio',
        })

        filter({ 'system:windows', 'options:with-asan', 'options:toolset=clang' })
        do
            links({
                'clang_rt.asan_dbg_dynamic-x86_64.lib',
                'clang_rt.asan_dynamic_runtime_thunk-x86_64.lib',
            })
        end
        filter({})
    else
        kind(project_kind)
    end

    fatalwarnings { "All" }

    defines({
        'SK_GL',
        'GL_SILENCE_DEPRECATION', -- For glReadPixels()
        'YOGA_EXPORT=',
    })

    includedirs({
        '.',
        RIVE_PLS_DIR .. '/include',
        RIVE_PLS_DIR .. '/path_fiddle',
        RIVE_PLS_DIR .. '/shader_hotload',
        RIVE_PLS_DIR .. '/src',
        RIVE_RUNTIME_DIR .. '/include',
        RIVE_RUNTIME_DIR .. '/cg_renderer/include',
        'unit_tests',
        '%{cfg.targetdir}/include/libpng',
    })

    externalincludedirs({
        'include',
        RIVE_PLS_DIR .. '/glad',
        RIVE_PLS_DIR .. '/glad/include',
        RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw/include',
        yoga,
        libpng,
        zlib,
    })

    if ndk then
        externalincludedirs({ ndk .. '/sources' })
        links({ 'log', 'android' })
    end

    if _OPTIONS['with_vulkan'] then
        dofile(RIVE_PLS_DIR .. '/rive_vk_bootstrap/bootstrap_project.lua')
    end

    filter('options:with-skia')
    do
        includedirs({
            RIVE_RUNTIME_DIR .. '/skia/renderer/include',
            RIVE_RUNTIME_DIR .. '/skia/dependencies',
            RIVE_RUNTIME_DIR .. '/skia/dependencies/skia',
        })
        defines({ 'RIVE_SKIA' })
        libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/skia/out/static' })
    end

    filter({ 'toolset:not msc' })
    do
        buildoptions({ '-Wshorten-64-to-32' })
    end

    filter({ 'system:windows' })
    do
        architecture('x64')
        defines({
            '_USE_MATH_DEFINES',
            '_CRT_SECURE_NO_WARNINGS',
            '_CRT_NONSTDC_NO_DEPRECATE',
            '_WINSOCK_DEPRECATED_NO_WARNINGS',
            'UNICODE',
        })
    end

    filter('system:android')
    do
        defines({ 'RIVE_TOOLS_NO_GLFW' })
    end

    filter('system:ios')
    do
        defines({ 'RIVE_TOOLS_NO_GLFW', 'RIVE_TOOLS_NO_GL' })
    end

    -- Match PLS math options for testing simd.
    filter({ 'system:not windows', 'options:not no_ffp_contract' })
    do
        buildoptions({
            '-ffp-contract=on',
            '-fassociative-math',
            -- Don't warn about simd vectors larger than 128 bits when AVX is not enabled.
            '-Wno-psabi',
        })
    end

    filter({ 'system:windows', 'options:toolset=msc' })
    do
        -- MSVC doesn't allow designated initializers on C++17.
        defines({
            '_SILENCE_CXX20_IS_POD_DEPRECATION_WARNING',
            '_SILENCE_ALL_CXX20_DEPRECATION_WARNINGS',
        })
        buildoptions({
            -- "warning C4577: 'noexcept' used with no exception handling mode specified;
            -- termination on exception is not guaranteed. Specify /EHsc"
            '/EHsc',
        })
    end

    filter('options:with-dawn')
    do
        includedirs({
            RIVE_PLS_DIR .. '/dependencies/dawn/include',
            RIVE_PLS_DIR .. '/dependencies/dawn/out/release/gen/include',
        })
    end

    filter({ 'kind:ConsoleApp or SharedLib or WindowedApp' })
    do
        libdirs({ RIVE_RUNTIME_DIR .. '/build/%{cfg.system}/bin/' .. RIVE_BUILD_CONFIG })

        links({
            'tools_common',
            'rive_pls_renderer',
            'rive_cg_renderer',
            'rive_decoders',
            'rive',
            'libpng',
            'zlib',
            'libwebp',
            'rive_yoga',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'miniaudio',
        })

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'options:not no_rive_jpeg' })
        do
            links({
                'libjpeg',
            })
        end

        if ndk then
            relative_ndk = ndk
            if string.sub(ndk, 1, 1) == '/' then
                -- An absolute file path wasn't working with premake.
                local current_path = string.gmatch(path.getabsolute('.'), '([^\\/]+)')
                for dir in current_path do
                    relative_ndk = '../' .. relative_ndk
                end
            end
            files({
                relative_ndk .. '/sources/android/native_app_glue/android_native_app_glue.c',
            })
        end

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'system:windows' })
        do
            libdirs({
                RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src/Release',
            })
            links({
                'glfw3',
                'opengl32',
                'd3d11',
                'd3d12',
                'dxguid',
                'dxgi',
                'Dbghelp',
                'd3dcompiler',
                'ws2_32',
            })
        end

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'system:macosx' })
        do
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src' })
            links({
                'glfw3',
                'Metal.framework',
                'QuartzCore.framework',
                'Cocoa.framework',
                'CoreGraphics.framework',
                'CoreFoundation.framework',
                'CoreMedia.framework',
                'CoreServices.framework',
                'IOKit.framework',
                'Security.framework',
                'OpenGL.framework',
                'bz2',
                'iconv',
                'lzma',
                'z', -- lib av format
            })
        end

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'system:linux' })
        do
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src' })
            links({ 'glfw3', 'm', 'z', 'dl', 'pthread', 'GL' })
        end

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'system:android' })
        do
            links({ 'EGL', 'GLESv3', 'log' })
        end

        filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'options:with-dawn' })
        do
            libdirs({
                RIVE_PLS_DIR .. '/dependencies/dawn/out/release/obj/src/dawn',
                RIVE_PLS_DIR .. '/dependencies/dawn/out/release/obj/src/dawn/native',
                RIVE_PLS_DIR .. '/dependencies/dawn/out/release/obj/src/dawn/platform',
                RIVE_PLS_DIR .. '/dependencies/dawn/out/release/obj/src/dawn/platform',
            })
            links({
                'winmm',
                'webgpu_dawn',
                'dawn_native_static',
                'dawn_proc_static',
                'dawn_platform_static',
            })
        end

        filter({
            'kind:ConsoleApp or SharedLib or WindowedApp',
            'options:with-dawn',
            'system:windows',
        })
        do
            links({ 'dxguid' })
        end

        filter({
            'kind:ConsoleApp or SharedLib or WindowedApp',
            'options:with-dawn',
            'system:macosx',
        })
        do
            links({ 'IOSurface.framework' })
        end
    end

    filter({ 'kind:ConsoleApp or SharedLib or WindowedApp', 'options:with-skia' })
    do
        links({ 'skia', 'rive_skia_renderer' })
    end

    filter({})
end

rive_tools_project('tools_common', 'StaticLib')
do
    files({
        'common/*.cpp',
        'unit_tests/assets/*.cpp',
        RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_gl.cpp',
        RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_d3d.cpp',
        RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_d3d12.cpp',
        RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_vulkan.cpp',
        RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_dawn.cpp',
        RIVE_PLS_DIR .. '/shader_hotload/**.cpp',
    })

    if _TARGET_OS == 'windows' then
        externalincludedirs({
            dx12_headers .. '/include/directx',
        })
    end

    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL', 'RIVE_TOOLS_NO_GLFW', 'RIVE_TOOLS_NO_GL' })
    end

    filter({ 'toolset:not msc' })
    do
        buildoptions({ '-Wshorten-64-to-32' })
    end

    filter('system:macosx or ios')
    do
        files({ 'common/*.mm' })
    end

    filter('system:macosx')
    do
        files({
            RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_metal.mm',
            RIVE_PLS_DIR .. '/path_fiddle/fiddle_context_dawn_helper.mm',
        })
    end

    filter({})
end
