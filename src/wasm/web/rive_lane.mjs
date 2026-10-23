// Instances of the lane modules a script module carries, started the same way
// by the page runner and, through createProcessorMixer, by the audio worklets.

import {
    kFrameMinorExport,
    kLaneAllocExport,
    kLaneConsole,
    kLaneEntryPrefixes,
    kLaneFreeExport,
    kLaneResetExport,
    kLiftedStartExport,
    kProcessorInitMs,
    kProcessorInitOverran,
    kProcessorOverran,
    kProcessorProcessExport,
    processorBlockBudgetMs,
} from './rive_lane_imports.mjs';
import { kHoldBlocks, kHoldStarting } from './rive_shared_layout.mjs';
import { createSharedLaneImports, managedLength, utf16Text } from './rive_shared_lane.mjs';

export function errorText(error) {
    return String(error?.message ?? error);
}

// Chrome's worklet scope has no performance; Date's milliseconds still catch a
// block of several.
export const monotonicNow =
    globalThis.performance !== undefined ? () => globalThis.performance.now() : Date.now;

// The console imports: each line is a std string, its byte length before it.
// Only jobs and workers import the timers.
export function laneConsole(memory, log, timers = new Map()) {
    const text = (ptr) => {
        const buffer = memory().buffer;
        return utf16Text(buffer, ptr, managedLength(buffer, ptr));
    };
    const env = {};
    for (const name of kLaneConsole) {
        env[name] = (ptr) => log(text(ptr));
    }
    const report = (end) => (ptr) => {
        const label = text(ptr);
        const started = timers.get(label);
        if (started === undefined) {
            log(`Timer '${label}' does not exist`);
            return;
        }
        if (end) {
            timers.delete(label);
        }
        log(`${label}: ${(monotonicNow() - started).toFixed(3)}ms`);
    };
    env['console.time'] = (ptr) => timers.set(text(ptr), monotonicNow());
    env['console.timeLog'] = report(false);
    env['console.timeEnd'] = report(true);
    return env;
}

const noFuelCheck = () => {};

// An instance of lane.module whose imports reach the lane's own natives, log,
// timers and fuel check. Returns { exports, memory, failure } or { error }.
export function instantiateLane(lane) {
    let memory = null;
    let thrown = null;
    const failure = (error) => thrown ?? errorText(error);
    const imports = {
        'env': laneConsole(() => memory, lane.log, lane.timers),
        'rive_shared_v1': lane.shared(() => memory),
        'rive_lane_v1': {
            'error': (ptr, byteLength) => {
                thrown = utf16Text(memory.buffer, ptr, byteLength);
                lane.onThrow?.(thrown);
            },
            'fuel_check': lane.fuel ?? noFuelCheck,
        },
    };
    if (lane.natives !== undefined) {
        imports[lane.natives] = lane.own(() => memory);
    }
    try {
        const exports = new WebAssembly.Instance(lane.module, imports).exports;
        memory = exports['memory'];
        return { exports, memory, failure };
    } catch (error) {
        return { error: failure(error) };
    }
}

// Runs the instance's start, or its reset when a job ran in it before, then
// prefix + entry with a copy of input and args, freeing the copy after when
// lane.free. Returns { result } or { error }.
export function enterLane(instance, lane, reset = false) {
    try {
        const exports = instance.exports;
        exports[reset ? kLaneResetExport : kLiftedStartExport]?.();
        const input = lane.input;
        const at = exports[kLaneAllocExport](input.length) >>> 0;
        new Uint8Array(instance.memory.buffer, at, input.length).set(input);
        const result = exports[lane.prefix + lane.entry](
            at,
            input.length,
            ...(lane.args ?? [])
        );
        if (lane.free) {
            exports[kLaneFreeExport](at);
        }
        return { result };
    } catch (error) {
        return { error: instance.failure(error) };
    }
}

// A new instance, entered. Returns { exports, memory, result, failure } or
// { error }.
export function startLane(lane) {
    const instance = instantiateLane(lane);
    const entered = instance.error === undefined ? enterLane(instance, lane) : instance;
    return entered.error === undefined ? { ...instance, result: entered.result } : entered;
}

// One script audio processor, whose init may take kProcessorInitMs. Returns
// { lane } or { error }; shared(memory, fail) answers its shared imports.
export function startProcessorLane(module, entry, input, format, shared, log) {
    // As natively, a native that fails the processor lets the block finish.
    let failed = null;
    const fail = (text) => {
        failed ??= text;
    };
    // When its fuel checks end it and why, made once so a block allocates
    // nothing on the audio thread.
    const bounds = { until: monotonicNow() + kProcessorInitMs, reason: kProcessorInitOverran };
    const started = startLane({
        module,
        shared: (memory) => shared(memory, fail),
        log,
        fuel: () => {
            if (monotonicNow() > bounds.until) {
                throw bounds.reason;
            }
        },
        prefix: kLaneEntryPrefixes['processor'],
        entry,
        input,
        args: [format.sampleRate, format.maxFrames, format.channels],
        free: true,
    });
    if (started.error !== undefined || failed !== null) {
        return { error: started.error ?? failed };
    }
    bounds.until = Infinity;
    bounds.reason = kProcessorOverran;
    const exports = started.exports;
    const memory = started.memory;
    // Rebuilt only when the block moves or memory grows, so a block
    // allocates nothing on the audio thread.
    let view = null;
    return {
        lane: {
            // Ends the block past budget ms, if any; sink gets its samples
            // before a collection can move them. Null, or why it failed.
            render(frames, startFrame, sink, budget = 0) {
                if (budget > 0) {
                    bounds.until = monotonicNow() + budget;
                }
                try {
                    const at = exports[kProcessorProcessExport](frames, startFrame) >>> 0;
                    if (failed !== null) {
                        return failed;
                    }
                    const length = frames * format.channels;
                    if (
                        view === null ||
                        view.buffer !== memory.buffer ||
                        view.byteOffset !== at ||
                        view.length !== length
                    ) {
                        view = new Float32Array(memory.buffer, at, length);
                    }
                    sink(view);
                    exports[kFrameMinorExport]?.();
                    return null;
                } catch (error) {
                    return started.failure(error);
                } finally {
                    bounds.until = Infinity;
                }
            },
        },
    };
}

// Every processor a real time output runs, as the page's messages say, with
// answers through post. It counts blocks and starts in holds, the Hold words.
export function createProcessorMixer(heap, table, post, holds) {
    const lanes = new Map();
    // What render walks, rebuilt only when lanes change, so a block does not
    // allocate an iterator.
    let order = [];
    const relist = () => {
        order = [...lanes.entries()];
    };
    // Null while compiling, then the module or the compile's error text,
    // until the page forgets it.
    const modules = new Map();
    // Starts not yet run, behind one whose module still compiles.
    const waiting = [];
    // The output's add and where in its block a chunk goes, set as render
    // walks, so sinks allocate nothing.
    let add = null;
    let offset = 0;
    let frame = -1;
    const fail = (id, text) => {
        lanes.delete(id);
        relist();
        post({ id, kind: 'failed', text });
    };
    const start = (message) => {
        const id = message.id;
        const module = modules.get(message.moduleId);
        if (typeof module === 'string') {
            post({ id, kind: 'failed', text: module });
            return;
        }
        const format = message.format;
        const started = startProcessorLane(
            module,
            message.entry,
            message.input,
            format,
            (moduleMemory, fail) =>
                createSharedLaneImports(heap, table, message.sharedOwner, moduleMemory, holds, fail),
            (text) => post({ id, kind: 'log', text })
        );
        if (started.error !== undefined) {
            post({ id, kind: 'failed', text: started.error });
            return;
        }
        const channels = format.channels;
        lanes.set(id, {
            lane: started.lane,
            maxFrames: format.maxFrames,
            // A block past its hard limit, where there is one, ends at its
            // next fuel check, or without fuel once it returns.
            hardBlocks: message.hardBlocks,
            sampleRate: format.sampleRate,
            warm: false,
            sink: (block) => add(block, channels, offset),
        });
        relist();
        post({ id, kind: 'running' });
    };
    // Starts in arrival order, up to the first whose module still compiles.
    const drain = () => {
        while (waiting.length > 0 && modules.get(waiting[0].moduleId) !== null) {
            start(waiting.shift());
            // Each start gets the page's whole start limit, and the checks it
            // took must not count under the shorter limit after it.
            Atomics.add(holds, kHoldBlocks, 1);
        }
        if (waiting.length === 0) {
            Atomics.store(holds, kHoldStarting, 0);
        }
    };
    return {
        receive(message) {
            if (message.kind === 'forget') {
                modules.delete(message.moduleId);
                return;
            }
            if (message.kind === 'stop') {
                // A stop beats the start it follows when that still waits.
                const at = waiting.findIndex((queued) => queued.id === message.id);
                if (at >= 0) {
                    waiting.splice(at, 1);
                    drain();
                } else {
                    lanes.delete(message.id);
                    relist();
                }
                return;
            }
            if (message.kind === 'end') {
                // The page gave up on this output and failed what it ran.
                lanes.clear();
                relist();
                waiting.length = 0;
                return;
            }
            const moduleId = message.moduleId;
            if (!modules.has(moduleId) && message.bytes === null) {
                // A fresh worklet lacks modules the page sent the one before.
                post({ id: message.id, kind: 'resend' });
                return;
            }
            if (!modules.has(moduleId)) {
                // Compiling on the audio thread would stall every processor.
                modules.set(moduleId, null);
                const settle = (value) => {
                    if (modules.has(moduleId)) {
                        modules.set(moduleId, value);
                    }
                };
                WebAssembly.compile(message.bytes)
                    .then(settle, (error) => settle(errorText(error)))
                    .then(drain);
            }
            Atomics.store(holds, kHoldStarting, 1);
            waiting.push(message);
            drain();
        },

        // Hands into(block, channels, offset) each processor's interleaved
        // samples, in chunks of at most its block, offset frames in.
        render(frames, firstFrame, into) {
            if (frame < 0) {
                frame = firstFrame;
            }
            add = into;
            // A failure relists, which must not shift the rest of this pass.
            const list = order;
            for (let i = 0; i < list.length; i++) {
                const [id, entry] = list[i];
                for (offset = 0; offset < frames; offset += entry.maxFrames) {
                    const chunk = Math.min(entry.maxFrames, frames - offset);
                    const budget = processorBlockBudgetMs(
                        entry.hardBlocks,
                        chunk,
                        entry.sampleRate,
                        !entry.warm
                    );
                    const began = monotonicNow();
                    let error = entry.lane.render(chunk, frame + offset, entry.sink, budget);
                    if (error === null && budget > 0 && monotonicNow() - began > budget) {
                        error = kProcessorOverran;
                    }
                    entry.warm = true;
                    if (error !== null) {
                        fail(id, error);
                        break;
                    }
                }
            }
            frame += frames;
            Atomics.add(holds, kHoldBlocks, 1);
        },

        // The output went away: every processor it ran ends with text.
        failAll(text) {
            for (const id of [...lanes.keys()]) {
                fail(id, text);
            }
            for (const message of waiting.splice(0)) {
                post({ id: message.id, kind: 'failed', text });
            }
        },
    };
}
