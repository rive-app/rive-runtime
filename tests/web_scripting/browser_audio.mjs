// Runs a baked riv on a browser page, where script audio processors render
// in real time on the page's audio worklet (rive_audio_worklet.mjs), and
// checks the page's console for lines.
//   node browser_audio.mjs <harness out dir> <file.riv> [--browser <name>]
//                          [--no-isolation] [--runner-context]
//                          [--carried-worklet]
//                          [--offline <blocks>] [--hash <blocks>]
//                          [--hard-blocks <n>] [--job-threads <n>]
//                          [--parallel-parts <n>] [--expect <line>]...
// --browser is chrome, the default, run from the binary CHROME names or else
// the macOS install, or firefox or webkit, run through playwright-core: npm
// ci here, then npx playwright-core install firefox webkit. --no-isolation
// serves the page without cross origin isolation, which a build with
// threads refuses at once. --runner-context hands the runner no
// AudioContext, so it makes its own, and firefox and webkit click only once
// the page logs a warning. --carried-worklet names no worklet and serves none
// of its files, so the runner starts the one the build carries. --offline renders that many blocks through each
// processor on the page, as run.mjs's --audio does, and --hash logs a hash of
// the first that many blocks the worklet renders, as harness_audio does.
// --hard-blocks sets the audio hard limit, --job-threads the job pool's
// threads and --parallel-parts a parallel call's parts, as run.mjs's do.
import { spawn } from 'child_process';
import { createServer } from 'http';
import { mkdtempSync, readFileSync, rmSync } from 'fs';
import { tmpdir } from 'os';
import { basename, dirname, extname, join, resolve } from 'path';
import { fileURLToPath } from 'url';

const [outDir, rivPath, ...rest] = process.argv.slice(2);
const valueOf = (flag) => rest.find((arg, i) => rest[i - 1] === flag);
const expected = rest.filter((arg, i) => rest[i - 1] === '--expect');
const browserName = valueOf('--browser') ?? 'chrome';
const isolated = !rest.includes('--no-isolation');
const runnerContext = rest.includes('--runner-context');
const carriedWorklet = rest.includes('--carried-worklet');
const offline = Number(valueOf('--offline') ?? 0);
const hash = Number(valueOf('--hash') ?? 0);
const hardBlocks = valueOf('--hard-blocks');
const jobThreads = valueOf('--job-threads');
const parallelParts = valueOf('--parallel-parts');
const chrome =
    process.env.CHROME ??
    '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome';
const here = dirname(fileURLToPath(import.meta.url));
const webDir = resolve(here, '../../src/wasm/web');
const sources = {
    'file.riv': rivPath,
    'worklet_tap.mjs': join(here, 'worklet_tap.mjs'),
};
for (const name of carriedWorklet ? [] : [
    'rive_audio_worklet.mjs',
    'rive_lane_imports.mjs',
    'rive_shared_layout.mjs',
    'rive_shared_lane.mjs',
    'rive_lane.mjs',
]) {
    sources[name] = join(webDir, name);
}

// The page: load the harness, name the worklet, import the file, then run
// frames while the worklet renders, until every expected line shows.
const page = `<!doctype html><meta charset="utf-8"><button>Start audio</button>
<script type="module">
import createHarness from './web_scripting_page.mjs';
const lines = [];
const print = (text) => { lines.push(text); console.log('PAGE ' + text); };
console.warn = (text) => print('warning: ' + text);
// At harness_audio's rate, and started by a click as a page's audio is.
const context = ${runnerContext} ? undefined : new AudioContext({ sampleRate: 48000 });
document.querySelector('button').onclick = () => context?.resume();
const Module = await createHarness({ print, printErr: print }).catch((error) => {
    // A build with threads refuses a page that cannot share memory.
    print('harness refused: ' + error.message);
    console.log('PAGE DONE');
    return new Promise(() => {});
});
const hardBlocks = ${JSON.stringify(hardBlocks)};
if (hardBlocks !== undefined) {
    Module._harness_audio_hard_blocks(Number(hardBlocks));
}
const jobThreads = ${JSON.stringify(jobThreads)};
if (jobThreads !== undefined) {
    Module._harness_job_threads(Number(jobThreads));
}
Module.riveScripting.parallelParts = ${JSON.stringify(parallelParts)};
if (!${carriedWorklet}) {
    Module.riveScripting.audioWorkletUrl = 'rive_audio_worklet.mjs';
}
Module.riveScripting.audioContext = context;
const hash = ${hash};
if (hash > 0) {
    await context.audioWorklet.addModule('worklet_tap.mjs');
    const tap = new AudioWorkletNode(context, 'rive-tap', {
        outputChannelCount: [2],
        channelCount: 2,
        channelCountMode: 'explicit',
        processorOptions: { blocks: hash },
    });
    tap.connect(context.destination);
    tap.port.onmessage = ({ data }) => {
        const [left, right] = data.sums.map((sum) => sum.toFixed(2));
        const hex = data.hash.toString(16).padStart(8, '0');
        print(\`worklet rendered \${hash} blocks sums \${left} \${right} hash \${hex}\`);
    };
    // Every node the runner makes feeds the tap as well.
    const Node = AudioWorkletNode;
    globalThis.AudioWorkletNode = class extends Node {
        constructor(...args) {
            super(...args);
            this.connect(tap);
        }
    };
}
const riv = new Uint8Array(await (await fetch('file.riv')).arrayBuffer());
const ptr = Module._malloc(riv.length);
Module.HEAPU8.set(riv, ptr);
Module._harness_import(ptr, riv.length);
Module._free(ptr);
const offline = ${offline};
if (offline > 0) {
    Module._harness_render_audio_offline();
}
await Module.riveScripting.prepare();
Module._harness_run(1);
const frame = async () => {
    await new Promise((done) => setTimeout(done, 10));
    Module._harness_frames(1);
};
if (offline > 0) {
    // A processor still in its init counts as pending.
    for (let turn = 0; turn < 1000 && Module._harness_pending(); turn++) {
        await frame();
    }
    Module._harness_audio(offline);
}
const expected = ${JSON.stringify(expected)};
for (let turn = 0; turn < 1000; turn++) {
    if (expected.every((want) => lines.some((line) => line.includes(want)))) {
        break;
    }
    await frame();
}
print('page audio ' + JSON.stringify(Module.riveScripting.audioState()));
print('page parallel ' + JSON.stringify(Module.riveScripting.parallelState()));
Module._harness_release();
console.log('PAGE DONE');
</script>`;

const types = {
    '.html': 'text/html',
    '.js': 'text/javascript',
    '.mjs': 'text/javascript',
    '.wasm': 'application/wasm',
    '.riv': 'application/octet-stream',
};
const server = createServer((request, response) => {
    const name = basename(new URL(request.url, 'http://x').pathname) || 'index.html';
    let body;
    try {
        body =
            name === 'index.html'
                ? page
                : readFileSync(sources[name] ?? join(resolve(outDir), name));
    } catch {
        response.writeHead(404).end();
        return;
    }
    const headers = { 'Content-Type': types[extname(name)] ?? 'application/octet-stream' };
    // Shared memory, so pthreads and the worklet, needs cross origin isolation.
    if (isolated) {
        headers['Cross-Origin-Opener-Policy'] = 'same-origin';
        headers['Cross-Origin-Embedder-Policy'] = 'require-corp';
    }
    response.writeHead(200, headers);
    response.end(body);
});
await new Promise((done) => server.listen(0, '127.0.0.1', done));
const url = `http://127.0.0.1:${server.address().port}/index.html`;

const lines = [];
let settle;
// True once the page is done, false if the browser goes first.
const done = new Promise((resolve) => {
    settle = resolve;
    setTimeout(() => settle(false), 60000).unref();
});
let warn;
const warned = new Promise((resolve) => (warn = resolve));
// One console message from the page: its own lines, and any error.
const take = (text, error = false) => {
    if (!text.startsWith('PAGE ')) {
        if (error) {
            lines.push(text);
        }
        return;
    }
    const line = text.slice(5);
    if (line === 'DONE') {
        settle(true);
    }
    if (line.startsWith('warning: ')) {
        warn();
    }
    lines.push(line);
};

// Chrome's own binary, with its console on stderr. Returns how to close it.
function openChrome() {
    const profile = mkdtempSync(join(tmpdir(), 'rive-audio-chrome-'));
    const browser = spawn(chrome, [
        '--headless=new',
        '--autoplay-policy=no-user-gesture-required',
        '--enable-logging=stderr',
        '--v=0',
        '--no-first-run',
        `--user-data-dir=${profile}`,
        url,
    ]);
    const exited = new Promise((resolve) => browser.on('exit', resolve));
    browser.on('close', () => settle(false));
    let pending = '';
    browser.stderr.on('data', (chunk) => {
        pending += chunk.toString();
        const parts = pending.split('\n');
        pending = parts.pop();
        for (const part of parts) {
            if (part.includes('CONSOLE')) {
                // A message of several lines closes its quote on its last.
                const open = part.indexOf('"');
                const close = part.lastIndexOf('"');
                take(part.slice(open + 1, close > open ? close : undefined), part.includes('rror'));
            }
        }
    });
    return async () => {
        browser.kill();
        await exited;
        // Chrome's helpers can still write the profile after it exits; a
        // leftover temporary folder is harmless.
        try {
            rmSync(profile, { recursive: true, force: true, maxRetries: 5 });
        } catch {}
    };
}

// Firefox or WebKit through playwright, which clicks the page's button as a
// user would. Returns how to close it.
async function openPlaywright() {
    const engines = await import('playwright-core');
    const engine = { firefox: engines.firefox, webkit: engines.webkit }[browserName];
    if (engine === undefined) {
        throw new Error(`no browser ${browserName}`);
    }
    const browser = await engine.launch();
    browser.on('disconnected', () => settle(false));
    const tab = await browser.newPage();
    tab.on('console', (message) => take(message.text(), message.type() === 'error'));
    tab.on('pageerror', (error) => lines.push(`page error: ${error.message}`));
    await tab.goto(url);
    if (runnerContext) {
        // A gesture only after the runner's context began without one.
        await Promise.race([warned, done]);
    }
    await tab.click('button');
    return () => browser.close();
}

let close = async () => {};
try {
    close = browserName === 'chrome' ? openChrome() : await openPlaywright();
} catch (error) {
    lines.push(String(error.message ?? error));
    settle(false);
}
const finished = await done;
await close();
server.close();

const missing = expected.filter((want) => !lines.some((line) => line.includes(want)));
// A thread past the pool starts only once the page yields.
const exhausted = lines.some((line) => line.includes('thread pool is exhausted'));
if (!finished || missing.length > 0 || exhausted) {
    const how = !finished ? 'did not finish' : exhausted ? 'ran out of pool threads' : 'finished';
    console.error(`FAIL: ${browserName} ${how}, missing log lines: ${JSON.stringify(missing)}`);
    console.error(lines.join('\n'));
    process.exit(1);
}
console.log(`browser audio ok in ${browserName}: ${lines.length} log lines`);
for (const line of lines) {
    console.log('  | ' + line);
}
