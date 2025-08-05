dofile('rive_build_config.lua')

dofile('premake5_pls_renderer.lua')
dofile(RIVE_RUNTIME_DIR .. '/premake5_v2.lua')
dofile(RIVE_RUNTIME_DIR .. '/decoders/premake5_v2.lua')

newoption({ trigger = 'with-skia', description = 'use skia' })
if _OPTIONS['with-skia'] then
    dofile(RIVE_RUNTIME_DIR .. '/skia/renderer/premake5_v2.lua')
end

if not _OPTIONS['with-webgpu'] then
    project('path_fiddle')
    do
        dependson('rive')
        kind('ConsoleApp')
        includedirs({
            'include',
            RIVE_RUNTIME_DIR .. '/include',
            'include',
            RIVE_RUNTIME_DIR .. '/renderer/src',
            RIVE_RUNTIME_DIR .. '/renderer/shader_hotload',
        })
        externalincludedirs({
            'glad',
            RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw/include',
            yoga,
        })

        fatalwarnings { "All" }

        defines({ 'YOGA_EXPORT=' })

        files({ 'path_fiddle/**.cpp', 'shader_hotload/**.cpp' })

        links({
            'rive',
            'rive_pls_renderer',
            'rive_decoders',
            'libwebp',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'rive_yoga',
        })
        filter({ 'options:not no_rive_png' })
        do
            links({ 'zlib', 'libpng' })
        end
        filter({ 'options:not no_rive_jpeg' })
        do
            links({ 'libjpeg' })
        end
        filter({})

        if _OPTIONS['with_vulkan'] then
            dofile('rive_vk_bootstrap/bootstrap_project.lua')
        end

        filter('action:xcode4')
        do
            -- xcode doesnt like angle brackets except for -isystem
            -- should use externalincludedirs but GitHub runners dont have latest premake5 binaries
            buildoptions({ '-isystem' .. yoga })
        end

        filter({ 'toolset:not msc' })
        do
            buildoptions({ '-Wshorten-64-to-32' })
        end

        filter('options:with-skia')
        do
            includedirs({
                RIVE_RUNTIME_DIR .. '/skia/renderer/include',
                RIVE_RUNTIME_DIR .. '/skia/dependencies',
                RIVE_RUNTIME_DIR .. '/skia/dependencies/skia',
            })
            defines({ 'RIVE_SKIA', 'SK_GL' })
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/skia/out/static' })
            links({ 'skia', 'rive_skia_renderer' })
        end

        filter('system:windows')
        do
            architecture('x64')
            defines({ 'RIVE_WINDOWS', '_CRT_SECURE_NO_WARNINGS' })
            libdirs({
                RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src/Release',
            })
            links({ 'glfw3', 'opengl32', 'd3d11', 'd3d12', 'dxguid', 'dxgi', 'd3dcompiler' })
        end

        if _TARGET_OS  == 'windows'then
            externalincludedirs({
                dx12_headers .. '/include/directx'
            })
        end

        filter('system:macosx')
        do
            files({ 'path_fiddle/**.mm' })
            buildoptions({ '-fobjc-arc' })
            links({
                'glfw3',
                'Cocoa.framework',
                'Metal.framework',
                'QuartzCore.framework',
                'IOKit.framework',
            })
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src' })
        end

        filter('system:linux')
        do
            links({ 'glfw3' })
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src' })
        end

        filter('options:with-dawn')
        do
            includedirs({
                'dependencies/dawn/include',
                'dependencies/dawn/out/release/gen/include',
            })
            libdirs({
                'dependencies/dawn/out/release/obj/src/dawn',
                'dependencies/dawn/out/release/obj/src/dawn/native',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
            })
            links({
                'winmm',
                'webgpu_dawn',
                'dawn_native_static',
                'dawn_proc_static',
                'dawn_platform_static',
            })
        end

        filter({ 'options:with-dawn', 'system:windows' })
        do
            links({ 'dxguid' })
        end

        filter({ 'options:with-dawn', 'system:macosx' })
        do
            links({ 'IOSurface.framework' })
        end

        filter('system:emscripten')
        do
            targetextension('.js')
            linkoptions({
                '-sUSE_GLFW=3',
                '-sMIN_WEBGL_VERSION=2',
                '-sMAX_WEBGL_VERSION=2',
                '--preload-file ' .. path.getabsolute('../../../zzzgold') .. '/rivs@/',
            })
            files({ 'path_fiddle/index.html' })
        end

        filter({ 'options:with_rive_layout' })
        do
            defines({ 'YOGA_EXPORT=' })
            includedirs({ yoga })
            links({
                'rive_yoga',
            })
        end

        filter('files:**.html')
        do
            buildmessage('Copying %{file.relpath} to %{cfg.targetdir}')
            buildcommands({ 'cp %{file.relpath} %{cfg.targetdir}/%{file.name}' })
            buildoutputs({ '%{cfg.targetdir}/%{file.name}' })
        end
    end
end

if _OPTIONS['with-webgpu'] or _OPTIONS['with-dawn'] then
    project('webgpu_player')
    do
        kind('ConsoleApp')
        includedirs({
            'include',
            RIVE_RUNTIME_DIR .. '/include',
            'glad',
            'include',
            RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw/include',
        })
        fatalwarnings { "All" }

        defines({ 'YOGA_EXPORT=' })

        files({
            'webgpu_player/webgpu_player.cpp',
            'webgpu_player/index.html',
        })

        links({
            'rive',
            'rive_pls_renderer',
            'rive_decoders',
            'libpng',
            'zlib',
            'libjpeg',
            'libwebp',
            'rive_harfbuzz',
            'rive_sheenbidi',
            'rive_yoga',
        })

        filter('system:windows')
        do
            architecture('x64')
            defines({ 'RIVE_WINDOWS', '_CRT_SECURE_NO_WARNINGS' })
            libdirs({
                RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src/Release',
            })
            links({ 'glfw3', 'opengl32', 'd3d11', 'dxgi', 'd3dcompiler' })
        end

        filter('system:macosx')
        do
            files({ 'path_fiddle/fiddle_context_dawn_helper.mm' })
            buildoptions({ '-fobjc-arc' })
            links({
                'glfw3',
                'Cocoa.framework',
                'Metal.framework',
                'QuartzCore.framework',
                'IOKit.framework',
            })
            libdirs({ RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw_build/src' })
        end

        filter('options:with-dawn')
        do
            includedirs({
                'dependencies/dawn/include',
                'dependencies/dawn/out/release/gen/include',
            })
            files({
                'path_fiddle/fiddle_context_dawn.cpp',
            })
            libdirs({
                'dependencies/dawn/out/release/obj/src/dawn',
                'dependencies/dawn/out/release/obj/src/dawn/native',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
            })
            links({
                'winmm',
                'webgpu_dawn',
                'dawn_native_static',
                'dawn_proc_static',
                'dawn_platform_static',
            })
        end

        filter({ 'options:with-dawn', 'system:windows' })
        do
            links({ 'dxguid' })
        end

        filter({ 'options:with-dawn', 'system:macosx' })
        do
            links({ 'IOSurface.framework' })
        end

        filter('system:emscripten')
        do
            targetextension('.js')
            linkoptions({
                '-sEXPORTED_FUNCTIONS=_animationFrame,_main,_start,_malloc,_free',
                '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap',
                '-sUSE_WEBGPU',
                '-sENVIRONMENT=web,shell',
                '-sWARN_ON_UNDEFINED_SYMBOLS=0',
                '-sERROR_ON_UNDEFINED_SYMBOLS=0',
            })
        end

        filter({ 'options:with_rive_layout' })
        do
            defines({ 'YOGA_EXPORT=' })
            includedirs({ yoga })
            links({
                'rive_yoga',
            })
        end

        filter('files:**.html or **.riv or **.js')
        do
            buildmessage('Copying %{file.relpath} to %{cfg.targetdir}')
            buildcommands({ 'cp %{file.relpath} %{cfg.targetdir}/%{file.name}' })
            buildoutputs({ '%{cfg.targetdir}/%{file.name}' })
        end
    end
end
