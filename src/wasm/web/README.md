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

Runtime builds only run signed modules. A file baked locally by rive-cli is
unsigned, so it only runs in a tools build.

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
  stand in for the browser's decoder.
  `fixture/bake.sh` rebakes the files and needs rive-cli built.
- `packages/runtime_wasm/js`: `npm run test:scripting`. Loads baked files
  through the JS runtime's public API under jest, from a canvas build with
  this backend and tools on, so unsigned files run. Build that in
  `packages/runtime_wasm/wasm` with
  `OUT_DIR=build/canvas_scripting_single/bin/release ./build_wasm.sh -s -w -t release`.
  It covers init and draw, a trap the page survives, an op the host lacks,
  browser image decodes and the bridge counters. CI runs it.
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
- **No automated browser gate.** The gates run under node and jsdom. The
  browser lane above and `tests/web_scripting/browser_probe.mjs` are manual.
  The probe loads the real rive_native web build in Chrome, Firefox and
  WebKit. It passed in all three on 2026-09-19: file imported, module
  prepared in about 2 ms, `init`, `advance` and `draw` ran, a gradient
  crossed, and a trap was reported while the page kept drawing. Nothing has
  run inside the Flutter editor or an app yet.
