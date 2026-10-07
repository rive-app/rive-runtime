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

// WasmScriptingVM::CallOutcome, then the browser's own host fault.
const kOk = 0;
const kMissing = 1;
const kTrapped = 2;
const kHostFault = 3;
const kLiftedStart = '__riveLiftedStart';

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

// rasc runs every script's top level in the module's start function, and a
// top level can pass memory to the host. The module's memory is an export,
// which the page cannot reach until start has returned. So the start section
// is dropped and its function exported, for the runner to call once the
// instance and its memory exist. Modules without one come back untouched.
function liftStart(bytes) {
    const sections = [];
    let startFunction = -1;
    for (let at = 8; at < bytes.length; ) {
        const [size, body] = readLeb(bytes, at + 1);
        if (bytes[at] === 8) {
            startFunction = readLeb(bytes, body)[0];
        } else {
            sections.push({ id: bytes[at], at, body, end: body + size });
        }
        at = body + size;
    }
    if (startFunction < 0 || !sections.some((section) => section.id === 7)) {
        return bytes;
    }
    const parts = [bytes.subarray(0, 8)];
    for (const section of sections) {
        if (section.id !== 7) {
            parts.push(bytes.subarray(section.at, section.end));
            continue;
        }
        const [count, first] = readLeb(bytes, section.body);
        const name = Array.from(kLiftedStart, (c) => c.charCodeAt(0));
        const entry = [...leb(name.length), ...name, 0, ...leb(startFunction)];
        const newCount = leb(count + 1);
        const rest = bytes.subarray(first, section.end);
        const size = newCount.length + rest.length + entry.length;
        parts.push(
            Uint8Array.from([7, ...leb(size), ...newCount]),
            rest,
            Uint8Array.from(entry)
        );
    }
    const out = new Uint8Array(parts.reduce((sum, part) => sum + part.length, 0));
    let at = 0;
    for (const part of parts) {
        out.set(part, at);
        at += part.length;
    }
    return out;
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

    function importsFor(entry, module) {
        const imports = createRiveModuleImports(
            {
                calls: bindRiveHostCalls(host.hostCall, entry.ctx),
                heapU8: host.heapU8,
                stackSave: host.stackSave,
                stackAlloc: host.stackAlloc,
                stackRestore: host.stackRestore,
                malloc: host.malloc,
                free: host.free,
                counters,
            },
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

    // Read off the module before it starts: the start function runs every
    // script's top level, which already passes strings.
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
    }

    function adopt(entry, instance) {
        entry.memory = instance.exports['memory'];
        entry.bytes = null;
        const booting = host.exports['_rive_web_vm_booting'];
        booting(entry.ctx.vm);
        const mark = host.stackSave();
        try {
            instance.exports[kLiftedStart]?.();
        } catch (error) {
            // A host fault skips the epilogues of the librive frames it unwinds.
            host.stackRestore(mark);
            entry.ctx.raised = null;
            throw error;
        } finally {
            booting(0);
        }
        entry.instance = instance;
    }

    async function prepareEntry(entry) {
        try {
            const module = await WebAssembly.compile(liftStart(entry.bytes));
            if (entry.ctx.vm === 0) {
                return;
            }
            announceExports(entry, module);
            const instance = await WebAssembly.instantiate(
                module,
                importsFor(entry, module)
            );
            // Released while instantiating.
            if (entry.ctx.vm === 0) {
                return;
            }
            adopt(entry, instance);
        } catch (error) {
            entry.error = String(error?.message ?? error);
        }
    }

    const runner = {
        attach(primitives) {
            host ??= primitives;
        },

        register(vm, bytes) {
            vms.set(vm, {
                ctx: { vm, raised: null, hostFault: false, counters },
                bytes: bytes.slice(),
                instance: null,
                memory: null,
                preparing: null,
                error: null,
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
                    adopt(
                        entry,
                        new WebAssembly.Instance(module, importsFor(entry, module))
                    );
                } catch (error) {
                    entry.error = String(error?.message ?? error);
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
            try {
                const at = argv >> 3;
                const result = fn(...host.heapF64().subarray(at, at + argc));
                host.heapF64()[resultPtr >> 3] =
                    typeof result === 'number' ? result : 0;
                return kOk;
            } catch (error) {
                entry.ctx.raised = null;
                host.writeUtf8(
                    String(error?.message ?? error),
                    trapBuffer,
                    trapCapacity
                );
                return entry.ctx.hostFault ? kHostFault : kTrapped;
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
            return memory ? memory.buffer.byteLength >>> 16 : 0;
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
    // The entries hosts call from outside this script.
    runner['prepare'] = runner.prepare;
    runner['countHostCalls'] = runner.countHostCalls;
    runner['hostCallCounts'] = runner.hostCallCounts;
    Module['riveScripting'] = runner;
    return runner;
}
