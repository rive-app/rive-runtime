// Manual check in real browsers, not part of test.sh: loads the rive_native
// web build on a bare page, imports a baked fixture through the two step
// load, and advances and draws it. Needs playwright installed wherever node
// resolves it from, and rive_native built with:
//   packages/rive_native/native/build.sh wasm release --scripting_vm=both
// Run: node browser_probe.mjs [chrome|firefox|webkit] [basic.riv|trap.riv] [module.wasm]
// With a rasc compiled module it also boots a VM outside a file load, the
// way the editor previews scripts, and checks a bad module is refused.
// The bare page installs no renderer callbacks, so the first script draw
// reports a trap from inside the Flutter renderer. That is the probe's
// doing, and the page surviving it is part of what this shows.
import { chromium, firefox, webkit } from 'playwright';
import http from 'http';
import { readFileSync } from 'fs';
import { fileURLToPath } from 'url';

const here = fileURLToPath(new URL('.', import.meta.url));
const bootModule = process.argv[4] ? readFileSync(process.argv[4]) : null;
const roots = { '/wasm/': here + '../../../rive_native/native/out/release/wasm_both/', '/fixture/': here + 'fixture/' };
const types = { js: 'text/javascript', wasm: 'application/wasm', riv: 'application/octet-stream', html: 'text/html' };
const server = http.createServer((req, res) => {
    res.setHeader('Cross-Origin-Opener-Policy', 'same-origin');
    res.setHeader('Cross-Origin-Embedder-Policy', 'require-corp');
    const url = req.url.split('?')[0];
    if (url === '/boot.wasm' && bootModule) { res.setHeader('content-type', 'application/wasm'); return res.end(bootModule); }
    if (url === '/') { res.setHeader('content-type', 'text/html'); return res.end('<!doctype html><script src="/wasm/rive_native.js"></script>'); }
    for (const [prefix, dir] of Object.entries(roots)) {
        if (url.startsWith(prefix)) {
            try { const b = readFileSync(dir + url.slice(prefix.length)); res.setHeader('content-type', types[url.split('.').pop()] ?? 'application/octet-stream'); return res.end(b); } catch {}
        }
    }
    res.statusCode = 404; res.end();
}).listen(8795, '127.0.0.1');

const which = process.argv[2] ?? 'chrome';
const launch = { chrome: () => chromium.launch({ channel: 'chrome' }), firefox: () => firefox.launch(), webkit: () => webkit.launch() }[which];
const browser = await launch();
const page = await browser.newPage();
const logs = [];
page.on('console', (m) => logs.push(m.text()));
page.on('pageerror', (e) => logs.push('PAGEERROR ' + e.message));
await page.goto('http://127.0.0.1:8795/');
const result = await page.evaluate(async ([riv, withBoot]) => {
    const out = { coi: crossOriginIsolated };
    const m = await RiveNative({ locateFile: (f) => '/wasm/' + f });
    out.hasRunner = typeof m.riveScripting?.prepare === 'function';
    out.hasStart = typeof m._riveFileStartScripts === 'function';
    const bytes = new Uint8Array(await (await fetch('/fixture/' + riv)).arrayBuffer());
    const ptr = m._malloc(bytes.length);
    new Uint8Array(m.wasmMemory.buffer).set(bytes, ptr);
    const factory = m._makeFlutterFactory();
    const file = m.loadRiveFile(ptr, bytes.length, factory, null, 0);
    out.file = file !== 0;
    const t0 = performance.now();
    await m.riveScripting.prepare();
    out.prepareMs = +(performance.now() - t0).toFixed(2);
    m._riveFileStartScripts(file);
    const artboard = m._riveFileArtboardDefault(file, true);
    out.artboard = artboard !== 0;
    let renderer = 0;
    try { renderer = m._makeFlutterRenderer(factory); } catch (e) { out.rendererError = String(e); }
    for (let i = 0; i < 5; i++) {
        try { m._riveArtboardAdvance(artboard, 1 / 60, 9); m._riveArtboardUpdatePass(artboard); } catch (e) { out.advanceError = String(e); break; }
        if (renderer) { try { m._artboardDraw(artboard, renderer); out.draws = (out.draws ?? 0) + 1; } catch (e) { out.drawError = String(e).slice(0, 160); renderer = 0; } }
    }
    if (withBoot) {
        const boot = (bytes) => {
            const p = m._malloc(bytes.length);
            new Uint8Array(m.wasmMemory.buffer).set(bytes, p);
            const vm = m.riveWasmVMBoot(p, bytes.length, factory, () => {});
            m._free(p);
            return vm;
        };
        const module = new Uint8Array(await (await fetch('/boot.wasm')).arrayBuffer());
        const vm = boot(module);
        out.bootedVM = vm !== 0;
        if (vm !== 0) m._riveWasmVMDelete(vm);
        out.badModuleRefused = boot(new Uint8Array([0, 97, 115, 109, 1, 0, 0, 0, 99])) === 0;
    }
    return out;
}, [process.argv[3] ?? 'basic.riv', bootModule !== null]);
await new Promise((r) => setTimeout(r, 300));
console.log(which, JSON.stringify(result));
for (const l of logs) console.log('  | ' + l.slice(0, 200));
await browser.close();
server.close();
