// Checks the page side of the browser lane without librive.
//   node web_host_test.mjs
import assert from 'assert';
import {
    bindRiveHostCalls,
    createRiveModuleImports,
} from '../../src/wasm/web/rive_module_imports.mjs';
import {
    createProcessorMixer,
    enterLane,
    instantiateLane,
    startProcessorLane,
} from '../../src/wasm/web/rive_lane.mjs';
import {
    kLaneEntryPrefixes,
    kProcessorInitMs,
    kProcessorInitOverran,
    kProcessorNode,
    kProcessorOverran,
    processorBlockBudgetMs,
} from '../../src/wasm/web/rive_lane_imports.mjs';
import {
    importMemory,
    installRiveScripting,
} from '../../src/wasm/web/rive_script_runner.mjs';
import { kHoldBlocks, kHoldStarting } from '../../src/wasm/web/rive_shared_layout.mjs';
import { readFileSync } from 'fs';
import { installNrdpMock } from './nrdp_mock.mjs';

function leb(value) {
    const out = [];
    do {
        const byte = value & 0x7f;
        value >>>= 7;
        out.push(value ? byte | 0x80 : byte);
    } while (value);
    return out;
}

const bytesOf = (text) => Array.from(text, (c) => c.charCodeAt(0));
const section = (id, body) => [id, ...leb(body.length), ...body];
const vector = (items) => [...leb(items.length), ...items.flat()];
const named = (text) => [...leb(text.length), ...bytesOf(text)];
const moduleOf = (...sections) =>
    new Uint8Array([0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0, ...sections.flat()]);
const custom = (name, payload) => section(0, [...named(name), ...payload]);

// A wasm i32 arrives in JS signed, so a u32 length of 2^32 - 1 is -1.
{
    let staged = 0;
    const host = {
        calls: { rive_shared_region_write: () => 1 },
        heapU8: () => new Uint8Array(1024),
        stackSave: () => 0,
        stackAlloc: () => (staged++, 16),
        stackRestore: () => {},
        malloc: () => (staged++, 16),
        free: () => {},
    };
    const memory = { buffer: new ArrayBuffer(65536) };
    const shared = createRiveModuleImports(host, () => memory)['rive_shared_v1'];
    assert.throws(() => shared['region_write'](1, 0, 8, -1), RangeError);
    assert.throws(() => shared['region_write'](1, 0, -8, 4), RangeError);
    assert.equal(staged, 0, 'nothing staged for a refused range');
    assert.equal(shared['region_write'](1, 0, 8, 4), 1);
    assert.equal(staged, 1);
    // A string whose length runs past memory traps as on WAMR.
    new DataView(memory.buffer).setUint32(60, 65536, true);
    const env = createRiveModuleImports(host, () => memory)['env'];
    assert.throws(() => env['console.log'](64), /out of bounds memory access/);
}

// A lane needs only the shared namespace, so only its host calls are bound.
{
    let all = 0;
    bindRiveHostCalls(() => (all++, () => 0), { vm: 1, raised: null });
    let bound = 0;
    const calls = bindRiveHostCalls(() => (bound++, () => 0), { vm: 1, raised: null },
        'rive_shared_v1');
    assert.ok(bound > 0 && bound < all, `bound ${bound} of ${all}`);
    assert.equal(typeof calls.rive_shared_region_write, 'function');
    assert.equal(calls.rive_console, false);
    const imports = createRiveModuleImports({ calls }, () => null, 'rive_shared_v1');
    assert.equal(typeof imports['rive_shared_v1']['atomic_load'], 'function');
    assert.equal(imports['rive_rt_v1'], false);
}

// An engine without relaxed SIMD validates only the main module, so the page
// reads the lane sections, past a custom section whose name would overflow
// a spread into String.fromCharCode. The lane's code is a relaxed madd.
{
    const lane = moduleOf(
        section(1, vector([[0x60, 0, 0]])),
        section(3, vector([[0]])),
        section(10, vector([[5, 0, 0xfd, 0x85, 0x02, 0x0b]]))
    );
    const Module = {
        riveScriptingValidate: (bytes) =>
            !(bytes.length < 100 && bytes.includes(0x80)) &&
            !(bytes.length === lane.length && bytes[0] === lane[0]),
    };
    const runner = installRiveScripting(Module);
    const long = 'x'.repeat(1 << 20);
    runner.register(1, moduleOf(custom(long, []), custom('rive.audio', lane)));
    assert.match(runner.start(1), /lacks WebAssembly relaxed SIMD/);
}

// A parallel module's shared memory becomes an import the runner fills: shared
// and set aside no larger than 2 GiB where threads can share it, halved while
// the engine refuses, unshared without isolation, and no other module changes.
{
    const parallel = (flags, maximum) =>
        moduleOf(
            section(1, vector([[0x60, 0, 1, 0x7f]])),
            section(2, vector([[...named('rive_parallel_v1'), ...named('size'), 0, 0]])),
            section(5, vector([[flags, 1, ...(flags & 1 ? leb(maximum) : [])]])),
            section(7, vector([[...named('memory'), 2, 0]]))
        );
    const Real = WebAssembly.Memory;
    const made = [];
    let refuseAbove = Infinity;
    WebAssembly.Memory = function (descriptor) {
        made.push(descriptor);
        if (descriptor.shared && descriptor.maximum > refuseAbove) {
            throw new RangeError('could not set aside the memory');
        }
        return new Real(descriptor);
    };
    const run = (bytes) => {
        const out = importMemory(bytes);
        const instance = new WebAssembly.Instance(new WebAssembly.Module(out.bytes), {
            rive_parallel_v1: { size: () => 1, memory: out.memory },
        });
        assert.equal(instance.exports.memory, out.memory);
        return out;
    };
    try {
        const shared = run(parallel(3, 65536));
        assert.ok(shared.memory.buffer instanceof SharedArrayBuffer);
        assert.deepEqual(made.at(-1), { initial: 1, maximum: 32768, shared: true });
        // wasmMaxPages caps it lower, and the engine's refusals halve it.
        run(parallel(3, 300));
        assert.equal(made.at(-1).maximum, 300);
        refuseAbove = 4096;
        made.length = 0;
        run(parallel(3, 65536));
        assert.deepEqual(made.map((d) => d.maximum), [32768, 16384, 8192, 4096]);
        refuseAbove = Infinity;
        globalThis.crossOriginIsolated = false;
        const unshared = run(parallel(3, 65536));
        assert.ok(!(unshared.memory.buffer instanceof SharedArrayBuffer));
        assert.deepEqual(made.at(-1), { initial: 1, maximum: 65536 });
        // A module whose memory is not shared is left as it was.
        const plain = parallel(1, 300);
        assert.equal(importMemory(plain).bytes, plain);
        assert.equal(importMemory(plain).memory, null);
    } finally {
        WebAssembly.Memory = Real;
        delete globalThis.crossOriginIsolated;
    }
}

// A job lane module with a start function, its result words [at, length] at
// 16, and the bytes librive hands over for it once its start is lifted.
function jobModule(at, length, lifted) {
    const exports = [
        [...named('memory'), 2, 0],
        [...named('__rive_lane_alloc'), 0, 0],
        [...named('__rive_job_0'), 0, 1],
    ];
    const words = [...new Uint8Array(Uint32Array.of(at, length).buffer)];
    return moduleOf(
        section(1, vector([[0x60, 1, 0x7f, 1, 0x7f], [0x60, 2, 0x7f, 0x7f, 1, 0x7f], [0x60, 0, 0]])),
        section(3, vector([[0], [1], [2]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector(lifted ? [...exports, [...named('__riveLiftedStart'), 0, 2]] : exports)),
        lifted ? [] : section(8, [2]),
        section(10, vector([[5, 0, 0x41, 0x80, 0x08, 0x0b], [4, 0, 0x41, 16, 0x0b], [2, 0, 0x0b]])),
        section(11, vector([[0, 0x41, 16, 0x0b, ...leb(words.length), ...words]]))
    );
}

function pageHost() {
    return {
        hostCall: () => () => 0,
        heapU8: () => new Uint8Array(64),
        stackSave: () => 0,
        stackAlloc: () => 0,
        stackRestore: () => {},
        exports: {
            _rive_web_vm_booting: () => {},
            _rive_web_vm_start_fuel: () => 0,
            _rive_web_vm_start_refusal: () => 0,
        },
        laneLog: () => {},
    };
}

async function compilesDuring(run) {
    const { compile, Module } = WebAssembly;
    let compiles = 0;
    WebAssembly.compile = (bytes) => (compiles++, compile(bytes));
    WebAssembly.Module = new Proxy(Module, {
        construct: (target, args) => (compiles++, new target(...args)),
    });
    try {
        await run();
    } finally {
        WebAssembly.compile = compile;
        WebAssembly.Module = Module;
    }
    return compiles;
}

// How many modules this thread compiles while a job of the lane module runs.
const compilesRunningJob = (runner) =>
    compilesDuring(() => {
        const ran = runner['runJob'](1, 9, () => jobModule(0, 0, true), 0, new Uint8Array(0), () => 0);
        assert.equal(ran.ok, true, ran.error);
    });

// A job lane prepare() compiled is reused when librive starts it, so no
// thread compiles it again on its own.
{
    const runner = installRiveScripting({});
    runner.attach(pageHost());
    runner.register(1, moduleOf(custom('rive.jobs', [...jobModule(0, 0, false)])));
    assert.deepEqual(await runner.prepare(), [undefined]);
    assert.equal(await compilesRunningJob(runner), 0, 'the prepared lane module is reused');
}

// A start without prepare() compiles the lane module too, once, and hands it
// to the idle threads as prepare() does.
{
    const runner = installRiveScripting({});
    runner.attach(pageHost());
    const posted = [];
    runner.attachThreads(() => [{ postMessage: (message) => posted.push(message['riveLane']) }]);
    runner.register(1, moduleOf(custom('rive.jobs', [...jobModule(0, 0, false)])));
    assert.equal(runner.start(1), null);
    assert.equal(posted.length, 1);
    assert.ok(posted[0]['module'] instanceof WebAssembly.Module);
    assert.deepEqual(posted[0]['bytes'], jobModule(0, 0, true));
    assert.equal(await compilesRunningJob(runner), 0, 'the lane module start compiled is reused');
    // A second file carrying the same lane module neither compiles nor posts.
    runner.register(2, moduleOf(custom('rive.jobs', [...jobModule(0, 0, false)])));
    assert.equal(runner.start(2), null);
    assert.equal(posted.length, 1);
}

// A module the host refuses compiles none of its lane sections and hands none
// to the threads, whether prepare() or a start meets it first.
for (const prepared of [true, false]) {
    const runner = installRiveScripting({});
    runner.attach({
        ...pageHost(),
        utf8: () => 'refused',
        exports: { ...pageHost().exports, _rive_web_vm_start_refusal: () => 1 },
    });
    const posted = [];
    runner.attachThreads(() => [{ postMessage: (message) => posted.push(message) }]);
    runner.register(1, moduleOf(custom('rive.jobs', [...jobModule(0, 0, false)])));
    const compiles = await compilesDuring(async () => {
        if (prepared) {
            await runner.prepare();
        }
        assert.equal(runner.start(1), 'refused');
    });
    assert.equal(compiles, 1, `prepared ${prepared}: only the main module compiles`);
    assert.deepEqual(posted, []);
}

// A main module whose start asks the host for its job threads, then traps if
// told to, and whose probe export answers 7.
function startingModule(traps) {
    return moduleOf(
        section(1, vector([[0x60, 0, 0], [0x60, 0, 1, 0x7f]])),
        section(2, vector([[...named('rive_jobs_v1'), ...named('threads'), 0, 1]])),
        section(3, vector([[0], [1]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector([[...named('memory'), 2, 0], [...named('probe'), 0, 2]])),
        section(8, [1]),
        section(10, vector([
            [traps ? 6 : 5, 0, 0x10, 0, 0x1a, ...(traps ? [0x00] : []), 0x0b],
            [4, 0, 0x41, 7, 0x0b],
        ]))
    );
}

// A host call made while a module starts reaches its exports, as a wait in a
// top level moves fuel on through one. A start that trapped leaves none.
{
    const kOk = 0;
    const kMissing = 1;
    const runner = installRiveScripting({});
    const result = new Float64Array(1);
    const probe = (vm) => runner.call(vm, 0, 0, 0, 0, 0, 0);
    const outcomes = [];
    runner.attach({
        ...pageHost(),
        hostCall: () => (vm) => outcomes.push(probe(vm)),
        utf8: () => 'probe',
        heapF64: () => result,
        writeUtf8: () => {},
        exports: { ...pageHost().exports, _rive_web_vm_module_export: () => {} },
    });
    runner.register(1, startingModule(false));
    assert.equal(runner.start(1), null);
    runner.register(2, startingModule(true));
    assert.match(runner.start(2), /unreachable/);
    assert.deepEqual([...outcomes, result[0], probe(2)], [kOk, kOk, 7, kMissing]);
}

// An empty result may point anywhere, as librive never reads it.
{
    const runner = installRiveScripting({});
    runner.attach(pageHost());
    const ran = runner['runJob'](1, 10, () => jobModule(1 << 20, 0, true), 0, new Uint8Array(0),
        () => 0);
    assert.equal(ran.ok, true, ran.error);
    assert.equal(ran.output.length, 0);
}

// A job lane module whose job counts its runs in memory and answers the
// count, and whose reset, when it has one, zeroes it.
function countingJobModule(resets) {
    const exports = [
        [...named('memory'), 2, 0],
        [...named('__rive_lane_alloc'), 0, 0],
        [...named('__rive_job_0'), 0, 1],
        ...(resets ? [[...named('__riveLaneReset'), 0, 2]] : []),
    ];
    const words = [...new Uint8Array(Uint32Array.of(32, 4).buffer)];
    return moduleOf(
        section(1, vector([[0x60, 1, 0x7f, 1, 0x7f], [0x60, 2, 0x7f, 0x7f, 1, 0x7f], [0x60, 0, 0]])),
        section(3, vector([[0], [1], [2]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector(exports)),
        section(10, vector([
            [5, 0, 0x41, 0x80, 0x08, 0x0b],
            [17, 0, 0x41, 32, 0x41, 32, 0x28, 2, 0, 0x41, 1, 0x6a, 0x36, 2, 0, 0x41, 16, 0x0b],
            [9, 0, 0x41, 32, 0x41, 0, 0x36, 2, 0, 0x0b],
        ])),
        section(11, vector([[0, 0x41, 16, 0x0b, ...leb(words.length), ...words]]))
    );
}

// A job leaves its instance for the next on its thread, which resets it
// first. A stopped job leaves none, nor does a module without a reset.
{
    const runner = installRiveScripting({});
    runner.attach(pageHost());
    const Instance = WebAssembly.Instance;
    let instances = 0;
    WebAssembly.Instance = new Proxy(Instance, {
        construct: (target, args) => (instances++, new target(...args)),
    });
    try {
        const count = (id, resets, cancelled = () => 0) => {
            const ran = runner['runJob'](1, id, () => countingJobModule(resets), 0,
                new Uint8Array(0), cancelled);
            assert.equal(ran.ok, true, ran.error);
            return new Uint32Array(ran.output.slice().buffer)[0];
        };
        assert.deepEqual([count(20, true), count(20, true), count(20, true)], [1, 1, 1]);
        assert.equal(instances, 1, 'jobs reuse the instance');
        count(20, true, () => 1);
        assert.equal(count(20, true), 1);
        assert.equal(instances, 2, 'a stopped job leaves no instance');
        instances = 0;
        assert.deepEqual([count(21, false), count(21, false)], [1, 1]);
        assert.equal(instances, 2, 'a module without a reset starts anew');
    } finally {
        WebAssembly.Instance = Instance;
    }
}

// A metered job lane module with a reset: job 0 returns at once, and job 1
// runs its fuel check until the check throws.
function meteredJobModule() {
    return moduleOf(
        section(1, vector([[0x60, 0, 0], [0x60, 1, 0x7f, 1, 0x7f], [0x60, 2, 0x7f, 0x7f, 1, 0x7f]])),
        section(2, vector([[...named('rive_lane_v1'), ...named('fuel_check'), 0, 0]])),
        section(3, vector([[1], [2], [2], [0]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector([
            [...named('memory'), 2, 0],
            [...named('__rive_lane_alloc'), 0, 1],
            [...named('__rive_job_0'), 0, 2],
            [...named('__rive_job_1'), 0, 3],
            [...named('__riveLaneReset'), 0, 4],
        ])),
        section(10, vector([
            [5, 0, 0x41, 0x80, 0x08, 0x0b],
            [4, 0, 0x41, 16, 0x0b],
            [10, 0, 0x03, 0x40, 0x10, 0, 0x0c, 0, 0x0b, 0x00, 0x0b],
            [2, 0, 0x0b],
        ]))
    );
}

// A job's fuel check throws what librive answers for the job running in the
// instance, which fails that job and leaves no instance for the next.
{
    const asked = [];
    const runner = installRiveScripting({});
    runner.attach({
        ...pageHost(),
        laneStop: (lane) => (asked.push(lane), asked.length < 3 ? null : 'stopped by its host'),
    });
    const Instance = WebAssembly.Instance;
    let instances = 0;
    WebAssembly.Instance = new Proxy(Instance, {
        construct: (target, args) => (instances++, new target(...args)),
    });
    try {
        const run = (lane, entry) =>
            runner['runJob'](lane, 30, () => meteredJobModule(), entry, new Uint8Array(0), () => 0);
        assert.equal(run(1, 0).ok, true);
        assert.deepEqual(run(2, 1), { ok: false, error: 'stopped by its host' });
        assert.deepEqual(asked, [2, 2, 2]);
        assert.equal(run(3, 0).ok, true);
        assert.equal(instances, 2, 'a job its fuel check ended leaves no instance');
    } finally {
        WebAssembly.Instance = Instance;
    }
}

// A job lane module with 2 GiB and a page of memory. Its input's words are a
// string's address and byte length: job 0 logs the string, job 1 throws it.
const speakingModule = new WebAssembly.Module(moduleOf(
    section(1, vector([
        [0x60, 1, 0x7f, 0],
        [0x60, 2, 0x7f, 0x7f, 0],
        [0x60, 1, 0x7f, 1, 0x7f],
        [0x60, 2, 0x7f, 0x7f, 1, 0x7f],
    ])),
    section(2, vector([
        [...named('env'), ...named('console.log'), 0, 0],
        [...named('rive_lane_v1'), ...named('error'), 0, 1],
    ])),
    section(3, vector([[2], [3], [3]])),
    section(5, vector([[0x00, ...leb(32769)]])),
    section(7, vector([
        [...named('memory'), 2, 0],
        [...named('__rive_lane_alloc'), 0, 2],
        [...named('__rive_job_0'), 0, 3],
        [...named('__rive_job_1'), 0, 4],
    ])),
    section(10, vector([
        [5, 0, 0x41, 0x80, 0x08, 0x0b],
        [11, 0, 0x20, 0, 0x28, 2, 0, 0x10, 0, 0x41, 16, 0x0b],
        [15, 0, 0x20, 0, 0x28, 2, 0, 0x20, 0, 0x28, 2, 4, 0x10, 1, 0x00, 0x0b],
    ]))
));
const kOddText = 67;
const kHighText = 2 ** 31 + 64;
const kMarkText = 128;

// A job or worker lane that reads the clock instantiates, and its clock moves
// forward.
{
    const clockModule = new WebAssembly.Module(moduleOf(
        section(1, vector([[0x60, 0, 1, 0x7c]])),
        section(2, vector([[...named('rive_rt_v1'), ...named('now'), 0, 0]])),
        section(3, vector([[0]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector([[...named('memory'), 2, 0], [...named('now'), 0, 1]])),
        section(10, vector([[4, 0, 0x10, 0, 0x0b]]))
    ));
    const instance = instantiateLane({ module: clockModule, shared: () => ({}), log: () => {} });
    assert.equal(instance.error, undefined);
    const first = instance.exports['now']();
    assert.ok(instance.exports['now']() >= first);
}

// What a lane logs and fails with for the string at ptr: "odd" sits at an
// odd address, "high" past 2 GiB, and "mark" opens with a byte order mark.
function speak(entry, ptr, byteLength = 0) {
    const lines = [];
    const instance = instantiateLane({
        module: speakingModule,
        shared: () => ({}),
        log: (line) => lines.push(line),
    });
    const view = new DataView(instance.memory.buffer);
    const texts = [[kOddText, 'odd'], [kHighText, 'high'], [kMarkText, '\uFEFFmark']];
    for (const [at, text] of texts) {
        view.setUint32(at - 4, text.length * 2, true);
        bytesOf(text).forEach((unit, i) => view.setUint16(at + i * 2, unit, true));
    }
    const input = new Uint8Array(Uint32Array.of(ptr, byteLength).buffer);
    const entered = enterLane(instance, { prefix: kLaneEntryPrefixes['job'], entry, input });
    return { lines, error: entered.error };
}

// A lane logs and throws a null string and each of those, and traps on one
// outside its memory. A worklet may lack TextDecoder.
for (const decoder of [globalThis.TextDecoder, undefined]) {
    const TextDecoder = globalThis.TextDecoder;
    globalThis.TextDecoder = decoder;
    try {
        assert.deepEqual(speak(0, 0), { lines: [''], error: undefined });
        assert.deepEqual(speak(0, kOddText), { lines: ['odd'], error: undefined });
        assert.deepEqual(speak(0, kHighText), { lines: ['high'], error: undefined });
        assert.deepEqual(speak(0, kMarkText), { lines: ['\uFEFFmark'], error: undefined });
        assert.deepEqual(speak(0, kHighText + 65536), {
            lines: [],
            error: 'out of bounds memory access',
        });
        assert.equal(speak(1, 0).error, '');
        assert.equal(speak(1, kOddText, 6).error, 'odd');
        assert.equal(speak(1, kHighText, 8).error, 'high');
        assert.equal(speak(1, kMarkText, 10).error, '\uFEFFmark');
        assert.equal(speak(1, kHighText, 65536).error, 'out of bounds memory access');
        assert.equal(speak(1, kOddText, -2).error, 'out of bounds memory access');
    } finally {
        globalThis.TextDecoder = TextDecoder;
    }
}

// A processor whose entry and process do nothing, its block at 64.
const processorModule = moduleOf(
    section(1, vector([
        [0x60, 1, 0x7f, 1, 0x7f],
        [0x60, 1, 0x7f, 0],
        [0x60, 5, 0x7f, 0x7f, 0x7c, 0x7f, 0x7f, 0],
        [0x60, 2, 0x7f, 0x7c, 1, 0x7f],
    ])),
    section(3, vector([[0], [1], [2], [3]])),
    section(5, vector([[0x00, 1]])),
    section(7, vector([
        [...named('memory'), 2, 0],
        [...named('__rive_lane_alloc'), 0, 0],
        [...named('__rive_lane_free'), 0, 1],
        [...named('__rive_processor_0'), 0, 2],
        [...named('__rive_processor_process'), 0, 3],
    ])),
    section(10, vector([
        [4, 0, 0x41, 16, 0x0b],
        [2, 0, 0x0b],
        [2, 0, 0x0b],
        [5, 0, 0x41, 0xc0, 0x00, 0x0b],
    ]))
);

{
    const heap = new SharedArrayBuffer(65536);
    const holds = new Int32Array(new SharedArrayBuffer(16));
    const posted = [];
    const mixer = createProcessorMixer(() => heap, 1024, (message) => posted.push(message), holds);
    const start = (id, moduleId, bytes) =>
        mixer.receive({
            kind: 'start',
            id,
            moduleId,
            bytes,
            entry: 0,
            input: new Uint8Array(0),
            format: { sampleRate: 48000, maxFrames: 128, channels: 2 },
            hardBlocks: 4,
        });
    // A fresh worklet asks for the bytes of a module only its last one had.
    start(1, 7, null);
    assert.deepEqual(posted.shift(), { id: 1, kind: 'resend' });

    start(2, 8, processorModule);
    for (let turn = 0; turn < 50 && posted.length === 0; turn++) {
        await new Promise((settle) => setTimeout(settle, 5));
    }
    assert.deepEqual(posted.shift(), { id: 2, kind: 'running' });
    let blocks = 0;
    mixer.render(128, 0, () => blocks++);
    assert.equal(blocks, 1);
    // The page gave up on the output, so nothing it ran renders again.
    mixer.receive({ kind: 'end' });
    mixer.render(128, 128, () => blocks++);
    assert.equal(blocks, 1);
    assert.deepEqual(posted, []);
}

// A block past the hard limit drops its processor, and a limit of 0 is none.
for (const hardBlocks of [4, 0]) {
    const heap = new SharedArrayBuffer(65536);
    const holds = new Int32Array(new SharedArrayBuffer(16));
    const posted = [];
    const mixer = createProcessorMixer(() => heap, 1024, (message) => posted.push(message), holds);
    mixer.receive({
        kind: 'start',
        id: 1,
        moduleId: 9,
        bytes: processorModule,
        entry: 0,
        input: new Uint8Array(0),
        format: { sampleRate: 48000, maxFrames: 128, channels: 2 },
        hardBlocks,
    });
    for (let turn = 0; turn < 50 && posted.length === 0; turn++) {
        await new Promise((settle) => setTimeout(settle, 5));
    }
    assert.deepEqual(posted.shift(), { id: 1, kind: 'running' });
    assert.equal(processorBlockBudgetMs(hardBlocks, 128, 48000, false), hardBlocks === 0 ? 0 : 11);
    let blocks = 0;
    mixer.render(128, 0, () => blocks++);
    // The block hands its samples over mid call, so a slow sink is a slow block.
    const slow = () => {
        blocks++;
        for (const until = Date.now() + 40; Date.now() < until; ) {}
    };
    mixer.render(128, 128, slow);
    mixer.render(128, 256, () => blocks++);
    if (hardBlocks === 0) {
        assert.equal(blocks, 3);
        assert.deepEqual(posted, []);
    } else {
        assert.equal(blocks, 2);
        assert.deepEqual(posted, [{ id: 1, kind: 'failed', text: kProcessorOverran }]);
    }
}

// A metered processor whose block, or with spinsInInit its entry, runs its
// fuel check until the check throws.
function spinningProcessorModule(spinsInInit) {
    const spin = [0x03, 0x40, 0x10, 0, 0x0c, 0, 0x0b];
    return moduleOf(
        section(1, vector([
            [0x60, 0, 0],
            [0x60, 1, 0x7f, 1, 0x7f],
            [0x60, 1, 0x7f, 0],
            [0x60, 5, 0x7f, 0x7f, 0x7c, 0x7f, 0x7f, 0],
            [0x60, 2, 0x7f, 0x7c, 1, 0x7f],
        ])),
        section(2, vector([[...named('rive_lane_v1'), ...named('fuel_check'), 0, 0]])),
        section(3, vector([[1], [2], [3], [4]])),
        section(5, vector([[0x00, 1]])),
        section(7, vector([
            [...named('memory'), 2, 0],
            [...named('__rive_lane_alloc'), 0, 1],
            [...named('__rive_lane_free'), 0, 2],
            [...named('__rive_processor_0'), 0, 3],
            [...named('__rive_processor_process'), 0, 4],
        ])),
        section(10, vector([
            [4, 0, 0x41, 16, 0x0b],
            [2, 0, 0x0b],
            spinsInInit ? [9, 0, ...spin, 0x0b] : [2, 0, 0x0b],
            spinsInInit ? [5, 0, 0x41, 0xc0, 0x00, 0x0b] : [10, 0, ...spin, 0x00, 0x0b],
        ]))
    );
}

// Its fuel checks end a block at the budget render is given, and an init past
// kProcessorInitMs, as a worklet cannot interrupt either.
{
    const format = { sampleRate: 48000, maxFrames: 128, channels: 2 };
    const start = (spinsInInit) =>
        startProcessorLane(new WebAssembly.Module(spinningProcessorModule(spinsInInit)), 0,
            new Uint8Array(0), format, () => ({}), () => {});
    const began = performance.now();
    assert.equal(start(false).lane.render(128, 0, () => {}, 5), kProcessorOverran);
    assert.ok(performance.now() - began < 500, 'the block ended at its budget');
    assert.deepEqual(start(true), { error: kProcessorInitOverran });
    assert.ok(performance.now() - began >= kProcessorInitMs, 'init ran its whole budget');
}

// The mixer gives each block its budget before it runs, so a metered
// processor stuck in its first block fails at the warm up budget.
{
    const heap = new SharedArrayBuffer(65536);
    const holds = new Int32Array(new SharedArrayBuffer(16));
    const posted = [];
    const mixer = createProcessorMixer(() => heap, 1024, (message) => posted.push(message), holds);
    mixer.receive({
        kind: 'start',
        id: 1,
        moduleId: 10,
        bytes: spinningProcessorModule(false),
        entry: 0,
        input: new Uint8Array(0),
        format: { sampleRate: 48000, maxFrames: 128, channels: 2 },
        hardBlocks: 4,
    });
    for (let turn = 0; turn < 50 && posted.length === 0; turn++) {
        await new Promise((settle) => setTimeout(settle, 5));
    }
    assert.deepEqual(posted.shift(), { id: 1, kind: 'running' });
    mixer.render(128, 0, () => {});
    assert.deepEqual(posted, [{ id: 1, kind: 'failed', text: kProcessorOverran }]);
}

// Starts behind one compile run back to back, so the block counter moves
// between them, giving each the page's whole start limit.
{
    const heap = new SharedArrayBuffer(65536);
    const holds = new Int32Array(new SharedArrayBuffer(16));
    const failed = [];
    const post = (message) => failed.push([message.text, Atomics.load(holds, kHoldBlocks)]);
    const mixer = createProcessorMixer(() => heap, 1024, post, holds);
    const bytes = spinningProcessorModule(true);
    for (const id of [1, 2]) {
        mixer.receive({
            kind: 'start',
            id,
            moduleId: 10,
            bytes: id === 1 ? bytes : null,
            entry: 0,
            input: new Uint8Array(0),
            format: { sampleRate: 48000, maxFrames: 128, channels: 2 },
            hardBlocks: 0,
        });
    }
    for (let turn = 0; turn < 50 && failed.length < 2; turn++) {
        await new Promise((settle) => setTimeout(settle, 5));
    }
    assert.deepEqual(failed.map(([text]) => text), [kProcessorInitOverran, kProcessorInitOverran]);
    assert.ok(failed[1][1] > failed[0][1], 'the block counter moved between the starts');
}

// A host's context that closed would leave every start waiting on it.
{
    const runner = installRiveScripting({});
    runner.attach({ memory: () => ({ buffer: new SharedArrayBuffer(8) }) });
    globalThis.crossOriginIsolated = true;
    globalThis.AudioContext = class {};
    runner['audioWorkletUrl'] = 'rive_audio_worklet.mjs';
    runner['audioContext'] = { state: 'closed' };
    assert.equal(
        runner['audioRefusal'](),
        'real time audio cannot start on a closed AudioContext'
    );
    runner['audioContext'] = { state: 'running' };
    assert.equal(runner['audioRefusal'](), null);
}

const sleep = (ms) => new Promise((settle) => setTimeout(settle, ms));
async function until(done, ms) {
    for (const end = Date.now() + ms; !done() && Date.now() < end; ) {
        await sleep(5);
    }
    assert.ok(done(), 'timed out');
}

// A runner with real time audio on a page with threads, and the processor
// events it reports to librive.
function audioRunner(context, { module = {}, workletUrl = 'rive_audio_worklet.mjs' } = {}) {
    const memory = { buffer: new SharedArrayBuffer(1 << 16) };
    const events = [];
    const runner = installRiveScripting(module);
    runner.attach({
        memory: () => memory,
        exports: { _rive_shared_table: () => 1024 },
        processorEvent: (owner, kind, text) => events.push([owner, kind, text]),
        heapU8: () => new Uint8Array(memory.buffer),
    });
    if (workletUrl !== null) {
        runner['audioWorkletUrl'] = workletUrl;
    }
    runner['audioContext'] = context;
    const format = { sampleRate: 48000, maxFrames: 128, channels: 2 };
    const start = (owner, moduleId) =>
        runner['audioStart'](owner, moduleId, () => processorModule, 0, new Uint8Array(0),
            format, 4, 1).id;
    return { runner, events, start };
}

// A browser context and worklet node that never render a block.
class PageContext {
    constructor() {
        this.state = 'running';
        this.sampleRate = 48000;
        this.destination = {};
        this.listeners = [];
        this.audioWorklet = { addModule: async () => {} };
    }
    addEventListener(name, listener) {
        this.listeners.push(listener);
    }
    close() {
        this.state = 'closed';
        this.listeners.forEach((listener) => listener());
    }
}
const nodes = [];
globalThis.AudioContext = PageContext;
globalThis.AudioWorkletNode = class {
    constructor(context, name, options) {
        this.options = options;
        this.posted = [];
        this.disconnected = false;
        this.port = { onmessage: null, postMessage: (message) => this.posted.push(message) };
        nodes.push(this);
    }
    connect() {}
    disconnect() {
        this.disconnected = true;
    }
};
const kFailed = 3;

// A build that carries the worklet starts it from a blob when the host
// serves none; one that carries none still asks the host for it.
{
    nodes.length = 0;
    const context = new PageContext();
    const added = [];
    context.audioWorklet = { addModule: async (url) => added.push(url) };
    const { runner, start } = audioRunner(context, {
        module: { riveAudioWorkletSource: 'registerProcessor()' },
        workletUrl: null,
    });
    assert.equal(runner['audioRefusal'](), null);
    start(11, 5);
    await until(() => nodes.length === 1, 1000);
    assert.match(added[0], /^blob:/);
    context.close();
    assert.equal(
        audioRunner(new PageContext(), { workletUrl: null }).runner['audioRefusal'](),
        'real time audio needs riveScripting.audioWorkletUrl'
    );
}

// The worklet a threaded build carries runs on its own and adds the node the
// runner makes.
{
    const carried = {};
    new Function(
        'Module',
        readFileSync(new URL('../../src/wasm/web/rive_audio_worklet_pre.js', import.meta.url), 'utf8')
    )(carried);
    const registered = [];
    new Function('AudioWorkletProcessor', 'registerProcessor', carried['riveAudioWorkletSource'])(
        class {},
        (name) => registered.push(name)
    );
    assert.deepEqual(registered, [kProcessorNode]);
}

// A worklet that lost a module's bytes gets them again, unless the processor
// asking already stopped. Closing the host's context fails what it ran.
{
    nodes.length = 0;
    const context = new PageContext();
    const { runner, events, start } = audioRunner(context);
    start(11, 5);
    await until(() => nodes[0]?.posted.length === 1, 1000);
    const node = nodes[0];
    assert.notEqual(node.posted[0].bytes, null);
    const second = start(12, 5);
    assert.equal(node.posted[1].bytes, null);
    node.port.onmessage({ data: { id: second, kind: 'resend' } });
    assert.equal(node.posted[2].id, second);
    assert.notEqual(node.posted[2].bytes, null);
    runner['audioStop'](second);
    node.port.onmessage({ data: { id: second, kind: 'resend' } });
    assert.deepEqual(node.posted.slice(3).map((message) => message.kind), ['stop']);

    context.close();
    assert.deepEqual(events, [[11, kFailed, 'script audio context closed']]);
    assert.equal(runner['audioState']()['context'], 'none');
    assert.equal(
        runner['audioRefusal'](),
        'real time audio cannot start on a closed AudioContext'
    );
}

// A worklet that stops counting blocks fails its processors and leaves the
// mix, and its context is refused from then on.
{
    nodes.length = 0;
    const context = new PageContext();
    const { runner, events, start } = audioRunner(context);
    start(11, 5);
    await until(() => events.length > 0, 3000);
    assert.deepEqual(events, [[11, kFailed, 'processor stalled the audio thread']]);
    assert.equal(nodes[0].disconnected, true);
    assert.equal(nodes[0].posted.at(-1).kind, 'end');
    assert.equal(
        runner['audioRefusal'](),
        "real time audio stalled the host's AudioContext; hand riveScripting a new one"
    );
}

// A start the worklet runs outlasts init's own budget before the page calls
// it a stall, so a metered init that never returns fails on that budget.
{
    nodes.length = 0;
    const context = new PageContext();
    const { events, start } = audioRunner(context);
    start(11, 5);
    await until(() => nodes[0]?.posted.length === 1, 1000);
    Atomics.store(new Int32Array(nodes[0].options.processorOptions.counter), kHoldStarting, 1);
    await sleep(kProcessorInitMs + 100);
    assert.deepEqual(events, []);
    await until(() => events.length > 0, 2000);
    assert.deepEqual(events, [[11, kFailed, 'processor stalled the audio thread']]);
}

// A context only the runner holds resumes on the page's first gesture, and
// warns once if the browser has not started it by then. A host's own context
// is the host's to resume. Only the runner's own plays as playback, through
// iOS's silent switch.
{
    const session = { type: 'auto' };
    const navigator = globalThis.navigator;
    Object.defineProperty(globalThis, 'navigator', {
        value: { audioSession: session },
        configurable: true,
    });
    const page = new EventTarget();
    globalThis.addEventListener = page.addEventListener.bind(page);
    globalThis.removeEventListener = page.removeEventListener.bind(page);
    const warnings = [];
    const warn = console.warn;
    console.warn = (text) => warnings.push(text);
    const made = [];
    class WaitingContext extends PageContext {
        constructor() {
            super();
            this.state = 'suspended';
            this.resumes = 0;
            made.push(this);
        }
        removeEventListener(name, listener) {
            this.listeners = this.listeners.filter((other) => other !== listener);
        }
        // As the browser starts one it lets start.
        start() {
            this.state = 'running';
            this.listeners.forEach((listener) => listener());
        }
        resume() {
            this.resumes++;
            this.start();
        }
    }
    globalThis.AudioContext = WaitingContext;
    const hosts = new WaitingContext();
    audioRunner(hosts).start(11, 5);
    assert.equal(session.type, 'auto');
    audioRunner(undefined).start(12, 5);
    assert.equal(session.type, 'playback');
    audioRunner(undefined).start(13, 5);
    const [, waiting, started] = made;
    started.start();
    await sleep(1200);
    assert.deepEqual(warnings, ['script audio waits for a click, tap or key press on the page']);
    page.dispatchEvent(new Event('pointerdown'));
    page.dispatchEvent(new Event('keydown'));
    assert.deepEqual(made.map((context) => context.resumes), [0, 1, 0]);
    assert.equal(waiting.state, 'running');
    made.forEach((context) => context.close());
    console.warn = warn;
    globalThis.AudioContext = PageContext;
    Object.defineProperty(globalThis, 'navigator', { value: navigator, configurable: true });
}

// NRDP has no statechange event, so its output notices a closed context by
// looking, and refuses one closed from the start.
{
    const worklet = new URL('../../src/wasm/web/rive_audio_worklet_nrdp.js', import.meta.url);
    installNrdpMock(() => readFileSync(worklet, 'utf8'));
    const closed = new AudioContext();
    const { runner } = audioRunner(closed);
    runner['audioWorkletUrl'] = 'rive_audio_worklet_nrdp.js';
    assert.equal(closed.state, 'closed');
    assert.equal(
        runner['audioRefusal'](),
        'real time audio cannot start on a closed AudioContext'
    );

    nrdp.audio.output.open();
    const context = new AudioContext();
    const page = audioRunner(context);
    page.runner['audioWorkletUrl'] = 'rive_audio_worklet_nrdp.js';
    assert.equal(page.runner['audioRefusal'](), null);
    page.start(11, 5);
    await until(() => page.events.length > 0, 1000);
    assert.deepEqual(page.events, [[11, 1, '']]);
    context.state = 'closed';
    await until(() => page.events.length > 1, 1000);
    assert.deepEqual(page.events[1], [11, kFailed, 'script audio context closed']);
    assert.equal(page.runner['audioState']()['context'], 'none');
    delete globalThis.nrdp;
}

console.log('web host ok');
