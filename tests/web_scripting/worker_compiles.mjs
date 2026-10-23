// Counts the modules worker threads compile and the instances they make, for
// run.mjs to report: node --import ./worker_compiles.mjs run.mjs ...
import { getEnvironmentData, isMainThread, setEnvironmentData } from 'worker_threads';

const kKey = 'riveWorkerCompiles';
if (isMainThread) {
    setEnvironmentData(kKey, new SharedArrayBuffer(8));
}
const counts = new Int32Array(getEnvironmentData(kKey));
if (isMainThread) {
    globalThis.riveWorkerCompiles = counts;
} else {
    const counting = (constructor, index) =>
        new Proxy(constructor, {
            construct: (target, args) => (Atomics.add(counts, index, 1), new target(...args)),
        });
    WebAssembly.Module = counting(WebAssembly.Module, 0);
    WebAssembly.Instance = counting(WebAssembly.Instance, 1);
}
