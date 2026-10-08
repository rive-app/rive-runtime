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
# A gradient crosses as raw module addresses. The script then traps in its
# third advance, which is reported while the page keeps drawing.
node run.mjs $HARNESS fixture/trap.riv \
    --expect "gradient ready" \
    --expect "wasm call trapped in host_obj_advance" \
    --expect "last trap: unreachable" \
    --expect "drew after advance 3.0" \
    --expect "script vms alive after the frames: 1"
# Gradient stops far past the module's memory resolve nothing rather than
# staging gigabytes.
node run.mjs $HARNESS fixture/gradient_bounds.riv \
    --expect "huge gradient handle 0" \
    --expect "script vms alive after the frames: 1"
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
for MODE in "" --sync; do
    node run.mjs $HARNESS fixture/simd.riv $MODE --lacks simd --vms 0 \
        --expect "this browser lacks WebAssembly SIMD, so this file's scripts are off; bake with wasmSimd: off to support it" \
        --expect "artboard drew 5 frames"
    node run.mjs $HARNESS fixture/relaxed_simd.riv $MODE --lacks relaxed --vms 0 \
        --expect "this browser lacks WebAssembly relaxed SIMD, so this file's scripts are off; bake with wasmSimd: on to support it" \
        --expect "artboard drew 5 frames"
done
