// Drives the harness the way a web host loads a file: import, await the
// script runner's prepare(), then start scripts and run frames.
//   node run.mjs <harness.mjs> <file.riv> [--sync] [--early] [--settle]
//                [--decoder-throws] [--fault <method>] [--deep] [--vms <count>]
//                [--scroll] [--lacks simd|relaxed|bulk,...]
//                [--page-boundary] [--seam <address>] [--expect <log line>]...
//                [--expect-line <log line>]...
// --early starts scripts once before prepare() settles, as a host that does
// not wait for it would. --deep runs the frames with little stack left.
// --vms is how many script vms must run, else one. --scroll sends one scroll
// over the artboard after the frames.
// --settle stands in for the browser's image decoder and lets its results
// land before the file is released. --decoder-throws settles with a decoder
// that throws as librive starts it. --fault makes librive's first call to the
// named script runner method throw, a JS error raised under librive frames.
// --lacks stands in for an engine without those features, which node always
// has: it refuses any module using their opcodes. --page-boundary starts the
// scripts and services them once through the runner, as the JS runtime does
// at the end of a frame, before the harness runs its own frames. --seam
// round trips a string through librive's module memory seam at that address
// after the frames. --expect wants a line holding the text, --expect-line one
// that is exactly the text.
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { pathToFileURL } from 'url';

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
const lacks = valueOf('--lacks')?.split(',');
const pageBoundary = rest.includes('--page-boundary');
const seam = valueOf('--seam');

// The relaxed opcodes are 0xfd then 0x80 to 0x93 and 0x02, and memory.copy
// and memory.fill 0xfc then 0x0a and 0x0b; a stray byte match only refuses
// more.
function usesSimd(bytes, relaxed) {
    for (let i = 0; i + 2 < bytes.length; i++) {
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
    for (let i = 0; i + 1 < bytes.length; i++) {
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

const lines = [];
const createHarness = (await import(pathToFileURL(resolve(harnessPath)))).default;
const Module = await createHarness({
    print: (text) => lines.push(text),
    printErr: (text) => lines.push(text),
    ...(lacks === undefined ? {} : {
        riveScriptingValidate: (bytes) =>
            refusal(bytes) === null && WebAssembly.validate(bytes),
    }),
});
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
if (fault !== undefined) {
    const runner = Module.riveScripting;
    const original = runner[fault];
    runner[fault] = () => {
        runner[fault] = original;
        throw new Error('host fault injected');
    };
}

const riv = readFileSync(rivPath);
const ptr = Module._malloc(riv.length);
Module.HEAPU8.set(riv, ptr);
const imported = Module._harness_import(ptr, riv.length);
Module._free(ptr);
if (!imported) {
    console.error('FAIL: import failed\n' + lines.join('\n'));
    process.exit(1);
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
if (settle) {
    for (let turn = 0; turn < 5; turn++) {
        await new Promise((done) => setTimeout(done, 10));
    }
}
Module._harness_release();

const missing = expected.filter((want) => !lines.some((line) => line.includes(want)));
missing.push(...exactLines.filter((want) => !lines.includes(want)));
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
