// Checks that browser_audio.mjs fails at once when Chrome exits before the
// page is done, or when a browser cannot start, without either browser.
//   node browser_audio_test.mjs
import { spawnSync } from 'child_process';
import { fileURLToPath } from 'url';

const runner = fileURLToPath(new URL('browser_audio.mjs', import.meta.url));

function check(what, args, env = {}) {
    const run = spawnSync(process.execPath, [runner, 'out', 'missing.riv', ...args], {
        env: { ...process.env, ...env },
        timeout: 20000,
    });
    if (run.status !== 1 || !String(run.stderr).includes('FAIL: ')) {
        console.error(
            `FAIL: ${what} left the runner ${run.signal ? 'hung' : 'exiting ' + run.status}`
        );
        console.error(String(run.stderr));
        process.exit(1);
    }
}

// Node is on every host and exits at once on Chrome's flags.
check('a Chrome that exited early', [], { CHROME: process.execPath });
check('a browser that cannot start', ['--browser', 'netscape']);
console.log('browser audio runner ok');
