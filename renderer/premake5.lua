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
        })
        externalincludedirs({
            'glad',
            RIVE_RUNTIME_DIR .. '/skia/dependencies/glfw/include',
            yoga,
        })

        flags({ 'FatalCompileWarnings' })

        defines({ 'YOGA_EXPORT=' })

        files({ 'path_fiddle/**.cpp' })

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
            links({ 'glfw3', 'opengl32', 'd3d11', 'dxgi', 'd3dcompiler' })
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
                'dawn_native_static',
                'webgpu_dawn',
                'dawn_platform_static',
                'dawn_proc_static',
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
            targetname('path_fiddle.js')
            linkoptions({
                '-sUSE_GLFW=3',
                '-sMIN_WEBGL_VERSION=2',
                '-sMAX_WEBGL_VERSION=2',
                '--preload-file ' .. path.getabsolute('../../../gold') .. '/rivs@/',
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
        flags({ 'FatalCompileWarnings' })

        defines({ 'YOGA_EXPORT=' })

        files({
            'webgpu_player/webgpu_player.cpp',
            'webgpu_player/index.html',
            'webgpu_player/icons.html',
            'webgpu_player/rive.js',
            'webgpu_player/rivs/Santa_Claus.riv',
            'webgpu_player/rivs/Coffee_Cup.riv',
            'webgpu_player/rivs/skull_404.riv',
            'webgpu_player/rivs/octopus_loop.riv',
            'webgpu_player/rivs/planets.riv',
            'webgpu_player/rivs/Timer.riv',
            'webgpu_player/rivs/adventuretime_marceline-pb.riv',
            'webgpu_player/rivs/towersDemo.riv',
            'webgpu_player/rivs/skills_demov1.riv',
            'webgpu_player/rivs/car_demo.riv',
            'webgpu_player/rivs/cloud_icon.riv',
            'webgpu_player/rivs/coffee_loader.riv',
            'webgpu_player/rivs/documentation.riv',
            'webgpu_player/rivs/fire_button.riv',
            'webgpu_player/rivs/lumberjackfinal.riv',
            'webgpu_player/rivs/mail_box.riv',
            'webgpu_player/rivs/new_file.riv',
            'webgpu_player/rivs/poison_loader.riv',
            'webgpu_player/rivs/popsicle_loader.riv',
            'webgpu_player/rivs/radio_button_example.riv',
            'webgpu_player/rivs/avatar_demo.riv',
            'webgpu_player/rivs/stopwatch.riv',
            'webgpu_player/rivs/volume_bars.riv',
            'webgpu_player/rivs/travel_icons.riv',
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
            libdirs({
                'dependencies/dawn/out/release/obj/src/dawn',
                'dependencies/dawn/out/release/obj/src/dawn/native',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
                'dependencies/dawn/out/release/obj/src/dawn/platform',
            })
            links({
                'dawn_native_static',
                'webgpu_dawn',
                'dawn_platform_static',
                'dawn_proc_static',
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
            targetname('webgpu_player.js')
            linkoptions({
                '-sEXPORTED_FUNCTIONS=_RiveInitialize,_RiveBeginRendering,_RiveFlushRendering,_RiveLoadFile,_File_artboardNamed,_File_artboardDefault,_File_destroy,_ArtboardInstance_width,_ArtboardInstance_height,_ArtboardInstance_stateMachineNamed,_ArtboardInstance_animationNamed,_ArtboardInstance_defaultStateMachine,_ArtboardInstance_align,_ArtboardInstance_destroy,_StateMachineInstance_setBool,_StateMachineInstance_setNumber,_StateMachineInstance_fireTrigger,_StateMachineInstance_pointerDown,_StateMachineInstance_pointerMove,_StateMachineInstance_pointerUp,_StateMachineInstance_advanceAndApply,_StateMachineInstance_draw,_StateMachineInstance_destroy,_LinearAnimationInstance_advanceAndApply,_LinearAnimationInstance_draw,_LinearAnimationInstance_destroy,_Renderer_save,_Renderer_restore,_Renderer_translate,_Renderer_transform,_malloc,_free',
                '-sEXPORTED_RUNTIME_METHODS=ccall,cwrap',
                '-sSINGLE_FILE',
                '-sUSE_WEBGPU',
                '-sENVIRONMENT=web,shell',
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
