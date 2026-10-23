# The browser scripting lane

On native platforms script modules run on WAMR, which is compiled into
librive. On the web they run on the browser's own wasm engine instead, and
WAMR is not in the build at all. This folder holds the page side of that.

The web supports AnimaScript modules. The Luau host module, which is the
Luau VM compiled to wasm, also needs a libc, WASI and setjmp/longjmp from its
host. The page does not provide those, so that module fails to link there.
Nothing forbids it; it is just not supported.

## How a file loads on the web

Loading is two steps, so that the browser can instantiate modules
asynchronously and no artboard ever exists before its scripts are ready.

1. `File::import` reads the file as usual. For each script module it makes a
   `BrowserScriptingVM` and registers the module bytes with the page. Nothing
   is instantiated and no script runs.
2. The host awaits `Module.riveScripting.prepare()`. The page compiles and
   instantiates every registered module. Module start runs each script's top
   level.
3. The host calls `File::startScripts()`. Each VM claims its instance, runs
   the same module boot as native (`host_newstate`, the installs, seal), and
   the file registers its scripts. Only now does the app get the File.

A host that skips step 2 still works on any browser that allows synchronous
compilation on the calling thread: step 3 instantiates inline. Every current
browser allows that for modules of our size, see
`browser_lane_instantiation_study.md`.

On a build with threads, both paths compile each lane module the file carries
once on the page and post it to the idle workers, so a pool thread does not
compile it again for its first job. A thread that was already running when
the file loaded gets no message, and compiles its own.

The editor also boots a VM outside of a file load, to preview scripts it just
compiled: `riveWasmVMBoot` in rive_native, which the editor then adopts into
the open file. That call is synchronous, so on the web it registers the
module and starts it inline, without the prepare step.

Both of our web hosts do all of this inside their own file load, so apps see
no change:

- rive_native: `decodeRiveFile` in `lib/src/web/rive_web.dart`, next to the
  wait for image decodes. The C entry point is `riveFileStartScripts`.
- The JS runtime: `Module.startFileScripts` in `wasm/js/shared.js`, used by
  both `load` wrappers. The binding is `File.startScripts`.

Script heaps also need `File::frameBoundary` once a frame, after everything
advanced and drew, or a frame collector module never collects. The JS
runtime keeps no list of its files, so at the end of each frame, the
batched animation frame or `resolveAnimationFrame`, it calls
`Module.riveScripting.frameBoundary()`, which services every live VM and
returns their leak warnings for `console.warn`.

## The files here

- `rive_module_imports.mjs`, generated. The import object a script module is
  instantiated against, and `bindRiveHostCalls`, which binds librive's
  `rive_web_*` functions to one VM. librive exports them as one table,
  `rive_web_calls`, in the IDL's op order, so their names stay out of the
  binary and the glue.
- `rive_script_runner.mjs`, hand written. Keeps one entry per VM, prepares
  and starts modules, and makes every call into a module, catching traps.
- `rive_scripting_pre.js`, generated from the two files above. One plain
  script for emscripten's `--pre-js`. It installs itself as
  `Module.riveScripting`. This is what hosts link.

Regenerate with `python3 src/wasm/idl/generate.py`. Never edit the generated
files. `generate.py --check` fails when they are stale, and that includes a
change to the runner that was not regenerated into the bundle.

## Turning it on

The backend is in a web build only when premake gets `--scripting_vm=wasm`
or `--scripting_vm=both`. No web build passes that by default.

- rive_native: `./build.sh wasm release --scripting_vm=both`
- The JS runtime: the `-w` flag of `wasm/build_wasm.sh`

Scripts decode images with the browser's decoder on every web build, like
the Luau lane, and with our decoders natively.

Modules are trusted like shaders, so a file baked locally by rive-cli runs
in any build.

## Gates

- `packages/rasc/animascript`: `sh tests/web_boot.sh out/debug/rasc`.
  Plain node, no emscripten. Boots an AnimaScript module against the
  generated import object with a fake host, and checks nested staging and
  raised errors.
- `packages/runtime/tests/web_scripting`: `./test.sh`. Builds librive for the
  web with this backend, links the bundle, and runs baked files under node:
  prepared start, inline start, a gradient, a trap the page survives, a
  script whose top level logs, a module importing an op the host lacks,
  which starts and traps only when it calls it, and image decodes through a
  stand in for the browser's decoder. Then it runs files on browser pages
  with `browser_audio.mjs`, in Chrome when it is installed and in Firefox
  and WebKit when `RIVE_WEB_BROWSERS` names them, which takes `npm ci` there
  and `npx playwright-core install firefox webkit`. Processors play on each
  browser's audio worklet, which must render the samples the harness hashes
  with SIMD or without, and jobs run on its workers. One page leaves the
  runner to make its own context, which in Firefox and WebKit must start on
  the page's first click. A page fails when librive's threads outgrow the
  harness's pool. A page without cross origin isolation loads the build
  without threads and runs its jobs on the page.
  `fixture/bake.sh` rebakes the files and needs rive-cli built.
- `packages/runtime_wasm/js`: `npm run test:scripting`. Loads baked files
  through the JS runtime's public API under jest, from a canvas build with
  this backend and tools on, so unsigned files run. Build that in
  `packages/runtime_wasm/wasm` with
  `OUT_DIR=build/canvas_scripting_single/bin/release ./build_wasm.sh -s -w -t release`.
  It covers init and draw, a trap the page survives, an op the host lacks,
  browser image decodes, the collection at the end of each frame and the
  bridge counters. CI runs it.
- `packages/luau_wasm/tests/check_web_imports.mjs`: every `rive_*` import of
  the Luau host module resolves through the generated import object. It is
  there because that module imports the whole rive surface, which makes it a
  coverage check for the generator. It does not mean the web runs it.

## Rules the code depends on

- **Every call into a module goes through `BrowserScriptingVM::call`.** That
  is where memory the host wrote gets copied back before the module runs,
  where `ScriptCallScope` opens, and where traps are caught. A JS exception
  must never cross librive frames, because it would skip their destructors.
  The catch sits directly around the export call, so only module frames ever
  unwind. `raiseModuleError` follows the same rule from the other side: it
  records the message and the page throws it after the host call returns.
- **Staging is a stack.** A host op can call back into the script, which
  calls more imports. Each staged import takes a mark and restores it in a
  `finally`, so a throw through it still restores.
- **Memory the host only read is never written back.** Natives read module
  memory while the module is running, and the module may reuse that memory
  before the next call.
- **The module's start function is lifted into an export.** rasc runs every
  script's top level in the wasm start function, and a top level can pass
  memory to the host (a log line is enough). A module's memory is an export,
  which the page cannot reach until start has returned. So the runner
  rewrites the module bytes: it drops the start section, exports that
  function, and calls it once the instance and its memory exist. A module
  baked before the clocks moved to `rive_rt_v1` still imports them from `env`
  and needs a rebake to link here.
- **librive's entry points are reached as `Module['_name']`.** Optimized
  emscripten builds rename the raw wasm exports, so looking them up by name
  only works in a debug build.
- **Export names are string literals.** The page caches the export by the
  name's pointer, like the native lane.
- **Names shared with wasm are quoted** in the generated code, because the
  JS runtime's release build uses the closure compiler.

## Limits

The host sets the main call budget and the audio hard limit through
`ScriptingLimits`, and each processor's start message carries its hard limit
to the worklet, where 0 means none. Jobs and workers have no time limit,
except where one with fuel holds the page's thread.

Nothing can interrupt a call in a browser, so a lane ends early only at its
own fuel checks. A bake gives a lane module fuel exactly when it gives the
main module fuel, and each check calls `rive_lane_v1.fuel_check`. Natively
that does nothing, since WAMR's interrupts already stop every lane there.

- For a job or worker, the runner's `fuel_check` asks librive through
  `rive_web_lane_fuel(lane)` why the lane must end, and throws the answer when
  there is one. The web `WasmLane` keeps native's `interruptCall`,
  `interruptAfter` and `dropInterrupt` over one deadline, so a stop ends a job
  at its next check and a worker at its first check past its 50 ms grace, as
  natively, a released file's lanes included.
- A job run or worker step on the page's thread, which is every one in a build
  without threads, runs under `PageThreadBudget`. It arms the thread's call
  deadline with the call budget the VM had when the lane started, and the
  check answers once that passes. A VM that bounds no calls, as under the
  debugger or with a budget of 0, gives its lanes no budget either.
- For a processor the worklet's mixer answers alone, without librive. Init,
  with the lifted start and the free, gets `kProcessorInitMs` in all, and each
  block gets its block budget from when it starts. The mixer still drops a
  block that returns past its budget, which catches processors without fuel
  and time spent outside script.
- A check's throw unwinds only the lane module's frames, never librive's, and
  the runner's catch turns it into the lane's failure, which settles stopped
  when the lane was stopped.

Without fuel, a stopped job ends only once it polls `Lanes.stopping()`, a
stopped worker ends at its next await, and a job or worker step on the page's
thread runs to its end.

## Script audio in real time

Audio processors run on an audio worklet, which reaches shared memory
through Atomics on librive's heap while librive stays on the page. That
needs librive built with threads and, in a browser, cross origin isolation.

- The host names the worklet script in `riveScripting.audioWorkletUrl`:
  `rive_audio_worklet.mjs` in a browser and `rive_audio_worklet_nrdp.js` on
  NRDP. In a browser the worklet loads the modules it imports from beside it,
  so the host serves these four next to it as well: `rive_lane.mjs`,
  `rive_lane_imports.mjs`, `rive_shared_lane.mjs` and
  `rive_shared_layout.mjs`. The NRDP script is one bundle and needs nothing
  beside it.
- The host may hand over its own `AudioContext` as
  `riveScripting.audioContext`, and should resume it on the user's first
  gesture. Without one, the runner makes a context as the first processor
  starts, and resumes it on the page's first click, tap or key press, since
  browsers start audio only after a gesture. If that context has not started
  a second after it was made, the runner warns once on the console.
- The worklet counts every block it renders. When that stops while
  processors run, every processor fails. On NRDP that takes 100 ms, and the
  runner ends the worklet thread. A browser cannot end it, and Chrome's
  close waits on the stuck thread, so after a second the runner drops the
  output, takes its node out of the mix and leaves its context open. Real
  time audio then refuses a host's context that stalled or closed, so the
  host hands over a new one. While a start is pending the watch waits longer
  than `kProcessorInitMs`, so an init with fuel that runs past its own budget
  fails alone first, and each queued start gets that whole allowance. A block
  with fuel ends at its hard limit long before the watch, so the watch is left
  for a processor without fuel or without a hard limit, a first block on
  NRDP, which is allowed 100 ms, and a stall in code without checks, such as
  the collector.

## The browser lane

`packages/runtime_wasm/scripting_lane/lane.mjs` runs the stress fixtures in
`packages/runtime/tests/web_scripting/stress` through the webgl2 JS runtime
in headless Chrome: a GPU Canvas pass, a path of thousands of segments, an
image mesh, a data bound list of a thousand rows and shaped text, each
rewritten every frame. For each it compares frame 60 with what rive-cli
renders on WAMR, and times 300 frames after a warmup on both. It bakes the
fixtures with `stress/bake.sh` first and the browser loads that same bake,
so both lanes run the same module. Only the WAMR bench differs: it rebakes
with the release scripts a publish ships, which drop the throw messages.
Build and run it with:

```
cd packages/runtime_wasm/wasm
OUT_DIR=build/webgl2_scripting/bin/release ./build_wasm.sh -r webgl2 -w -t release
cd ../../rive-cli && ./build.sh release
cd ../runtime_wasm/scripting_lane && npm install && node lane.mjs
```

It prints a table of milliseconds, host calls and bytes per frame. `--aot`
also benches WAMR's AOT tier, with `RIVE_WAMRC` naming a wamrc built from our
patched WAMR. `--swiftshader` draws with Chrome's software GL, and
`--no-wamr` skips rive-cli and loads the committed `.riv` files.
`stress/bake.sh` rebakes those.

The page reads the bridge counters with
`Module.riveScripting.countHostCalls(true)` and `hostCallCounts()`: calls
into librive, bytes staged into its heap and copied back, and bytes read and
written through its memory seam.

The lane stays manual. It needs Chrome and a GPU: under SwiftShader the GPU
Canvas fixture draws nothing from a vertex buffer it updates every frame,
while the machine's GPU matches WAMR exactly, and frame times there mostly
measure the software rasterizer.

## Not done yet

- **The debugger** sees no trap text on the web.
- **No automated gate for our hosts in a browser.** The browser pages of
  `tests/web_scripting/test.sh` load its harness, not a host. The browser
  lane above and `tests/web_scripting/browser_probe.mjs` are manual.
  The probe loads the real rive_native web build in Chrome, Firefox and
  WebKit. It passed in all three on 2026-09-19: file imported, module
  prepared in about 2 ms, `init`, `advance` and `draw` ran, a gradient
  crossed, and a trap was reported while the page kept drawing. Nothing has
  run inside the Flutter editor or an app yet.
