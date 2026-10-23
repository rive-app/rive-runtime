// The JS half of the browser scripting lane. Script modules run on the
// browser's own wasm engine; librive reaches them through the EM_JS calls in
// browser_scripting_vm.cpp, which forward to the runner installed here.
//
// Loading a file is two steps. File::import makes each script VM and
// registers its module bytes here, without instantiating. The host then
// awaits prepare(), which compiles and instantiates every registered module
// asynchronously, and only then asks librive to start the file's scripts.
// A host that skips prepare() still works where the browser allows
// synchronous compilation on the calling thread: start() instantiates inline.
//
// Hosts link rive_scripting_pre.js, generated from this file, into their
// emscripten build with --pre-js. It installs the runner by itself, so a
// host only does:
//   ... import a file ...
//   await Module.riveScripting.prepare();
//   ... start the file's scripts ...
import {
    createRiveModuleImports,
    bindRiveHostCalls,
    unlinkedImport,
} from './rive_module_imports.mjs';
import {
    kFrameMinorExport,
    kLaneEntryPrefixes,
    kLaneResetExport,
    kLaneSections,
    kLaneStates,
    kLiftedStartExport,
    kProcessorEventLog,
    kProcessorInitMs,
    kProcessorNode,
    kReuseMaxPages,
    kWorkerWakeExport,
    laneImportError,
    laneImportRefusal,
} from './rive_lane_imports.mjs';
import { kHoldBlocks, kHoldStarting, kHoldWords } from './rive_shared_layout.mjs';
import { createSharedLaneImports, releaseSharedLane } from './rive_shared_lane.mjs';
import {
    enterLane,
    errorText,
    instantiateLane,
    monotonicNow,
    startLane,
    startProcessorLane,
} from './rive_lane.mjs';

// WasmScriptingVM::CallOutcome, then the browser's own host fault.
const kOk = 0;
const kMissing = 1;
const kTrapped = 2;
const kHostFault = 3;

// One function returning v128.const zero, then one returning the relaxed
// swizzle of two: the smallest modules each SIMD flavor needs.
const kV128Zero = [0xfd, 0x0c, ...new Array(16).fill(0)];
const kSimdProbe = probeModule([...kV128Zero]);
const kRelaxedSimdProbe = probeModule([
    ...kV128Zero,
    ...kV128Zero,
    0xfd,
    0x80,
    0x02,
]);

function probeModule(body) {
    const code = [0, ...body, 0x0b];
    return new Uint8Array([
        0x00, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        // One type, () -> v128, and one function of it.
        1, 5, 1, 0x60, 0, 1, 0x7b,
        3, 2, 1, 0,
        10, code.length + 2, 1, code.length, ...code,
    ]);
}

// Index immediates of the 0xfc ops by sub-opcode: none for the saturating
// truncations, then memory.init through table.fill.
const kBulkIndices = [0, 0, 0, 0, 0, 0, 0, 0, 2, 1, 2, 1, 2, 1, 2, 1, 1, 1];
// The ops past the variable accesses that take one LEB.
const kOneLebOps = [0x0c, 0x0d, 0x10, 0x12, 0x3f, 0x40, 0x41, 0x42, 0xd0, 0xd2];

// The SIMD a module's types and code need: 2 for relaxed, 1 for plain, 0 for
// none, or -1 when they hold what this walk does not read.
function simdNeeded(bytes) {
    let needed = 0;
    let at = 8;
    const need = (flavor) => {
        needed = Math.max(needed, flavor);
    };
    const next = () => {
        const [value, end] = readLeb(bytes, at);
        at = end;
        return value;
    };
    const memarg = () => {
        if (next() & 0x40) {
            next();
        }
        next();
    };
    // False for a reference type with a heap type, which takes more bytes.
    const valueType = () => {
        const type = bytes[at++];
        if (type === 0x7b) {
            need(1);
        }
        return type !== 0x63 && type !== 0x64;
    };
    const instruction = () => {
        const op = bytes[at++];
        if (op === 0xfd) {
            const sub = next();
            need(sub >= 0x100 && sub <= 0x113 ? 2 : 1);
            if (sub <= 0x0b || sub === 0x5c || sub === 0x5d) {
                memarg();
            } else if (sub === 0x0c || sub === 0x0d) {
                at += 16;
            } else if (sub >= 0x15 && sub <= 0x22) {
                at += 1;
            } else if (sub >= 0x54 && sub <= 0x5b) {
                memarg();
                at += 1;
            }
        } else if (op === 0xfc) {
            const sub = next();
            if (sub >= kBulkIndices.length) {
                return false;
            }
            for (let i = 0; i < kBulkIndices[sub]; i++) {
                next();
            }
        } else if (op === 0xfe) {
            // atomic.fence takes a byte, the other atomics a memarg.
            if (next() === 3) {
                at += 1;
            } else {
                memarg();
            }
        } else if (op >= 0x28 && op <= 0x3e) {
            memarg();
        } else if (op >= 0x02 && op <= 0x04) {
            // A block type: one byte, or a type index.
            if (bytes[at] === 0x7b) {
                need(1);
            }
            next();
        } else if (op === 0x0e) {
            for (let labels = next(); labels >= 0 && at < bytes.length; labels--) {
                next();
            }
        } else if (op === 0x1c) {
            for (let types = next(); types > 0 && at < bytes.length; types--) {
                if (!valueType()) {
                    return false;
                }
            }
        } else if (op === 0x11 || op === 0x13) {
            next();
            next();
        } else if (op === 0x43 || op === 0x44) {
            at += op === 0x43 ? 4 : 8;
        } else if ((op >= 0x20 && op <= 0x26) || kOneLebOps.includes(op)) {
            next();
        } else if (!(op <= 0x01 || op === 0x05 || op === 0x0b || op === 0x0f ||
                op === 0x1a || op === 0x1b || op === 0xd1 ||
                (op >= 0x45 && op <= 0xc4))) {
            return false;
        }
        return true;
    };
    while (at < bytes.length) {
        const id = bytes[at];
        const [size, body] = readLeb(bytes, at + 1);
        at = body;
        if (id === 1) {
            for (let types = next(); types > 0 && at < bytes.length; types--) {
                // Function types only: params, then results.
                if (bytes[at++] !== 0x60) {
                    return -1;
                }
                for (let list = 0; list < 2; list++) {
                    for (let n = next(); n > 0 && at < bytes.length; n--) {
                        if (!valueType()) {
                            return -1;
                        }
                    }
                }
            }
        } else if (id === 10) {
            for (let bodies = next(); bodies > 0 && at < bytes.length; bodies--) {
                const end = Math.min(next() + at, bytes.length);
                for (let locals = next(); locals > 0 && at < end; locals--) {
                    next();
                    if (!valueType()) {
                        return -1;
                    }
                }
                while (at < end) {
                    if (!instruction()) {
                        return -1;
                    }
                }
            }
        }
        at = body + size;
    }
    return needed;
}

function readLeb(bytes, at) {
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

function leb(value) {
    const out = [];
    do {
        const byte = value & 0x7f;
        value >>>= 7;
        out.push(value ? byte | 0x80 : byte);
    } while (value);
    return out;
}

// Each section of a module: its id, where it starts, its body and its end.
function sectionsOf(bytes) {
    const sections = [];
    for (let at = 8; at < bytes.length; ) {
        const [size, body] = readLeb(bytes, at + 1);
        sections.push({ id: bytes[at], at, body, end: body + size });
        at = body + size;
    }
    return sections;
}

const kLaneSectionNames = kLaneSections.map(([name]) =>
    Array.from(name, (c) => c.charCodeAt(0))
);

// The lane modules a script module carries in its custom sections.
function laneModulesOf(bytes) {
    const lanes = [];
    for (const section of sectionsOf(bytes)) {
        if (section.id !== 0) {
            continue;
        }
        const [length, name] = readLeb(bytes, section.body);
        const named = (wanted) =>
            wanted.length === length && wanted.every((c, i) => bytes[name + i] === c);
        if (kLaneSectionNames.some(named)) {
            lanes.push(bytes.subarray(name + length, section.end));
        }
    }
    return lanes;
}

// rasc runs every script's top level in the module's start function, and a
// top level can pass memory to the host. The module's memory is an export,
// which the page cannot reach until start has returned. So the start section
// is dropped and its function exported, for the runner to call once the
// instance and its memory exist. Modules without one come back untouched.
// Lane modules are cached by these bytes, so they must match wasm_lane.cpp's.
function liftStart(bytes) {
    const all = sectionsOf(bytes);
    const start = all.find((section) => section.id === 8);
    const sections = all.filter((section) => section !== start);
    if (start === undefined || !sections.some((section) => section.id === 7)) {
        return bytes;
    }
    const startFunction = readLeb(bytes, start.body)[0];
    const parts = [bytes.subarray(0, 8)];
    for (const section of sections) {
        let body = bytes.subarray(section.body, section.end);
        if (section.id === 7) {
            const [count, first] = readLeb(bytes, section.body);
            const name = Array.from(kLiftedStartExport, (c) => c.charCodeAt(0));
            body = Uint8Array.from([
                ...leb(count + 1),
                ...bytes.subarray(first, section.end),
                ...leb(name.length),
                ...name,
                0,
                ...leb(startFunction),
            ]);
        }
        parts.push(Uint8Array.from([section.id, ...leb(body.length)]), body);
    }
    const out = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
    let at = 0;
    for (const part of parts) {
        out.set(part, at);
        at += part.length;
    }
    return out;
}

// Args by index: spreading a subarray of the heap cost more than the call.
function callExport(fn, args, at, argc) {
    switch (argc) {
        case 0:
            return fn();
        case 1:
            return fn(args[at]);
        case 2:
            return fn(args[at], args[at + 1]);
        case 3:
            return fn(args[at], args[at + 1], args[at + 2]);
        case 4:
            return fn(args[at], args[at + 1], args[at + 2], args[at + 3]);
        case 5:
            return fn(args[at], args[at + 1], args[at + 2], args[at + 3],
                args[at + 4]);
        case 6:
            return fn(args[at], args[at + 1], args[at + 2], args[at + 3],
                args[at + 4], args[at + 5]);
        case 7:
            return fn(args[at], args[at + 1], args[at + 2], args[at + 3],
                args[at + 4], args[at + 5], args[at + 6]);
        case 8:
            return fn(args[at], args[at + 1], args[at + 2], args[at + 3],
                args[at + 4], args[at + 5], args[at + 6], args[at + 7]);
        default:
            return fn(...args.subarray(at, at + argc));
    }
}

export function installRiveScripting(Module) {
    // Primitives over the librive heap and exports, handed over by librive
    // itself on first use because only code inside it can see them.
    let host = null;
    const vms = new Map();
    // Bridge traffic, tallied only while a page measures it.
    const counters = {
        on: false,
        hostCalls: 0,
        bytesStaged: 0,
        bytesCopiedOut: 0,
        bytesRead: 0,
        bytesWritten: 0,
    };

    // What the generated imports need from librive, for calls under ctx,
    // binding only that namespace when only names one.
    function librive(ctx, only = null) {
        return {
            calls: bindRiveHostCalls(host.hostCall, ctx, only),
            heapU8: host.heapU8,
            stackSave: host.stackSave,
            stackAlloc: host.stackAlloc,
            stackRestore: host.stackRestore,
            malloc: host.malloc,
            free: host.free,
            counters,
        };
    }

    // Tests stand in for an engine without SIMD with their own validator,
    // and run offline processors on the natives a worklet uses.
    const validate =
        Module['riveScriptingValidate'] ??
        ((bytes) => WebAssembly.validate(bytes));
    const laneTwin = Module['riveScriptingLaneTwin'] === true;
    let features = null;
    // Why this engine refuses a module or a lane module it carries, when a
    // SIMD flavor it lacks is the reason; null otherwise.
    function missingSimd(bytes) {
        features ??= {
            simd: validate(kSimdProbe),
            relaxed: validate(kRelaxedSimdProbe),
        };
        const modules = [bytes, ...laneModulesOf(bytes)];
        if (
            (features.simd && features.relaxed) ||
            modules.every((module) => validate(module))
        ) {
            return null;
        }
        const needed = Math.max(...modules.map(simdNeeded));
        if (!features.simd && needed > 0) {
            return "this browser lacks WebAssembly SIMD, so this file's scripts " +
                'are off; bake with wasmSimd: off to support it';
        }
        if (!features.relaxed && needed === 2) {
            return "this browser lacks WebAssembly relaxed SIMD, so this file's " +
                'scripts are off; bake with wasmSimd: on to support it';
        }
        return null;
    }

    function importsFor(entry, module) {
        const imports = createRiveModuleImports(
            librive(entry.ctx),
            () => {
                if (entry.memory === null) {
                    // Only a start function liftStart could not move.
                    throw new Error(
                        'script passed memory to the host while its module ' +
                            'was still starting'
                    );
                }
                return entry.memory;
            }
        );
        // The browser refuses a module with an import nothing provides.
        for (const wanted of WebAssembly.Module.imports(module)) {
            if (wanted['kind'] === 'function') {
                const space = (imports[wanted['module']] ??= {});
                space[wanted['name']] ??= unlinkedImport(
                    wanted['module'],
                    wanted['name']
                );
            }
        }
        return imports;
    }

    // Before its lanes compile and a start liftStart left in place runs, as
    // top levels already pass strings and a refused module must do neither.
    function announceExports(entry, module) {
        const mark = host.stackSave();
        for (const { name } of WebAssembly.Module.exports(module)) {
            if (!name.startsWith('__rive')) {
                continue;
            }
            const capacity = name.length * 3 + 1;
            const ptr = host.stackAlloc(capacity);
            host.writeUtf8(name, ptr, capacity);
            host.exports['_rive_web_vm_module_export'](entry.ctx.vm, ptr);
        }
        host.stackRestore(mark);
        const refusal =
            host.exports['_rive_web_vm_start_refusal'](entry.ctx.vm);
        if (refusal !== 0) {
            throw new Error(host.utf8(refusal));
        }
    }

    function adopt(entry, instance) {
        entry.memory = instance.exports['memory'];
        entry.bytes = null;
        // State of the call in progress, which a trap unwinds without
        // restoring; a global the module cannot write is not state.
        entry.callState = Object.keys(instance.exports)
            .filter((name) => name.startsWith('__riveCall'))
            .map((name) => instance.exports[name])
            .filter((global) => {
                if (!(global instanceof WebAssembly.Global)) {
                    return false;
                }
                try {
                    global.value = global.value;
                    return true;
                } catch {
                    return false;
                }
            });
        // One stack for nested calls, so hot paths allocate nothing.
        entry.callSaved = [];
        // A metered module's own fuel bounds its top level, as it does its
        // calls.
        const fuelMs = host.exports['_rive_web_vm_start_fuel'](entry.ctx.vm);
        const arm = instance.exports['__riveFuelArm'];
        const end = instance.exports['__riveFuelEnd'];
        const fueled = fuelMs > 0 && arm !== undefined && end !== undefined;
        const booting = host.exports['_rive_web_vm_booting'];
        booting(entry.ctx.vm);
        const mark = host.stackSave();
        // A wait in the top level moves a metered module's fuel on with a call.
        entry.instance = instance;
        try {
            if (fueled) arm(fuelMs);
            instance.exports[kLiftedStartExport]?.();
            if (fueled) end();
        } catch (error) {
            entry.instance = null;
            // A host fault skips the epilogues of the librive frames it unwinds.
            host.stackRestore(mark);
            entry.ctx.raised = null;
            throw error;
        } finally {
            booting(0);
        }
    }

    async function prepareEntry(entry) {
        try {
            const module = await WebAssembly.compile(liftStart(entry.bytes));
            if (entry.ctx.vm === 0) {
                return;
            }
            announceExports(entry, module);
            const lanes = prepareLanes(module);
            const instance = await WebAssembly.instantiate(
                module,
                importsFor(entry, module)
            );
            await lanes;
            // Released while instantiating.
            if (entry.ctx.vm === 0) {
                return;
            }
            adopt(entry, instance);
        } catch (error) {
            entry.error = errorText(error);
        }
    }

    // Lanes run the lane modules a script module carries, on whichever thread
    // runs them, reaching only their own natives, shared memory and the console.

    // Compiled lane modules, oldest first, matched by id or by their lifted
    // bytes, which librive hands over; a dropped one only costs a recompile.
    const laneModules = [];
    const kLaneModuleCache = 8;
    // The page's idle pthread workers, on a pthread build.
    let threads = () => [];

    function sameBytes(a, b) {
        if (a.length !== b.length) {
            return false;
        }
        for (let i = 0; i < a.length; i++) {
            if (a[i] !== b[i]) {
                return false;
            }
        }
        return true;
    }

    function cacheLane(bytes, module, error) {
        // job is the instance a finished job left on this thread for the next.
        const compiled = { bytes, module, error, ids: new Set(), job: null };
        laneModules.push(compiled);
        if (laneModules.length > kLaneModuleCache) {
            laneModules.shift();
        }
        return compiled;
    }

    const laneCached = (bytes) => laneModules.some((lane) => sameBytes(lane.bytes, bytes));

    // A thread that missed the page's compile compiles its own.
    function shareLane(bytes, module) {
        if (laneCached(bytes)) {
            return;
        }
        cacheLane(bytes, module, null);
        for (const worker of threads()) {
            worker.postMessage({ 'riveLane': { 'bytes': bytes, 'module': module } });
        }
    }

    const takeLane = (data) => {
        const lane = data?.['riveLane'];
        if (lane !== undefined) {
            shareLane(lane['bytes'], lane['module']);
        }
    };
    const nodeThreads = globalThis.process?.getBuiltinModule?.('worker_threads');
    if (nodeThreads?.isMainThread === false) {
        nodeThreads.parentPort.on('message', takeLane);
    } else if (globalThis.WorkerGlobalScope !== undefined) {
        globalThis.addEventListener('message', (event) => takeLane(event.data));
    }

    function checkLaneImports(module, kind) {
        for (const wanted of WebAssembly.Module.imports(module)) {
            if (laneImportRefusal(kind, wanted['module'], wanted['name']) !== null) {
                throw new Error(laneImportError(wanted['module'], wanted['name']));
            }
        }
    }

    // Compiles a main module's lane sections for every thread, off the page's
    // thread unless now. One that fails is left to fail where it first runs.
    function prepareLanes(main, now = false) {
        const compiles = [];
        for (const [name, kind] of kLaneSections) {
            for (const section of WebAssembly.Module.customSections(main, name)) {
                const bytes = liftStart(new Uint8Array(section));
                if (laneCached(bytes)) {
                    continue;
                }
                const share = (module) => {
                    checkLaneImports(module, kind);
                    shareLane(bytes, module);
                };
                if (now) {
                    try {
                        share(new WebAssembly.Module(bytes));
                    } catch (error) {}
                } else {
                    compiles.push(WebAssembly.compile(bytes).then(share).catch(() => {}));
                }
            }
        }
        return Promise.all(compiles);
    }

    function laneModule(id, bytes, kind) {
        let compiled = laneModules.find((lane) => lane.ids.has(id));
        if (compiled === undefined) {
            const lifted = bytes();
            compiled = laneModules.find((lane) => sameBytes(lane.bytes, lifted));
            if (compiled === undefined) {
                compiled = cacheLane(lifted, null, null);
                try {
                    const module = new WebAssembly.Module(lifted);
                    checkLaneImports(module, kind);
                    compiled.module = module;
                } catch (error) {
                    compiled.error = errorText(error);
                }
            }
            compiled.ids.add(id);
            if (compiled.ids.size > kLaneModuleCache) {
                compiled.ids.delete(compiled.ids.values().next().value);
            }
        }
        return compiled;
    }

    // A lane's shared imports: librive's natives, or with twin Atomics on
    // librive's heap reaching owner's objects, as an audio worklet does.
    function sharedImports(twin = false, owner = 0) {
        return (memory, fail) =>
            twin
                ? createSharedLaneImports(
                      () => host.heapU8().buffer,
                      host.exports['_rive_shared_table'](),
                      owner,
                      memory,
                      null,
                      fail
                  )
                : createRiveModuleImports(
                      librive(
                          { vm: 1, raised: null, hostFault: false, counters },
                          'rive_shared_v1'
                      ),
                      memory,
                      'rive_shared_v1'
                  )['rive_shared_v1'];
    }

    // A browser cannot interrupt a lane, so a metered one's fuel checks throw
    // why librive would.
    function laneFuel(lane) {
        const stop = host.laneStop(lane);
        if (stop !== null) {
            throw stop;
        }
    }

    // The instance a job left idle on this thread, or a new one, whose imports
    // reach whichever job it runs.
    function jobInstance(compiled) {
        const idle = compiled.job;
        compiled.job = null;
        if (idle !== null) {
            idle.timers.clear();
            return idle;
        }
        const job = { lane: 0, cancelled: null, timers: new Map(), reset: false };
        return Object.assign(
            job,
            instantiateLane({
                module: compiled.module,
                natives: 'rive_job_v1',
                own: () => ({ 'cancelled': () => job.cancelled() }),
                shared: sharedImports(),
                log: (line) => host.laneLog(job.lane, line),
                timers: job.timers,
                fuel: () => laneFuel(job.lane),
            })
        );
    }

    // A stop reaches a running job through its cancelled() and its fuel
    // checks. The output views the job's memory.
    function runJob(lane, id, bytes, entry, input, cancelled) {
        const compiled = laneModule(id, bytes, 'job');
        if (compiled.error !== null) {
            return { ok: false, error: compiled.error };
        }
        const job = jobInstance(compiled);
        if (job.error !== undefined) {
            return { ok: false, error: job.error };
        }
        job.lane = lane;
        job.cancelled = cancelled;
        const entered = enterLane(
            job,
            { prefix: kLaneEntryPrefixes['job'], entry, input },
            job.reset
        );
        if (entered.error !== undefined) {
            return { ok: false, error: entered.error };
        }
        try {
            const buffer = job.memory.buffer;
            const words = new Uint32Array(buffer, entered.result >>> 0, 2);
            const output =
                words[1] === 0 ? new Uint8Array(0) : new Uint8Array(buffer, words[0], words[1]);
            // Only a job that returned, unstopped, with small memory leaves its
            // instance; the next resets it once this output is copied out.
            if (
                job.exports[kLaneResetExport] !== undefined &&
                buffer.byteLength <= kReuseMaxPages * 65536 &&
                !cancelled()
            ) {
                job.reset = true;
                compiled.job = job;
            }
            return { ok: true, output };
        } catch (error) {
            return { ok: false, error: job.failure(error) };
        }
    }

    // Workers keep their instance between wakes, on the thread that started
    // them; librive steps each one and owns its waits.
    const workers = new Map();
    let nextWorker = 1;

    // A worker's natives call back into librive through natives.
    function workerStart(lane, id, bytes, entry, input, natives) {
        const compiled = laneModule(id, bytes, 'worker');
        if (compiled.error !== null) {
            return { ok: false, error: compiled.error };
        }
        const started = startLane({
            module: compiled.module,
            natives: 'rive_worker_v1',
            // A view of the worker's memory, which librive copies once.
            own: (memory) => ({
                'finished': (ok, at, length) =>
                    natives.finished(
                        ok,
                        new Uint8Array(memory().buffer, at >>> 0, length >>> 0)
                    ),
                'wait': natives.wait,
                'stopping': natives.stopping,
            }),
            onThrow: (text) => host.laneFail(lane, text),
            shared: sharedImports(),
            log: (line) => host.laneLog(lane, line),
            fuel: () => laneFuel(lane),
            prefix: kLaneEntryPrefixes['worker'],
            entry,
            input,
            free: true,
        });
        if (started.error !== undefined) {
            return { ok: false, error: started.error };
        }
        const worker = nextWorker++;
        workers.set(worker, {
            wake: started.exports[kWorkerWakeExport],
            collect: started.exports[kFrameMinorExport],
        });
        return { ok: true, id: worker };
    }

    // Returns null once the worker took the wake, else the error.
    function workerWake(worker, wait, value) {
        try {
            workers.get(worker).wake(wait, value);
            return null;
        } catch (error) {
            return errorText(error);
        }
    }

    // Back at the host no frame is live, so the frame collector may run.
    // Returns null, or the error that ended the worker.
    function workerCollect(worker) {
        try {
            workers.get(worker)?.collect?.();
            return null;
        } catch (error) {
            return errorText(error);
        }
    }

    function workerEnd(worker) {
        workers.delete(worker);
    }

    // A processor rendered offline: librive pulls each block.
    const processors = new Map();
    let nextProcessor = 1;

    function processorStart(lane, id, bytes, entry, input, format, sharedOwner) {
        const compiled = laneModule(id, bytes, 'processor');
        if (compiled.error !== null) {
            return { ok: false, error: compiled.error };
        }
        const started = startProcessorLane(
            compiled.module,
            entry,
            input,
            format,
            sharedImports(laneTwin, sharedOwner),
            (line) => host.laneLog(lane, line)
        );
        if (started.error !== undefined) {
            return { ok: false, error: started.error };
        }
        const processor = nextProcessor++;
        processors.set(processor, started.lane);
        return { ok: true, id: processor };
    }

    function processorRender(processor, frames, startFrame, sink) {
        return processors.get(processor).render(frames, startFrame, sink);
    }

    // Processors in real time run on an audio worklet that reaches librive's
    // heap through Atomics; README.md says what the host provides for it.

    // owners: the librive processor and module behind each worklet id.
    const audio = { output: null, owners: new Map(), next: 1 };
    // Checks of the worklet's block counter that may see no block before the
    // output ends. A start outlasts init's budget, which ends a metered one.
    const kStallCheckMs = 50;
    const kStallChecks = 2;
    const kStartStallChecks = kProcessorInitMs / kStallCheckMs + 10;
    const kReadyChecks = 40;
    // A stuck browser worklet holds up only its own context, and a slow block
    // that returns fails on its budget, so a stall there must last longer.
    const kBrowserStallChecks = 20;
    // An ended NRDP thread ran on for up to 39 ms on the devices we measured.
    const kEndedThreadMs = 200;
    const kContextClosed = 'script audio context closed';
    // A context the browser lets start does so well within this.
    const kGestureWaitMs = 1000;

    // NRDP's names are quoted so a closure compiled host keeps them.
    const nrdp = () => globalThis['nrdp'];
    const onNrdp = () => nrdp()?.['audio']?.['output'] !== undefined;

    // Null when processors can run in real time, else why not.
    function audioRefusal() {
        if (!onNrdp() && globalThis.crossOriginIsolated !== true) {
            return 'real time audio needs cross origin isolation';
        }
        // The worklet reaches librive's heap through Atomics.
        if (
            typeof SharedArrayBuffer === 'undefined' ||
            !(host.memory().buffer instanceof SharedArrayBuffer)
        ) {
            return 'real time audio needs librive built with threads';
        }
        if (typeof runner['audioWorkletUrl'] !== 'string') {
            return 'real time audio needs riveScripting.audioWorkletUrl';
        }
        if (typeof AudioContext === 'undefined') {
            return 'real time audio needs Web Audio';
        }
        const given = runner['audioContext'];
        if (given?.state === 'closed') {
            return 'real time audio cannot start on a closed AudioContext';
        }
        // A stuck worklet holds every later addModule on its context.
        if (contextHolds.get(given)?.stalled) {
            return "real time audio stalled the host's AudioContext; hand riveScripting a new one";
        }
        return null;
    }

    // What both outputs share: messages queue until the worklet can take
    // them, and those that never reach it fail their processors.
    function audioQueue() {
        let queue = [];
        let send = null;
        return {
            post(message) {
                if (send !== null) {
                    send(message);
                } else {
                    queue.push(message);
                }
            },
            open(sender) {
                send = sender;
                for (const message of queue) {
                    send(message);
                }
                queue = [];
            },
            fail(text) {
                for (const message of queue) {
                    audioEvent({ 'id': message['id'], 'kind': 'failed', 'text': text });
                }
                queue = [];
            },
        };
    }

    // The worklets run uncompiled, so keys that cross to them are quoted.
    function workletOptions(holds) {
        return {
            'numberOfInputs': 0,
            'numberOfOutputs': 1,
            'outputChannelCount': [2],
            'processorOptions': {
                'memory': host.memory(),
                'table': host.exports['_rive_shared_table'](),
                'counter': holds.buffer,
            },
        };
    }

    const newHolds = () => new Int32Array(new SharedArrayBuffer(kHoldWords * 4));

    // Frees the lock and lane word a worklet that is surely gone left held.
    const releaseLane = (holds) =>
        releaseSharedLane(
            () => host.memory().buffer,
            host.exports['_rive_shared_table'](),
            holds
        );

    function failOwners(text) {
        for (const id of [...audio.owners.keys()]) {
            audioEvent({ 'id': id, 'kind': 'failed', 'text': text });
        }
    }

    // Calls end with why once the worklet stops counting blocks for
    // stallChecks while processors wait on it, or once context closes.
    function watchBlocks(context, holds, readyChecks, stallChecks, end, closed = end) {
        let ready = false;
        let seen = -1;
        let stalled = 0;
        const timer = setInterval(() => {
            if (context.state === 'closed') {
                clearInterval(timer);
                closed(kContextClosed);
                return;
            }
            const now = Atomics.load(holds, kHoldBlocks);
            // A suspended context renders nothing without being stuck. A
            // stall outlives the stop of the processor that caused it.
            const waiting =
                (audio.owners.size > 0 || stalled > 0) && context.state === 'running';
            stalled = now === seen && waiting ? stalled + 1 : 0;
            seen = now;
            const limit = !ready
                ? readyChecks
                : Atomics.load(holds, kHoldStarting) !== 0
                  ? kStartStallChecks
                  : stallChecks;
            if (stalled >= limit) {
                clearInterval(timer);
                end(ready ? 'processor stalled the audio thread' : 'script audio worklet never answered');
            }
        }, kStallCheckMs);
        return {
            ready() {
                ready = true;
                stalled = 0;
            },
            stop: () => clearInterval(timer),
        };
    }

    // An output the host's worklet runs. sent: the modules it has the bytes
    // of, so each crosses once; a worklet that lost them asks again.
    function audioOutputOf(context, queue) {
        return { context, post: queue.post, sent: new Set(), queue };
    }

    // An output that failed or was ended: what it still queued fails, and the
    // next start makes a fresh one. Ends the context when it was ours.
    function dropOutput(output, text, close = true) {
        output.queue.fail(text);
        if (audio.output === output) {
            audio.output = null;
        }
        if (close && runner['audioContext'] === undefined) {
            output.context.close?.();
        }
    }

    const noWorklet = (error) =>
        'script audio has no worklet: ' + errorText(error);

    // A browser context's worklet keeps one lane word until the context
    // closes, unless it stalled, as a stalled worklet may yet run.
    const contextHolds = new WeakMap();

    function browserHolds(context) {
        let holds = contextHolds.get(context);
        if (holds === undefined) {
            holds = { words: newHolds(), stalled: false };
            contextHolds.set(context, holds);
            context.addEventListener('statechange', () => {
                if (context.state === 'closed' && !holds.stalled) {
                    releaseLane(holds.words);
                }
            });
        }
        return holds;
    }

    // Browsers start audio only after a gesture, so a context only the runner
    // holds resumes on the page's first one, and warns once while it waits.
    function ownContext() {
        const context = new AudioContext();
        const gestures = ['pointerdown', 'pointerup', 'keydown', 'touchend'];
        const resume = () => context.resume();
        const waiting = setTimeout(
            () => console.warn('script audio waits for a click, tap or key press on the page'),
            kGestureWaitMs
        );
        const started = () => {
            if (context.state === 'running' || context.state === 'closed') {
                clearTimeout(waiting);
                context.removeEventListener('statechange', started);
                for (const gesture of gestures) {
                    globalThis.removeEventListener(gesture, resume, { capture: true });
                }
            }
        };
        for (const gesture of gestures) {
            globalThis.addEventListener(gesture, resume, { capture: true });
        }
        context.addEventListener('statechange', started);
        started();
        return context;
    }

    function browserOutput() {
        const context = runner['audioContext'] ?? ownContext();
        const queue = audioQueue();
        const output = audioOutputOf(context, queue);
        const holds = browserHolds(context);
        let node = null;
        const closed = (text) => {
            if (audio.output === output) {
                watch.stop();
                failOwners(text);
                dropOutput(output, text, false);
            }
        };
        // Nothing ends a worklet stuck in a block, so its processors fail and the node
        // leaves the mix in case it returns. Chrome's close would wait on that thread.
        const stalled = (text) => {
            holds.stalled = true;
            node.port.onmessage = null;
            node.port.postMessage({ 'kind': 'end' });
            node.disconnect();
            failOwners(text);
            dropOutput(output, text, false);
        };
        const watch = watchBlocks(
            context,
            holds.words,
            Infinity,
            kBrowserStallChecks,
            stalled,
            closed
        );
        context.addEventListener('statechange', () => {
            if (context.state === 'closed') {
                closed(kContextClosed);
            }
        });
        context.audioWorklet
            .addModule(runner['audioWorkletUrl'])
            .then(() => {
                node = new AudioWorkletNode(
                    context,
                    kProcessorNode,
                    workletOptions(holds.words)
                );
                node.port.onmessage = (event) => audioEvent(event.data);
                node.connect(context.destination);
                watch.ready();
                queue.open((message) => node.port.postMessage(message));
            })
            .catch((error) => {
                watch.stop();
                dropOutput(output, noWorklet(error));
            });
        return output;
    }

    // NRDP's worklet has no addModule: init binds a thread of ours to the
    // context, which runs the bundled script and adds the processor.
    function nrdpOutput() {
        const url = runner['audioWorkletUrl'];
        let context = runner['audioContext'];
        if (context === undefined) {
            // A context made before the output opens never renders.
            nrdp()['audio']['output']['open']();
            context = new AudioContext();
        }
        context['forceStart']?.();
        const queue = audioQueue();
        const output = audioOutputOf(context, queue);
        const holds = newHolds();
        let thread = null;
        let ready = false;
        let ended = false;
        // A processor stuck in a block freezes every context; ending the
        // thread lets sample playback resume.
        const end = (text) => {
            if (ended) {
                return;
            }
            ended = true;
            watch.stop();
            thread?.['terminate']();
            if (ready) {
                // Waits here, where librive cannot meanwhile spin on a lock
                // the ended thread left held.
                for (const until = monotonicNow() + kEndedThreadMs; monotonicNow() < until; ) {}
                releaseLane(holds);
                failOwners(text);
            }
            dropOutput(output, text);
        };
        const watch = watchBlocks(context, holds, kReadyChecks, kStallChecks, end);
        // A throw in a callback NRDP runs would escape the fetch's catch.
        const guard =
            (fn) =>
            (...args) => {
                try {
                    fn(...args);
                } catch (error) {
                    end(noWorklet(error));
                }
            };
        const onReady = guard(() => {
            const node = new AudioWorkletNode(
                context,
                kProcessorNode,
                workletOptions(holds)
            );
            node.connect(context.destination);
            ready = true;
            watch.ready();
            queue.open((message) => thread['send'](message));
        });
        fetch(url)
            .then((response) => {
                if (!response.ok) {
                    throw new Error(url + ' answered HTTP ' + response.status);
                }
                return response.text();
            })
            .then((source) => {
                if (ended) {
                    return;
                }
                const worklet = context.audioWorklet;
                worklet['active'] = true;
                worklet['init'](
                    guard(() => {
                        if (ended) {
                            return;
                        }
                        thread = nrdp()['thread']['start']({
                            'url': url,
                            'source': source,
                            'audio': context,
                        });
                        thread.addEventListener('raw-message', (message) =>
                            message['kind'] === 'ready' ? onReady() : audioEvent(message)
                        );
                    })
                );
            })
            .catch((error) => end(noWorklet(error)));
        return output;
    }

    function audioOutput() {
        audio.output ??= onNrdp() ? nrdpOutput() : browserOutput();
        return audio.output;
    }

    // Forgets the oldest modules no processor runs, past the cache's size.
    function forgetModules(output) {
        const running = new Set([...audio.owners.values()].map((o) => o.moduleId));
        for (const moduleId of output.sent) {
            if (output.sent.size <= kLaneModuleCache) {
                return;
            }
            if (!running.has(moduleId)) {
                output.sent.delete(moduleId);
                output.post({ 'kind': 'forget', 'moduleId': moduleId });
            }
        }
    }

    // Bytes rather than the compiled module: a browser's worklet may sit in
    // another agent cluster, where a module cannot cross.
    function postStart(output, owned, withBytes) {
        const compiled = owned.compiled;
        output.sent.add(owned.moduleId);
        output.post({
            ...owned.start,
            'bytes': withBytes ? compiled.bytes : null,
        });
    }

    // owner names the processor to librive when the worklet answers.
    function audioStart(owner, moduleId, bytes, entry, input, format, hardBlocks, sharedOwner) {
        const compiled = laneModule(moduleId, bytes, 'processor');
        if (compiled.error !== null) {
            return { ok: false, error: compiled.error };
        }
        const output = audioOutput();
        const id = audio.next++;
        const owned = {
            owner,
            moduleId,
            compiled,
            start: {
                'kind': 'start',
                'id': id,
                'moduleId': moduleId,
                'entry': entry,
                'input': input,
                'format': {
                    'sampleRate': format.sampleRate,
                    'maxFrames': format.maxFrames,
                    'channels': format.channels,
                },
                'hardBlocks': hardBlocks,
                'sharedOwner': sharedOwner,
            },
        };
        audio.owners.set(id, owned);
        postStart(output, owned, !output.sent.has(moduleId));
        forgetModules(output);
        return { ok: true, id };
    }

    function audioStop(id) {
        audio.owners.delete(id);
        if (audio.output !== null) {
            audio.output.post({ 'kind': 'stop', 'id': id });
            forgetModules(audio.output);
        }
    }

    // The LaneState a processor reached, or a console line.
    const kProcessorEvents = {
        'running': kLaneStates['running'],
        'failed': kLaneStates['failed'],
        'log': kProcessorEventLog,
    };

    function audioEvent(event) {
        const owned = audio.owners.get(event['id']);
        if (owned === undefined) {
            return;
        }
        if (event['kind'] === 'resend') {
            if (audio.output !== null) {
                postStart(audio.output, owned, true);
            }
            return;
        }
        host.processorEvent(owned.owner, kProcessorEvents[event['kind']], event['text'] ?? '');
    }

    function processorEnd(processor) {
        processors.delete(processor);
    }

    const runner = {
        attach(primitives) {
            host ??= primitives;
        },

        attachThreads(workers) {
            threads = workers;
        },

        register(vm, bytes) {
            const copy = bytes.slice();
            vms.set(vm, {
                ctx: { vm, raised: null, hostFault: false, counters },
                bytes: copy,
                instance: null,
                memory: null,
                preparing: null,
                error: missingSimd(copy),
                names: new Map(),
            });
        },

        release(vm) {
            const entry = vms.get(vm);
            if (entry) {
                // A module still instantiating may call imports afterwards.
                entry.ctx.vm = 0;
                vms.delete(vm);
            }
        },

        prepare() {
            const pending = [];
            for (const entry of vms.values()) {
                if (entry.instance === null && entry.error === null) {
                    entry.preparing ??= prepareEntry(entry);
                    pending.push(entry.preparing);
                }
            }
            return Promise.all(pending);
        },

        // Returns null when the VM has an instance, else the error.
        start(vm) {
            const entry = vms.get(vm);
            if (!entry) {
                return 'script module was never registered';
            }
            if (entry.instance === null && entry.error === null) {
                if (entry.preparing !== null) {
                    return 'script module is still being prepared';
                }
                try {
                    const module = new WebAssembly.Module(liftStart(entry.bytes));
                    announceExports(entry, module);
                    prepareLanes(module, true);
                    adopt(
                        entry,
                        new WebAssembly.Instance(module, importsFor(entry, module))
                    );
                } catch (error) {
                    entry.error = errorText(error);
                }
            }
            return entry.error;
        },

        preparing(vm) {
            const entry = vms.get(vm);
            return entry?.instance === null &&
                entry.error === null &&
                entry.preparing !== null
                ? 1
                : 0;
        },

        hasExport(vm, namePtr) {
            const entry = vms.get(vm);
            return typeof entry?.instance?.exports[host.utf8(namePtr)] ===
                'function'
                ? 1
                : 0;
        },

        // Args arrive as doubles: a u32 is exact in one and the engine
        // converts each to the export's own parameter type.
        call(vm, namePtr, argc, argv, resultPtr, trapBuffer, trapCapacity) {
            const entry = vms.get(vm);
            if (!entry || entry.instance === null) {
                return kMissing;
            }
            // Export names reach librive's call seam as string literals, so
            // the pointer identifies the name.
            let fn = entry.names.get(namePtr);
            if (fn === undefined) {
                fn = entry.instance.exports[host.utf8(namePtr)] ?? null;
                entry.names.set(namePtr, fn);
            }
            if (typeof fn !== 'function') {
                return kMissing;
            }
            const callState = entry.callState;
            const saved = entry.callSaved;
            const base = saved.length;
            for (let i = 0; i < callState.length; i++) {
                saved.push(callState[i].value);
            }
            try {
                const result = callExport(fn, host.heapF64(), argv >> 3, argc);
                host.heapF64()[resultPtr >> 3] =
                    typeof result === 'number' ? result : 0;
                return kOk;
            } catch (error) {
                entry.ctx.raised = null;
                host.writeUtf8(
                    errorText(error),
                    trapBuffer,
                    trapCapacity
                );
                return entry.ctx.hostFault ? kHostFault : kTrapped;
            } finally {
                for (let i = 0; i < callState.length; i++) {
                    callState[i].value = saved[base + i];
                }
                saved.length = base;
            }
        },

        raise(vm, messagePtr) {
            const entry = vms.get(vm);
            if (entry) {
                entry.ctx.raised = host.utf8(messagePtr);
            }
        },

        read(vm, appAddr, dst, size) {
            const memory = vms.get(vm)?.memory;
            if (!memory || appAddr + size > memory.buffer.byteLength) {
                return 0;
            }
            host.heapU8().set(new Uint8Array(memory.buffer, appAddr, size), dst);
            if (counters.on) counters.bytesRead += size;
            return 1;
        },

        write(vm, appAddr, src, size) {
            const memory = vms.get(vm)?.memory;
            if (!memory || appAddr + size > memory.buffer.byteLength) {
                return 0;
            }
            new Uint8Array(memory.buffer, appAddr, size).set(
                host.heapU8().subarray(src, src + size)
            );
            if (counters.on) counters.bytesWritten += size;
            return 1;
        },

        // Length of the zero terminated string at appAddr, or -1.
        stringLength(vm, appAddr) {
            const memory = vms.get(vm)?.memory;
            if (!memory || appAddr >= memory.buffer.byteLength) {
                return -1;
            }
            const end = new Uint8Array(memory.buffer).indexOf(0, appAddr);
            return end < 0 ? -1 : end - appAddr;
        },

        pages(vm) {
            const memory = vms.get(vm)?.memory;
            // Not >>> 16, which reads a full 4 GiB memory as none.
            return memory ? memory.buffer.byteLength / 65536 : 0;
        },

        // File::frameBoundary for every live VM, for hosts that keep no list
        // of their files. Returns the warnings they raised.
        frameBoundary() {
            const warnings = [];
            for (const vm of vms.keys()) {
                const text = host.exports['_rive_web_vm_frame_boundary'](vm);
                if (text !== 0) {
                    warnings.push(host.utf8(text));
                }
            }
            return warnings;
        },

        // Starts or stops tallying the bridge traffic, from zero.
        countHostCalls(on) {
            counters.on = !!on;
            counters.hostCalls = 0;
            counters.bytesStaged = 0;
            counters.bytesCopiedOut = 0;
            counters.bytesRead = 0;
            counters.bytesWritten = 0;
        },

        // Calls into librive, bytes staged into its heap and copied back, and
        // bytes librive read and wrote through its memory seam.
        hostCallCounts() {
            return {
                'hostCalls': counters.hostCalls,
                'bytesStaged': counters.bytesStaged,
                'bytesCopiedOut': counters.bytesCopiedOut,
                'bytesRead': counters.bytesRead,
                'bytesWritten': counters.bytesWritten,
            };
        },
    };
    runner['runJob'] = runJob;
    runner['workerStart'] = workerStart;
    runner['workerWake'] = workerWake;
    runner['workerCollect'] = workerCollect;
    runner['workerEnd'] = workerEnd;
    runner['processorStart'] = processorStart;
    runner['processorRender'] = processorRender;
    runner['processorEnd'] = processorEnd;
    runner['audioRefusal'] = audioRefusal;
    runner['audioSampleRate'] = () => audioOutput().context.sampleRate;
    runner['audioStart'] = audioStart;
    runner['audioStop'] = audioStop;
    // For hosts and tests: where the real time output stands.
    runner['audioState'] = () => ({
        'context': audio.output?.context.state ?? 'none',
        'processors': audio.owners.size,
    });
    // The entries hosts call from outside this script.
    runner['prepare'] = runner.prepare;
    runner['frameBoundary'] = runner.frameBoundary;
    runner['countHostCalls'] = runner.countHostCalls;
    runner['hostCallCounts'] = runner.hostCallCounts;
    Module['riveScripting'] = runner;
    return runner;
}
