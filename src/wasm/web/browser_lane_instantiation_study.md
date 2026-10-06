# Browser scripting lane: how to instantiate script modules

The design so far instantiates the script module synchronously inside
`File::import`. The worry was that browsers refuse synchronous wasm
compilation on the main thread. This note records what the browsers actually
do today, what the runtime already offers for late script start, and the
options.

## What the browsers do (measured 2026-09-19 on this Mac)

Same probe in each engine: `new WebAssembly.Module(bytes)` then
`new WebAssembly.Instance(module)`, on the main thread and in a worker. Real
modules are the AnimaScript module ABI fixture (27 KB) and the Luau host
module `vm_host.wasm` (825 KB). Padded modules are a valid module with one
large data segment, to find size limits. Times are milliseconds for three
runs.

| Engine | Thread | 27 KB AS | 825 KB Luau host | 8.1 MB | 64 MB |
|---|---|---|---|---|---|
| Chrome 153 | main | 0.3, 0.1, 0.1 | 1.5, 0.7, 1.0 | **refused** | **refused** |
| Chrome 153 | worker | 0.2, 0.2, 0.1 | 0.6, 0.9, 0.6 | 10.7 | 82 |
| Firefox 155 | main | 3, 2, 2 | 6, 9, 8 | 1 | 7 |
| Firefox 155 | worker | 2, 1, 2 | 9, 7, 7 | 1 | 8 |
| WebKit 26.6 | main | 0, 0, 0 | 2, 2, 1 | 3 | 20 |
| WebKit 26.6 | worker | 0, 1, 0 | 2, 1, 2 | 1 | 12 |

What this says:

- **Workers have no limit in any engine.**
- **Firefox and WebKit have no main thread limit either.**
- **Chrome limits the main thread to 8 MB.** The error reads "WebAssembly.
  Compile is disallowed on the main thread, if the buffer size is larger
  than 8MB". The old limit was 4 KB; Chrome raised it
  (chromestatus feature 5099433642950656). Our largest module is a tenth of
  the new limit.
- Synchronous compile is cheap because engines compile lazily or with a fast
  baseline first. Firefox is the slowest at 6 to 9 ms for the Luau host
  module, which is half a frame, once per file.
- Asynchronous `WebAssembly.compile` took about the same time as the
  synchronous call in every case. Async does not make compile faster, it
  only moves the wait off the call stack.

Not measured: Chrome versions from before the limit was raised, where any
module over 4 KB is refused on the main thread. Embedded Chromium (Android
WebView, Electron) follows the Chrome version it ships. The WebKit build is
Playwright's, which shares the JavaScript engine with Safari but is not
Safari itself.

## What the runtime already has for late script start

- `File::import` finishes reading objects and then calls
  `File::registerScripts()`, which makes the VM, requires each script and
  calls `ScriptAsset::registrationComplete(ref)`. Registration is already a
  separate step at the end of import, not woven through it.
- The editor lane already starts scripts after import:
  `File::adoptWasmScriptingVM` swaps a VM built elsewhere into a file and
  `File::applyWasmRegistration` completes the script assets against it.
- A scripted object with no ready backend is inert: `initScriptedObject`
  returns false and the object skips its calls.
- Script init is attempted when an artboard instance initializes, when a
  data context is bound, and when a state machine instance is built. It is
  not retried per frame. So an instance created before its VM is ready
  stays inert unless something re-runs init (`ScriptedObject::reinit`).
  Images differ here: instances read the image from the shared asset at
  draw time, so a late image just appears.
- Both web hosts already load files asynchronously. Dart's
  `decodeRiveFile` returns a `Future` and already awaits
  `completedDecodingFile`, the image decode wait. The JS runtime's `load`
  returns a `Promise`.

## Options

**A. Synchronous inside import (the design as it stands).** Works today in
every current engine with ten times headroom. Fails on Chrome builds older
than the 8 MB change, and the failure is total: no script runs.

**B. Two step load at the host boundary.** `File::import` stays synchronous
and on the web stops before `registerScripts`. It leaves each VM created but
not instantiated. The host glue, which is already async, asks JS to
`WebAssembly.instantiate` every pending module, awaits them together with
the image decodes, then calls one new entry point that boots the VMs and
runs `registerScripts`. Only then does the app receive the File.
No instance can exist before scripts are ready, so behaviour matches native
exactly, no first frame differs, and no engine limit applies. Cost: one new
C++ entry point, and hosts must call it. A host that skips the await can
fall back to option A inside that entry point, so forgetting it degrades to
today's design instead of breaking.

**C. Lazy start, the way images work.** Import returns at once, modules
instantiate in the background, and scripted objects pick the VM up when it
arrives. This needs a new per frame check (or a walk over live instances)
so early instances get initialized late. The real cost is behaviour: for
the first frames layouts are unsized, scripted drawables are blank, and
converters and listeners do nothing, so a state machine can take a
different path than it does on native. Images arriving late is cosmetic.
Scripts arriving late changes logic.

**D. Asynchronous `File::import` in C++.** Changes the import API on every
platform to solve a web only problem. B gets the same result without it.

**E. Scripting on a worker.** No limits anywhere, but VM calls, the JS slot
table and recording are all per thread, and recording runs on the main
thread today. Far larger than the problem.

## Recommendation

B, with A as its built in fallback. It removes the dependence on any
browser's sync limit, keeps web behaviour identical to native, and reuses
the await the hosts already have. It also fits the decision that the editor
or the high level runtime loads the JS glue, since that same glue is what
awaits the modules.
