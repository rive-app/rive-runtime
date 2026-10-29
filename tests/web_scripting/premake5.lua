dofile('rive_build_config.lua')

newoption({
    trigger = 'web_threads',
    description = 'build librive and the harness with pthreads, the lane a '
        .. 'cross origin isolated page runs',
})
if _OPTIONS['web_threads'] then
    -- Workspace wide, so every library shares the pthread memory model.
    buildoptions({ '-pthread' })
    -- A browser starts a thread past the pool only once the page yields, so it
    -- covers 6 job threads, the worker scheduler and 8 workers' own threads.
    linkoptions({ '-pthread', '-sPTHREAD_POOL_SIZE=16' })
end

local runtime = path.getabsolute('../../')
dofile(path.join(runtime, 'premake5_v2.lua'))
dofile(path.join(runtime, 'decoders/premake5_v2.lua'))
dofile(path.join(runtime, 'renderer/premake5_pls_renderer.lua'))

-- librive built for the web with the wasm scripting backend, linked into the
-- harness run.mjs drives under node and the one browser_audio.mjs loads.
local function harness(name, environment)
    project(name)
    do
        kind('ConsoleApp')
        targetextension('.mjs')
        includedirs({
            path.join(runtime, 'include'),
            path.join(runtime, 'renderer/include'),
        })
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
            '-sENVIRONMENT=' .. environment,
            '-sEXPORTED_FUNCTIONS=_malloc,_free',
            '-sEXPORTED_RUNTIME_METHODS=HEAPU8',
        })
        if _OPTIONS['web_threads'] then
            -- As threaded runtimes do, so a page can leave the worklet unnamed
            -- and a page that cannot share memory fails the load at once.
            linkoptions({
                '--pre-js ' .. path.join(runtime, 'src/wasm/web/rive_audio_worklet_pre.js'),
                '--pre-js ' .. path.join(runtime, 'src/wasm/web/rive_threads_pre.js'),
            })
        end
    end
end

harness('web_scripting_harness', 'node')
harness('web_scripting_page', 'web,worker')
