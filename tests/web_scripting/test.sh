#!/bin/bash
# Builds librive for the web with the wasm scripting backend and runs baked
# AnimaScript files on the JS engine under node, the browser lane's path.
set -e
cd "$(dirname "$0")"

# The link does not track its --pre-js input, so a changed bundle would
# otherwise be silently left out.
BUNDLE=../../src/wasm/web/rive_scripting_pre.js
if [[ $BUNDLE -nt out/wasm_debug/web_scripting_harness.mjs ]]; then
    rm -f out/wasm_debug/web_scripting_harness.mjs \
        out/wasm_debug/web_scripting_harness.wasm
fi

build_rive.sh wasm debug --with_rive_scripting --scripting_vm=wasm \
    --with_rive_layout --with_rive_tools -- web_scripting_harness

HARNESS=out/wasm_debug/web_scripting_harness.mjs
BASIC=(--expect "init ran on the web" --expect "clocks ok" --expect "random ok"
    --expect "drew frame 2")
node run.mjs $HARNESS fixture/basic.riv "${BASIC[@]}"
# Without prepare() the module instantiates inline when scripts start.
node run.mjs $HARNESS fixture/basic.riv --sync "${BASIC[@]}"
# A host without a list of its files services every script VM through the
# runner once a frame, as the JS runtime does, which hands back warnings only.
node run.mjs $HARNESS fixture/basic.riv --page-boundary \
    --expect "page boundary called __riveFrameMinor" \
    --expect "page boundary warned []"
# A gradient crosses as raw module addresses. The script then traps in its
# third advance, which is reported while the page keeps drawing.
node run.mjs $HARNESS fixture/trap.riv \
    --expect "gradient ready" \
    --expect "wasm call trapped in host_obj_advance" \
    --expect "last trap: unreachable" \
    --expect "drew after advance 3.0" \
    --expect "script vms alive after the frames: 1"
# Gradient stops far past the module's memory trap the call, as on WAMR,
# rather than staging gigabytes.
node run.mjs $HARNESS fixture/gradient_bounds.riv \
    --expect "wasm call trapped in host_obj_user_init: out of bounds memory access" \
    --expect "script vms alive after the frames: 1"
# Module addresses past 2 GiB reach JS as negative i32s, yet the staged
# imports and librive's memory seam still reach them, as on WAMR.
node run.mjs $HARNESS fixture/high_memory.riv --seam 2147484416 \
    --expect "hi from past 2 GiB" \
    --expect-line "managed past 2 GiB" \
    --expect "trace: managed past 2 GiB 2" \
    --expect "high mat4 inverse 1 0.5" \
    --expect "high gradient minted" \
    --expect "seam seam"
# A full 4 GiB memory still counts all of its pages, and the collector still
# carves chunks below its top.
node run.mjs $HARNESS fixture/top_memory.riv --expect "top gradient minted" \
    --expect "top carve 300000"
# A trap leaves the spill zone to later calls: the runner puts the module's
# call state back after every call.
node run.mjs $HARNESS fixture/spill_trap.riv --expect "spill ok on frame 2"
# A collector that traps mid collection stops its module.
node run.mjs $HARNESS fixture/collector_trap.riv \
    --expect "wasm call trapped in __riveFrameMinor" \
    --expect "script vms alive after the frames: 0"
# Nesting that would run librive out of stack traps the call instead.
node run.mjs $HARNESS fixture/basic.riv --deep \
    --expect "last trap: script calls nest too deep for the host stack" \
    --expect "script vms alive after the frames: 1"
# A script top level that passes memory to the host, which needs the module's
# start function lifted into an export. As on WAMR, only the ops that trap see
# the VM there.
TOP=(--expect "top level log" --expect "init after top level"
    --expect "top level path handle 0")
node run.mjs $HARNESS fixture/top_level.riv "${TOP[@]}"
node run.mjs $HARNESS fixture/top_level.riv --sync "${TOP[@]}"
# A module whose start traps leaves the file, as a native one never joins it.
node run.mjs $HARNESS fixture/start_trap.riv --vms 0 \
    --expect "wasm script module failed to start: module instantiate failed" \
    --expect "script vms kept: 0"
# A host fault in a prepared module's start fails it and leaves librive's
# stack where it was.
node run.mjs $HARNESS fixture/start_fault.riv --vms 0 --fault raise \
    --expect "wasm script module failed to start: module instantiate failed: host fault injected" \
    --expect "librive stack kept across prepare"
# A metered module's own fuel stops a top level that would hang the page.
RUNAWAY=(--vms 0
    --expect "wasm script module failed to start: module instantiate failed: execution exceeded timeout")
node run.mjs $HARNESS fixture/runaway_top.riv "${RUNAWAY[@]}"
node run.mjs $HARNESS fixture/runaway_top.riv --sync "${RUNAWAY[@]}"
# A default release bake runs unbounded unless the page runs only what it can
# bound, as an editor does. Debug bakes run either way.
node run.mjs $HARNESS fixture/release.riv --expect "release init ran" \
    --expect "a runaway script will hang the page"
UNBOUNDED=(--bounded --vms 0
    --expect "wasm script module failed to start: module instantiate failed: this host runs only scripts it can bound"
    --expect-line "script modules instantiated: 0")
node run.mjs $HARNESS fixture/release.riv "${UNBOUNDED[@]}"
node run.mjs $HARNESS fixture/release.riv --sync "${UNBOUNDED[@]}"
node run.mjs $HARNESS fixture/basic.riv --bounded "${BASIC[@]}"
# A release bake with wasmFuel stops its own runaway loop.
METERED=(--expect "metered release init"
    --expect "wasm call trapped in host_obj_user_init: execution exceeded timeout")
node run.mjs $HARNESS fixture/release_fuel.riv "${METERED[@]}"
node run.mjs $HARNESS fixture/release_fuel.riv --sync "${METERED[@]}"
node run.mjs $HARNESS fixture/release_fuel.riv --bounded "${METERED[@]}"
# Scripts started before the page prepared their module wait for it.
node run.mjs $HARNESS fixture/basic.riv --early "${BASIC[@]}" \
    --expect "wasm script module is still being prepared"
# Modules baked before rive_rt_v1 carried the clocks still read them.
node run.mjs $HARNESS fixture/legacy_clock.riv --expect "legacy clocks ok"
# A module importing an op no host has still starts, like on WAMR; only the
# call traps, naming the import.
UNLINKED=(--expect "init without the missing op"
    --expect "last trap: failed to call unlinked import function rive_web_gate_v1.missing")
node run.mjs $HARNESS fixture/unlinked.riv "${UNLINKED[@]}"
node run.mjs $HARNESS fixture/unlinked.riv --sync "${UNLINKED[@]}"
# This tools build links the raw shader op; runtime_wasm's own suite has no
# tools and checks that the call traps there.
node run.mjs $HARNESS fixture/raw_shader.riv \
    --expect "init reaches the raw shader op" \
    --expect "raw shader op returned 0"
# The browser decodes images for scripts, and its results arrive premultiplied
# on a later event loop turn.
DECODE=(--settle --expect "decoded 2x1 first pixel 100,128"
    --expect "decode failed: failed to decode image data")
node run.mjs $HARNESS fixture/decode.riv "${DECODE[@]}"
# A decoder that throws as librive starts it only fails that decode.
node run.mjs $HARNESS fixture/decode.riv --decoder-throws \
    --expect "decode failed: failed to decode image data" \
    --expect "script vms alive after the frames: 1"
# Scroll crosses as typed args, so the web gets it through the same call.
node run.mjs $HARNESS fixture/scroll.riv --scroll \
    --expect "scroll crossed intact" --expect "scroll claimed"
# console reaches the script log and keeps its timers on the web too.
node run.mjs $HARNESS fixture/console.riv \
    --expect "console log on the web" --expect "boot: " \
    --expect "Timer 'boot' does not exist"
# A null string logs an empty line, as on WAMR, rather than trapping, and one
# outside module memory traps the call as it does there.
node run.mjs $HARNESS fixture/console_null.riv \
    --expect "before the null log" --expect-line "" \
    --expect "after the null log" \
    --expect "wasm call trapped in host_obj_user_init: out of bounds memory access"
# trace reaches the script log too, formatted as on WAMR.
node run.mjs $HARNESS fixture/trace.riv \
    --expect "trace: traced 1.5 42 NaN" --expect "after trace"
# A host fault in a nested script call also ends the call it nests in.
node run.mjs $HARNESS fixture/nested_fault.riv --fault raise \
    --expect "wasm call trapped in host_data_value_changed: librive faulted in a host call" \
    --expect "wasm call trapped in host_obj_user_init: librive faulted in a host call, so its script module stops: librive faulted in a nested script call" \
    --expect "script vms alive after the frames: 0"
# A JS error thrown under librive frames is a host fault: unlike a script
# trap, the module stops for good.
node run.mjs $HARNESS fixture/trap.riv --fault read \
    --expect "wasm call trapped in host_obj_user_init: librive faulted in a host call" \
    --expect "last trap: librive faulted in a host call" \
    --expect "script vms alive after the frames: 0"
# A SIMD module runs where the engine has SIMD. Where it lacks the flavor the
# module was baked with, the file's scripts are off with one message saying
# why, and the artboard still draws.
node run.mjs $HARNESS fixture/simd.riv --expect "simd lane 22"
node run.mjs $HARNESS fixture/relaxed_simd.riv --expect "relaxed lane 7"
node run.mjs $HARNESS fixture/scalar.riv --expect "scalar lane 3"
for MODE in "" --sync; do
    node run.mjs $HARNESS fixture/simd.riv $MODE --lacks simd --vms 0 \
        --expect "this browser lacks WebAssembly SIMD, so this file's scripts are off; bake with wasmSimd: off to support it" \
        --expect "artboard drew 5 frames"
    node run.mjs $HARNESS fixture/relaxed_simd.riv $MODE --lacks relaxed --vms 0 \
        --expect "this browser lacks WebAssembly relaxed SIMD, so this file's scripts are off; bake with wasmSimd: on to support it" \
        --expect "artboard drew 5 frames"
    # A module refused for a feature other than the SIMD it uses carries the
    # engine's own reason.
    node run.mjs $HARNESS fixture/scalar.riv $MODE --lacks simd,bulk --vms 0 \
        --expect "module instantiate failed: this engine lacks bulk memory" \
        --expect "artboard drew 5 frames"
    node run.mjs $HARNESS fixture/simd.riv $MODE --lacks relaxed,bulk --vms 0 \
        --expect "module instantiate failed: this engine lacks bulk memory" \
        --expect "artboard drew 5 frames"
done
