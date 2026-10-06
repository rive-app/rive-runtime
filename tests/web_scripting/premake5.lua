dofile('rive_build_config.lua')

local runtime = path.getabsolute('../../')
dofile(path.join(runtime, 'premake5_v2.lua'))
dofile(path.join(runtime, 'decoders/premake5_v2.lua'))
dofile(path.join(runtime, 'renderer/premake5_pls_renderer.lua'))

-- librive built for the web with the wasm scripting backend, linked into a
-- page that run.mjs drives under node.
project('web_scripting_harness')
do
    kind('ConsoleApp')
    targetextension('.mjs')
    includedirs({ path.join(runtime, 'include') })
    files({ 'harness.cpp', path.join(runtime, 'utils/no_op_factory.cpp') })
    links({
        'rive_pls_renderer',
        'rive_decoders',
        'libpng',
        'zlib',
        'libjpeg',
        'libwebp',
        'rive',
        'rive_harfbuzz',
        'rive_sheenbidi',
        'rive_yoga',
        'miniaudio',
    })
    linkoptions({
        '--no-entry',
        '--pre-js ' .. path.join(runtime, 'src/wasm/web/rive_scripting_pre.js'),
        '-sMODULARIZE=1',
        '-sEXPORT_ES6=1',
        '-sENVIRONMENT=node',
        '-sEXPORTED_FUNCTIONS=_malloc,_free',
        '-sEXPORTED_RUNTIME_METHODS=HEAPU8',
    })
end
