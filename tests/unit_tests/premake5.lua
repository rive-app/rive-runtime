filter({ 'system:not windows' })
do
    buildoptions({ '-ffp-model=strict' })
end
filter({ 'system:windows' })
do
    -- Visual Studio compiles through clang-cl and takes MSVC flag syntax; ninja drives clang++
    -- directly, which only understands the GNU spelling.
    buildoptions({ _ACTION == 'ninja' and '-ffp-model=strict' or '/fp:strict' })
end
filter({ 'system:windows', 'options:toolset=clang' })
do
    buildoptions({
        '-Wno-overriding-option',
    })
end
filter({})

dofile('rive_build_config.lua')
TESTING = true
defines({
    'TESTING',
    'ENABLE_QUERY_FLAT_VERTICES',
    'WITH_RIVE_TOOLS',
    'WITH_RIVE_TEXT',
    'WITH_RIVE_AUDIO',
    'WITH_RIVE_AUDIO_TOOLS',
    'WITH_RIVE_LAYOUT',
    'WITH_RIVE_SCRIPTING',
    'YOGA_EXPORT=',
    'RIVE_NO_CORETEXT',
})

if _OPTIONS['toolset'] == 'msc' then
    defines({
        'MA_NO_MP3', -- miniaudio's mp3 decoder + fp:strict causes C2099
    })
end
dofile(path.join(path.getabsolute('../../'), 'premake5_v2.lua'))
dofile(path.join(path.getabsolute('../../decoders/'), 'premake5_v2.lua'))

-- Force "--raw_shaders" so that we can include the generated shader files and
-- compile them as C++ for unit tests.
_OPTIONS['raw_shaders'] = true

dofile(path.join(path.getabsolute('../../renderer/'), 'premake5_pls_renderer.lua'))

dofile('../rive_tools_project.lua')

rive_tools_project('unit_tests', 'ConsoleApp')
do
    kind('ConsoleApp')
    exceptionhandling('On')

    includedirs({
        '..',
        '../include',
        '../../include',
        '../../decoders/include',
        '../../renderer/include',
        '../../renderer/rive_vk_bootstrap/include',
        '../../renderer/src',
        '../../../rive_native/native/include',
        '../../../texture_compressor/src',
        harfbuzz .. '/src',
        miniaudio,
        yoga,
        pls_generated_headers,
        luau .. '/VM/include',
        -- we need these for testing so we can compile
        luau .. '/Compiler/include',
        luau .. '/Ast/include',
        luau .. '/Common/include',
    })

    -- The renderer premake only puts these on rive_pls_renderer's search path,
    -- but the vulkan tests include vkutil.hpp directly.
    if _OPTIONS['with_vulkan'] then
        externalincludedirs({
            vulkan_headers .. '/include',
            vulkan_memory_allocator .. '/include',
        })
    end

    links({
        'rive',
        'rive_harfbuzz',
        'rive_sheenbidi',
        'rive_yoga',
        'rive_decoders',
        'libpng',
        'zlib',
        'libjpeg',
        'libwebp',
        'miniaudio',
        'luau_vm',
        'luau_compiler',
    })

    files({
        'runtime/**.cpp', -- the runtime tests
        'renderer/**.cpp', -- the renderer tests
        '../../utils/**.cpp', -- no_op utils
        '../common/render_context_null.cpp',
        '../../../texture_compressor/src/write_ktx2.cpp',
    })

    -- These exercise the Luau backend directly; the wasm backend's coverage
    -- is the two-runner differential in rive-cli until wasm twins land.
    if _OPTIONS['scripting_vm'] == 'wasm' then
        removefiles({ 'runtime/scripting/**' })
    end

    filter('system:linux')
    do
        links({ 'dl', 'pthread' })
    end
    filter({ 'options:not no-harfbuzz-renames' })
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

    filter({ 'system:macosx' })
    do
        links({
            'Foundation.framework',
            'ImageIO.framework',
            'CoreGraphics.framework',
            'CoreText.framework',
        })
        -- The ore helper pulls in <Metal/Metal.h>, so compile these tests as
        -- Obj-C++ on Apple instead of the C++ glob.
        files({
            'renderer/ore_buffer_race_test.mm',
            'renderer/ore_layout_intern_test.mm',
            'renderer/ore_scratch_pass_objects_test.mm',
            'renderer/ore_split_stage_test.mm',
        })
        removefiles({
            'renderer/ore_buffer_race_test.cpp',
            'renderer/ore_layout_intern_test.cpp',
            'renderer/ore_scratch_pass_objects_test.cpp',
            'renderer/ore_split_stage_test.cpp',
        })
    end

    filter({ 'toolset:not msc' })
    do
        linkoptions({ '-fprofile-instr-generate', '-fcoverage-mapping' })
    end

    filter({})
end
