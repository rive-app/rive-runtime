// Drives the harness the way a web host loads a file: import, await the
// script runner's prepare(), then start scripts and run frames.
//   node run.mjs <harness.mjs> <file.riv> [--sync] [--early] [--settle]
//                [--decoder-throws] [--fault <method>] [--deep] [--vms <count>]
//                [--scroll] [--lacks simd|relaxed|bulk,...] [--until <log line>]
//                [--audio <blocks>] [--pump-audio] [--lane-twin] [--nrdp-mock]
//                [--hard-blocks <n>] [--bounded]
//                [--while-pending] [--nrdp-fault <start|silent|fetch>] [--nrdp-stall]
//                [--forget-modules] [--worklet-throws] [--page-boundary]
//                [--seam <address>] [--first <file.riv>] [--error-sink]
//                [--hash <blocks>] [--no-isolation] [--job-threads <n>]
//                [--parallel-parts <n>] [--parallel-safe] [--call-budget <ms>]
//                [--expect <log line>]... [--expect-line <log line>]...
// --no-isolation runs as a page without cross origin isolation would, which
// a build with threads refuses to load.
// --job-threads sets the job pool's threads. --parallel-parts sets the
// runner's parallelParts, the parts a parallel call gets on a build with
// threads. --parallel-safe runs every parallel call one part at a time.
// --call-budget sets how long a call into the file's scripts may run, 0 for
// no limit.
// --early starts scripts once before prepare() settles, as a host that does
// not wait for it would. --deep runs the frames with little stack left.
// --bounded starts only modules the page can bound, as an editor does.
// --vms is how many script vms must run, else one. --scroll sends one scroll
// over the artboard after the frames. --until keeps running a frame every
// few milliseconds until a line holds the text, for script on other lanes;
// given more than once, until each text shows.
// --while-pending runs them only while script work is pending, as a host
// that settles does.
// --audio then renders that many blocks offline through each audio processor
// and runs frames until every expected line shows. --pump-audio renders a block
// with every one of those frames instead. --lane-twin gives processors the
// shared memory natives an audio worklet uses, Atomics on librive's heap.
// --hard-blocks sets the audio hard limit, 0 for none, for real time runs a
// loaded machine must not drop a processor in.
// --nrdp-mock stands in NRDP's audio API (nrdp_mock.mjs), so processors run
// in real time through the runner's NRDP output and its worklet bundle.
// --nrdp-fault gives the mock that fault. --nrdp-stall freezes the mocked
// audio thread once a line "stall ring <id>" shows, as if it froze holding
// that ring's lock. --forget-modules then starts and stops processors of
// more modules than the page keeps, and reports which it forgot.
// --hash logs a hash of the first that many blocks the mock renders, as
// browser_audio.mjs does on a page.
// --worklet-throws stands in a browser whose worklet node refuses librive's
// memory.
// --settle stands in for the browser's image decoder and lets its results
// land before the file is released. --decoder-throws settles with a decoder
// that throws as librive starts it. --fault makes librive's first call to the
// named script runner method throw, a JS error raised under librive frames.
// --lacks stands in for an engine without those features, which node always
// has: it refuses any module whose code uses their opcodes. --page-boundary
// starts the scripts and services them once through the runner, as the JS
// runtime does at the end of a frame, before the harness runs its own frames.
// --seam round trips a string through librive's module memory seam at that
// address after the frames. --first loads, starts and releases that file on
// the same page before the one under test. --error-sink gives the files an
// error sink, which prefixes its lines with "error sink: ". --expect wants a
// line holding the text, --expect-line one that is exactly the text, once for
// each time it is given.
// Preloaded with node --import ./worker_compiles.mjs, the run also says how
// many modules the worker threads compiled.
import { readFileSync } from 'fs';
import { dirname, resolve } from 'path';
import { fileURLToPath, pathToFileURL } from 'url';
import { installNrdpMock } from './nrdp_mock.mjs';
import {
    kChunkShift,
    kChunkSlots,
    kHeaderLock,
    kHoldLock,
    kHoldToken,
    kIdBit,
    kIdSlotShift,
    kLaneFirstToken,
    kSlotBlock,
    kSlotSize,
    kTableChunks,
    kTableLanes,
} from '../../src/wasm/web/rive_shared_layout.mjs';

const [harnessPath, rivPath, ...rest] = process.argv.slice(2);
const sync = rest.includes('--sync');
const decoderThrows = rest.includes('--decoder-throws');
const settle = rest.includes('--settle') || decoderThrows;
const valueOf = (flag) => rest.find((arg, i) => rest[i - 1] === flag);
const expected = rest.filter((arg, i) => rest[i - 1] === '--expect');
const exactLines = rest.filter((arg, i) => rest[i - 1] === '--expect-line');
const fault = valueOf('--fault');
const early = rest.includes('--early');
const deep = rest.includes('--deep');
const vms = valueOf('--vms');
const scroll = rest.includes('--scroll');
const untils = rest.filter((arg, i) => rest[i - 1] === '--until');
const audio = valueOf('--audio');
const pumpAudio = rest.includes('--pump-audio');
const whilePending = rest.includes('--while-pending');
const lacks = valueOf('--lacks')?.split(',');
const hardBlocks = valueOf('--hard-blocks');
const pageBoundary = rest.includes('--page-boundary');
const seam = valueOf('--seam');
const bounded = rest.includes('--bounded');
const first = valueOf('--first');
const errorSink = rest.includes('--error-sink');

// The relaxed opcodes are 0xfd then 0x80 to 0x93 and 0x02, and memory.copy
// and memory.fill 0xfc then 0x0a and 0x0b; a stray byte match only refuses
// more. Only the code section counts, as an engine never validates the lane
// modules custom sections carry.
function usesSimd(bytes, relaxed) {
    const code = sections(bytes, 8).find((section) => section.id === 10);
    if (code === undefined) {
        return false;
    }
    for (let i = code.body; i + 2 < code.end; i++) {
        if (bytes[i] !== 0xfd) {
            continue;
        }
        if (!relaxed ||
            (bytes[i + 1] >= 0x80 && bytes[i + 1] <= 0x93 && bytes[i + 2] === 0x02)) {
            return true;
        }
    }
    return false;
}

function usesBulkMemory(bytes) {
    const code = sections(bytes, 8).find((section) => section.id === 10);
    if (code === undefined) {
        return false;
    }
    for (let i = code.body; i + 1 < code.end; i++) {
        if (bytes[i] === 0xfc && (bytes[i + 1] === 0x0a || bytes[i + 1] === 0x0b)) {
            return true;
        }
    }
    return false;
}

// The feature the engine lacks that the module uses, or null.
function refusal(bytes) {
    if (lacks.includes('bulk') && usesBulkMemory(bytes)) {
        return 'bulk memory';
    }
    if (lacks.includes('simd') && usesSimd(bytes, false)) {
        return 'SIMD';
    }
    if (lacks.includes('relaxed') && usesSimd(bytes, true)) {
        return 'relaxed SIMD';
    }
    return null;
}

function leb(bytes, at) {
    let value = 0;
    let shift = 0;
    let byte;
    do {
        byte = bytes[at++];
        value |= (byte & 0x7f) << shift;
        shift += 7;
    } while (byte & 0x80);
    return [value >>> 0, at];
}

// The sections of a module whose first one starts at first.
function sections(bytes, first) {
    const out = [];
    for (let at = first; at < bytes.length; ) {
        const [size, body] = leb(bytes, at + 1);
        out.push({ id: bytes[at], body, end: body + size });
        at = body + size;
    }
    return out;
}

if (settle && !decoderThrows) {
    // Accepts bytes that start with a PNG signature byte as a 2x1 image whose
    // first pixel is straight red at half alpha.
    globalThis.createImageBitmap = async (blob) => {
        const bytes = new Uint8Array(await blob.arrayBuffer());
        if (bytes[0] !== 0x89) {
            throw new Error('not an image');
        }
        return { width: 2, height: 1 };
    };
    globalThis.OffscreenCanvas = class {
        getContext() {
            return {
                drawImage() {},
                getImageData: () => ({
                    data: new Uint8ClampedArray([200, 0, 0, 128, 0, 0, 255, 255]),
                }),
            };
        }
    };
}
if (decoderThrows) {
    globalThis.createImageBitmap = () => {
        throw new Error('decoder threw');
    };
}

const nrdpFault = valueOf('--nrdp-fault') ?? null;
const nrdpStall = rest.includes('--nrdp-stall');
const forgetModules = rest.includes('--forget-modules');
const nrdpMock = rest.includes('--nrdp-mock') || nrdpFault !== null || nrdpStall;
let mock = null;
if (nrdpMock) {
    const web = resolve(dirname(fileURLToPath(import.meta.url)), '../../src/wasm/web');
    mock = installNrdpMock((url) => readFileSync(resolve(web, url), 'utf8'), nrdpFault);
}
// As a page without cross origin isolation, which has no shared memory.
if (rest.includes('--no-isolation')) {
    globalThis.crossOriginIsolated = false;
}
const workletThrows = rest.includes('--worklet-throws');
if (workletThrows) {
    globalThis.crossOriginIsolated = true;
    // The page's window, where the runner waits for a gesture.
    const page = new EventTarget();
    globalThis.addEventListener = page.addEventListener.bind(page);
    globalThis.removeEventListener = page.removeEventListener.bind(page);
    globalThis.AudioContext = class {
        constructor() {
            this.sampleRate = 48000;
            this.state = 'suspended';
            this.destination = {};
            this.audioWorklet = { addModule: async () => {} };
        }
        addEventListener() {}
        close() {}
    };
    globalThis.AudioWorkletNode = class {
        constructor() {
            throw new DOMException('memory could not be cloned', 'DataCloneError');
        }
    };
}
const lines = [];
const hash = valueOf('--hash');
if (hash !== undefined) {
    mock.tap(Number(hash), (sums, value) => {
        const [left, right] = sums.map((sum) => sum.toFixed(2));
        const hex = value.toString(16).padStart(8, '0');
        lines.push(`worklet rendered ${hash} blocks sums ${left} ${right} hash ${hex}`);
    });
}
const createHarness = (await import(pathToFileURL(resolve(harnessPath)))).default;
const Module = await createHarness({
    print: (text) => lines.push(text),
    printErr: (text) => lines.push(text),
    ...(lacks === undefined ? {} : {
        riveScriptingValidate: (bytes) =>
            refusal(bytes) === null && WebAssembly.validate(bytes),
    }),
    riveScriptingLaneTwin: rest.includes('--lane-twin'),
}).catch((error) => {
    // A build with threads refuses a page that cannot share memory, which
    // passes only a run that expects it.
    const line = `harness refused: ${error.message}`;
    const missing = expected.filter((want) => !line.includes(want));
    const wanted = (want) => exactLines.filter((other) => other === want).length;
    missing.push(...exactLines.filter((want) => want !== line || wanted(want) > 1));
    const passed = missing.length === 0 && expected.length + exactLines.length > 0;
    console[passed ? 'log' : 'error'](line);
    process.exit(passed ? 0 : 1);
});
if (nrdpMock) {
    Module.riveScripting.audioWorkletUrl = 'rive_audio_worklet_nrdp.js';
}
if (workletThrows) {
    Module.riveScripting.audioWorkletUrl = 'rive_audio_worklet.mjs';
}
// The engine refuses to compile them too, once librive itself is running.
if (lacks !== undefined) {
    const refuse = (bytes) => {
        const missing = refusal(new Uint8Array(bytes));
        if (missing !== null) {
            throw new WebAssembly.CompileError(`this engine lacks ${missing}`);
        }
    };
    const { Module: WasmModule, compile } = WebAssembly;
    WebAssembly.Module = class extends WasmModule {
        constructor(bytes) {
            refuse(bytes);
            super(bytes);
        }
    };
    WebAssembly.compile = async (bytes) => {
        refuse(bytes);
        return compile(bytes);
    };
}
// Counts the script modules the runner tries to instantiate.
let instantiations = 0;
const { Instance, instantiate } = WebAssembly;
WebAssembly.Instance = class extends Instance {
    constructor(...args) {
        instantiations++;
        super(...args);
    }
};
WebAssembly.instantiate = (...args) => {
    instantiations++;
    return instantiate(...args);
};
if (fault !== undefined) {
    const runner = Module.riveScripting;
    const original = runner[fault];
    runner[fault] = () => {
        runner[fault] = original;
        throw new Error('host fault injected');
    };
}

if (bounded) {
    Module._harness_require_bounds();
}
if (hardBlocks !== undefined) {
    Module._harness_audio_hard_blocks(Number(hardBlocks));
}
if (errorSink) {
    Module._harness_error_sink();
}
const jobThreads = valueOf('--job-threads');
if (jobThreads !== undefined) {
    Module._harness_job_threads(Number(jobThreads));
}
if (rest.includes('--parallel-safe')) {
    Module._harness_parallel_safe();
}
Module.riveScripting.parallelParts = valueOf('--parallel-parts');
const callBudget = valueOf('--call-budget');
if (callBudget !== undefined) {
    Module._harness_call_budget(Number(callBudget));
}
function importRiv(path) {
    const bytes = readFileSync(path);
    const ptr = Module._malloc(bytes.length);
    Module.HEAPU8.set(bytes, ptr);
    const imported = Module._harness_import(ptr, bytes.length);
    Module._free(ptr);
    if (!imported) {
        console.error('FAIL: import failed\n' + lines.join('\n'));
        process.exit(1);
    }
    return bytes;
}
if (first !== undefined) {
    importRiv(first);
    if (!sync) {
        await Module.riveScripting.prepare();
    }
    Module._harness_run(0);
    Module._harness_release();
}
// Only the file under test counts.
instantiations = 0;
const riv = importRiv(rivPath);
if (audio !== undefined || pumpAudio) {
    Module._harness_render_audio_offline();
}
// Skipping prepare exercises the inline instantiate fallback.
if (early) {
    const preparing = Module.riveScripting.prepare();
    Module._harness_run(0);
    await preparing;
} else if (!sync) {
    const stack = Module._harness_stack();
    await Module.riveScripting.prepare();
    const kept = Module._harness_stack() === stack;
    lines.push(`librive stack ${kept ? 'kept' : 'leaked'} across prepare`);
}
if (scroll) {
    Module._harness_scroll();
}
if (pageBoundary) {
    Module._harness_run(0);
    // The module exports librive calls while the runner services the VMs.
    const runner = Module.riveScripting;
    const call = runner.call;
    const called = [];
    runner.call = (vm, name, ...args) => {
        const end = Module.HEAPU8.indexOf(0, name);
        called.push(new TextDecoder().decode(Module.HEAPU8.subarray(name, end)));
        return call(vm, name, ...args);
    };
    const warnings = runner.frameBoundary();
    runner.call = call;
    lines.push(`page boundary called ${called.join(' ')}`);
    lines.push(`page boundary warned ${JSON.stringify(warnings)}`);
}
const running = deep
    ? (Module._harness_run(0), Module._harness_run_low_on_stack(5, 16384))
    : Module._harness_run(5);
if (seam !== undefined) {
    Module._harness_seam(Number(seam));
}
// The heap words of the ring the stall lane names, once frozen.
let stall = null;
const freezeOnStall = () => {
    const ring = lines.map((line) => /^stall ring (\d+)$/.exec(line)).find(Boolean);
    const holds = mock.options !== null ? new Int32Array(mock.options.counter) : null;
    if (stall !== null || ring === undefined || holds === null) {
        return;
    }
    const token = Atomics.load(holds, kHoldToken);
    const table = mock.options.table >>> 0;
    const u32 = new Uint32Array(mock.options.memory.buffer);
    const index = (Number(ring[1]) & ~kIdBit) >>> kIdSlotShift;
    const chunk = u32[(table + kTableChunks + (index >>> kChunkShift) * 4) >>> 2];
    const slot = chunk + (index & (kChunkSlots - 1)) * kSlotSize;
    const lockAt = u32[(slot + kSlotBlock) >>> 2] + kHeaderLock;
    Atomics.store(u32, lockAt >>> 2, token);
    Atomics.store(holds, kHoldLock, lockAt);
    mock.freeze();
    stall = { u32, lockAt, laneAt: ((table + kTableLanes) >>> 2) + token - kLaneFirstToken };
};
// A deadline only a stuck lane misses, so a loaded machine only slows a run.
const runUntil = async (done) => {
    const deadline = Date.now() + 30000;
    while (!done() && Date.now() < deadline) {
        await new Promise((settle) => setTimeout(settle, 5));
        if (nrdpStall) {
            freezeOnStall();
        }
        Module._harness_frames(1);
        if (pumpAudio) {
            Module._harness_audio_pump(1);
        }
    }
};
if (untils.length > 0) {
    await runUntil(() =>
        untils.every((until) => lines.some((line) => line.includes(until)))
    );
}
if (whilePending) {
    await runUntil(() => !Module._harness_pending());
}
if (pumpAudio) {
    await runUntil(() =>
        expected.every((want) => lines.some((line) => line.includes(want)))
    );
}
if (audio !== undefined) {
    // A processor still in its init counts as pending, and each one's state
    // goes in the lines.
    await runUntil(() => !Module._harness_pending());
    Module._harness_audio(Number(audio));
    await runUntil(() =>
        expected.every((want) => lines.some((line) => line.includes(want)))
    );
}
if (stall !== null) {
    const yes = (ok) => (ok ? 'yes' : 'no');
    lines.push(`stall thread terminated ${yes(mock.terminated)}`);
    lines.push(`stall lock freed ${yes(Atomics.load(stall.u32, stall.lockAt >>> 2) === 0)}`);
    lines.push(`stall lane freed ${yes(Atomics.load(stall.u32, stall.laneAt) === 0)}`);
}
if (forgetModules) {
    const runner = Module.riveScripting;
    const bytes = laneSection(riv, 'rive.audio');
    const format = { sampleRate: 48000, maxFrames: 128, channels: 2 };
    const start = (moduleId) =>
        runner.audioStart(0, moduleId, () => bytes, 0, new Uint8Array(0), format, 4).id;
    for (let i = 0; i < 10; i++) {
        runner.audioStop(start(9000 + i));
    }
    const forgot = mock.sent.filter((message) => message.kind === 'forget');
    lines.push('forgot modules ' + forgot.map((message) => message.moduleId).join(' '));
    runner.audioStop(start(9000));
    const resent = mock.sent.filter((message) => message.kind === 'start' && message.moduleId === 9000);
    lines.push(`forgotten module resent ${resent.at(-1).bytes !== null ? 'yes' : 'no'}`);
}
lines.push(`pending async work ${Module._harness_pending() ? 'yes' : 'no'}`);
// Set by worker_compiles.mjs. A count only means something once workers
// made instances, which shows the probe reached them.
const workerCompiles = globalThis.riveWorkerCompiles;
if (workerCompiles !== undefined && Atomics.load(workerCompiles, 1) > 0) {
    lines.push(`worker threads compiled ${Atomics.load(workerCompiles, 0)} modules`);
}
if (settle) {
    for (let turn = 0; turn < 5; turn++) {
        await new Promise((done) => setTimeout(done, 10));
    }
}
Module._harness_release();
lines.push(`script modules instantiated: ${instantiations}`);
lines.push(`parallel team ${JSON.stringify(Module.riveScripting.parallelState())}`);

// A custom section of the riv's script module.
function laneSection(bytes, name) {
    for (const section of sections(bytes, bytes.indexOf(Buffer.from('\0asm')) + 8)) {
        const [length, start] = leb(bytes, section.body);
        if (section.id === 0 && bytes.toString('utf8', start, start + length) === name) {
            return bytes.subarray(start + length, section.end);
        }
    }
    return null;
}

const missing = expected.filter((want) => !lines.some((line) => line.includes(want)));
const times = (list, want) => list.filter((item) => item === want).length;
missing.push(...exactLines.filter((want) => times(lines, want) < times(exactLines, want)));
if ((vms === undefined ? running < 1 : running !== Number(vms)) ||
    missing.length > 0) {
    console.error(`FAIL: ${running} script vms running, missing log lines: ${JSON.stringify(missing)}`);
    console.error(lines.join('\n'));
    process.exit(1);
}
console.log(`web scripting ok (${sync ? 'inline' : 'prepared'}): ${running} vm, ${lines.length} log lines`);
for (const line of lines) {
    console.log('  | ' + line);
}
// A mocked audio thread and its watchdog would keep node running.
process.exit(0);
