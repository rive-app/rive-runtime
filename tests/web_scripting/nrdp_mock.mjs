// Stands in for NRDP's audio API as we measured it on devices, so run.mjs can
// drive the runner's NRDP output and rive_audio_worklet_nrdp.js under node:
// - a context renders only once nrdp.audio.output.open() ran before it;
// - audioWorklet.init(callback) lets nrdp.thread.start({ url, source, audio })
//   run our script on a worklet thread, with audioContext as a global;
// - messages cross through nrdp.thread on both sides, as structured clones;
// - each block, onRenderWorkletNode gets one interleaved f32 buffer through
//   getRawData(), 2 channels by 128 frames.
// The worklet thread runs on node's main thread here, so it checks plumbing,
// not timing or threading. fault 'start' makes nrdp.thread.start throw,
// 'silent' drops the thread's ready message and 'fetch' answers HTTP 404. The
// mock answers what the page sent its thread, the node's processorOptions,
// and freeze(), which stops rendering as a processor stuck in a block would.
// Its tap(blocks, report) hashes the first blocks once one is not silent, as
// worklet_tap.mjs does, and reports each channel's sum and the hash.
export function installNrdpMock(readWorklet, fault = null) {
    const kFrames = 128;
    let worklet = null;
    let renderTimer = null;
    const workletListeners = [];
    let tap = null;
    const sample = new Float32Array(1);
    const sampleBytes = new Uint8Array(sample.buffer);
    const tapBlock = (raw) => {
        if (tap === null || tap.blocks === 0 ||
            (tap.sums === null && raw.every((value) => value === 0))) {
            return;
        }
        tap.sums ??= [0, 0];
        for (let i = 0; i < raw.length; i++) {
            sample[0] = raw[i];
            for (const byte of sampleBytes) {
                tap.hash = Math.imul(tap.hash ^ byte, 16777619) >>> 0;
            }
            tap.sums[i % 2] += raw[i];
        }
        if (--tap.blocks === 0) {
            tap.report(tap.sums, tap.hash);
        }
    };
    const mock = {
        sent: [],
        options: null,
        terminated: false,
        freeze: () => clearInterval(renderTimer),
        tap: (blocks, report) => {
            tap = { blocks, report, sums: null, hash: 2166136261 };
        },
    };

    class AudioContext {
        constructor() {
            this.sampleRate = 48000;
            this.state = globalThis.nrdp.audio.output.isOpen ? 'running' : 'closed';
            this.currentTime = 0;
            this.destination = {};
            this.audioWorklet = {
                active: false,
                init: (callback) => setTimeout(callback, 1),
                addProcessor: () => {},
            };
            worklet = this.audioWorklet;
        }
        forceStart() {}
    }

    class AudioWorkletNode {
        constructor(context, name, options) {
            mock.options = options.processorOptions;
            setTimeout(() => {
                worklet.onCreateWorkletNode({
                    id: 1,
                    processorName: name,
                    processorOptions: options.processorOptions,
                });
                const output = {
                    numberOfChannels: 2,
                    raw: new Float32Array(2 * kFrames),
                    getRawData() {
                        return this.raw;
                    },
                };
                renderTimer = setInterval(() => {
                    worklet.onRenderWorkletNode(1, [], [output], {});
                    context.currentTime += kFrames / context.sampleRate;
                    tapBlock(output.raw);
                }, 3);
                renderTimer.unref();
            }, 1);
        }
        connect() {}
    }

    globalThis.AudioContext = AudioContext;
    globalThis.AudioWorkletNode = AudioWorkletNode;
    globalThis.fetch = async (url) =>
        fault === 'fetch'
            ? { ok: false, status: 404 }
            : { ok: true, status: 200, text: async () => readWorklet(url) };
    globalThis.nrdp = {
        audio: {
            output: {
                isOpen: false,
                open() {
                    this.isOpen = true;
                },
            },
        },
        thread: {
            start({ source, audio }) {
                if (fault === 'start') {
                    throw new Error('thread start refused');
                }
                const pageListeners = [];
                const handle = {
                    send: (message) => {
                        const copy = structuredClone(message);
                        mock.sent.push(copy);
                        setTimeout(() => workletListeners.forEach((listener) => listener(copy)), 0);
                    },
                    addEventListener: (type, listener) => pageListeners.push(listener),
                    terminate: () => {
                        mock.terminated = true;
                        clearInterval(renderTimer);
                    },
                };
                const threadSide = {
                    thread: {
                        send: (message) => {
                            if (fault === 'silent' && message.kind === 'ready') {
                                return;
                            }
                            const copy = structuredClone(message);
                            setTimeout(() => pageListeners.forEach((listener) => listener(copy)), 0);
                        },
                        addEventListener: (type, listener) => workletListeners.push(listener),
                    },
                };
                globalThis.audioContext = audio;
                new Function('nrdp', source)(threadSide);
                return handle;
            },
        },
    };
    return mock;
}
