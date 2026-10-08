// Drives the harness the way a web host loads a file: import, await the
// script runner's prepare(), then start scripts and run frames.
//   node run.mjs <harness.mjs> <file.riv> [--sync] [--early] [--settle]
//                [--decoder-throws] [--fault <method>] [--deep] [--vms <count>]
//                [--scroll] [--lacks simd|relaxed] [--expect <log line>]...
// --early starts scripts once before prepare() settles, as a host that does
// not wait for it would. --deep runs the frames with little stack left.
// --vms is how many script vms must run, else one. --scroll sends one scroll
// over the artboard after the frames.
// --settle stands in for the browser's image decoder and lets its results
// land before the file is released. --decoder-throws settles with a decoder
// that throws as librive starts it. --fault makes librive's first call to the
// named script runner method throw, a JS error raised under librive frames.
// --lacks stands in for an engine without that SIMD flavor, which node always
// has: it refuses any module using its opcodes.
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { pathToFileURL } from 'url';

const [harnessPath, rivPath, ...rest] = process.argv.slice(2);
const sync = rest.includes('--sync');
const decoderThrows = rest.includes('--decoder-throws');
const settle = rest.includes('--settle') || decoderThrows;
const valueOf = (flag) => rest.find((arg, i) => rest[i - 1] === flag);
const expected = rest.filter((arg, i) => rest[i - 1] === '--expect');
const fault = valueOf('--fault');
const early = rest.includes('--early');
const deep = rest.includes('--deep');
const vms = valueOf('--vms');
const scroll = rest.includes('--scroll');
const lacks = valueOf('--lacks');

// The relaxed opcodes are 0xfd then 0x80 to 0x93 and 0x02; a stray byte match
// only refuses more, and no case here expects a scalar module to run.
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
            !usesSimd(bytes, lacks === 'relaxed') && WebAssembly.validate(bytes),
    }),
});
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
const running = deep
    ? (Module._harness_run(0), Module._harness_run_low_on_stack(5, 16384))
    : Module._harness_run(5);
if (settle) {
    for (let turn = 0; turn < 5; turn++) {
        await new Promise((done) => setTimeout(done, 10));
    }
}
Module._harness_release();

const missing = expected.filter((want) => !lines.some((line) => line.includes(want)));
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
