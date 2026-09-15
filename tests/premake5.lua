newoption({
    trigger = 'with-coverage',
    description = 'instrument for llvm-cov so gms runs count toward coverage',
})
if _OPTIONS['with-coverage'] then
    -- premake5_v2 adds the llvm-cov compile flags when TESTING is set, so the
    -- runtime and renderer inside this workspace get instrumented too.
    TESTING = true
end

dofile('rive_tools_project.lua')

newoption({
    trigger = 'no_tools_shader_hotloading',
    description = 'do not compile in support for shader hotloading',
})

if not _OPTIONS['for_unreal'] then
    rive_tools_project(
        'bench',
        _OPTIONS['os'] == 'ios' and 'StaticLib'
            or _OPTIONS['all_tools_as_static'] and 'StaticLib'
            or 'ConsoleApp'
    )
    do
        files({ 'bench/*.cpp' })
    end
end

rive_tools_project('gms', 'RiveTool')
do
    filter({ 'options:with-coverage', 'toolset:not msc' })
    do
        buildoptions({ '-fprofile-instr-generate', '-fcoverage-mapping' })
        linkoptions({ '-fprofile-instr-generate', '-fcoverage-mapping' })
    end
    filter({})
    files({ 'gm/*.cpp' })
    -- Deferred-rendering 2D record/replay (SerializingFactory + the replay that
    -- drives a real Factory/Renderer) so GMs can verify 2D replay against PLS.
    files({
        '../utils/serializing_factory.cpp',
        '../utils/serialized_replay.cpp',
    })
    -- serializing_factory.cpp decodes images (decoders header).
    includedirs({ '../decoders/include' })
    -- Ore GM tests need Obj-C++ on Apple (ore headers include <Metal/Metal.h>).
    -- .mm wrappers #include the .cpp files so every Apple generator compiles
    -- them as Obj-C++ without needing compileas or buildoptions hacks.
    -- On non-Apple, ore GMs are excluded from the gm/*.cpp glob but re-added
    -- per-platform as the corresponding Ore backend lands.
    removefiles({ 'gm/ore_*.cpp' })
    -- render_canvas GMs use RenderContext::makeRenderCanvas which is gated
    -- behind RIVE_CANVAS; only compile them when with_rive_canvas is on.
    filter({ 'options:not with_rive_canvas' })
    do
        removefiles({ 'gm/render_canvas*.cpp' })
    end
    filter('system:macosx or ios')
    do
        files({ 'gm/*.mm' })
        buildoptions({ '-fobjc-arc' })
    end
    -- D3D11 Ore backend: include ore GMs on Windows when with_rive_canvas is on.
    filter({ 'system:windows', 'options:with_rive_canvas' })
    do
        files({ 'gm/ore_*.cpp' })
    end
    -- GL Ore backend: include ore GMs on Android/Emscripten/Linux.
    filter({ 'system:android or emscripten or linux', 'options:with_rive_canvas' })
    do
        files({ 'gm/ore_*.cpp' })
    end
    -- Console builds: include ore GMs to exercise ORE_BACKEND_VK paths.
    filter({ 'options:_console_only_ore_vk', 'options:with_rive_canvas' })
    do
        files({ 'gm/ore_*.cpp' })
    end
    filter({})
    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({ RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
    filter('system:emscripten')
    do
        files({ 'gm/gms.html' })
    end
    filter({})
end

rive_tools_project('goldens', 'RiveTool')
do
    exceptionhandling('On')
    files({ 'goldens/goldens.cpp', 'goldens/goldens_bench.cpp' })
    -- The deferred recording factory (deferred_render_factory.hpp) decodes image
    -- dimensions at record time so the artboard's layout sees real sizes; needs
    -- the decoder header + RIVE_DECODERS (the lib is already linked).
    includedirs({ '../decoders/include' })
    defines({ 'RIVE_DECODERS' })
    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({ RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
    filter({ 'options:for_unreal' })
    do
        defines({ 'RIVE_UNREAL' })
    end
    filter('system:emscripten')
    do
        files({ 'goldens/goldens.html' })
    end
    -- prospero turns RuntimeTypeInfo on whenever exceptions are enabled, and it
    -- overrides an explicit rtti('Off'). That leaves goldens the only -frtti
    -- target in an otherwise -fno-rtti build, so the deferred render types emit
    -- typeinfo referencing bases that librive.a never defines. AdditionalOptions
    -- land after the toolset flag, so this wins. Every other target, host
    -- included, already builds goldens -fno-rtti.
    filter('system:prospero')
    do
        buildoptions({ '-fno-rtti' })
    end
    filter({})
end

-- Headless collector validation on device targets; a plain executable so it
-- runs from adb shell without the APK harness. Wasm scripting only: the
-- source names WasmScriptingVM, which other configurations never declare.
if _OPTIONS['with_rive_scripting']
    and (_OPTIONS['scripting_vm'] == 'wasm' or _OPTIONS['scripting_vm'] == 'both')
then
    rive_tools_project('wasm_gc_bench', 'ConsoleApp')
    do
        files({ 'wasm_gc_bench/wasm_gc_bench.cpp' })
    end
end

rive_tools_project('player', 'RiveTool')
do
    files({ 'player/player.cpp' })
    filter('system:emscripten')
    do
        files({ 'player/player.html' })
    end

    filter({ 'options:not no_tools_shader_hotloading' })
    do
        files({ RIVE_PLS_DIR .. '/shader_hotload/**.cpp' })
    end
end
