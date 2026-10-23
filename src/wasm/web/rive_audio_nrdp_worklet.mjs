// Script audio processors on NRDP's worklet, which has no addModule: the page
// runs the bundle generate.py makes of this file on an NRDP audio thread.

import { createProcessorMixer } from './rive_lane.mjs';
import { kProcessorNode } from './rive_lane_imports.mjs';

const context = globalThis.audioContext;
const worklet = context.audioWorklet;
let mixer = null;
// Starts that beat the node's creation to this thread.
const early = [];
let raw = null;
let width = 2;
// NRDP hands the block as one interleaved buffer.
const add = (block, channels, offset) => {
    const frames = block.length / channels;
    for (let c = 0; c < width; c++) {
        const from = Math.min(c, channels - 1);
        for (let i = 0; i < frames; i++) {
            raw[(offset + i) * width + c] += block[i * channels + from];
        }
    }
};

worklet.addProcessor(kProcessorNode);
// NRDP passes one { id, processorName, processorOptions } object.
worklet.onCreateWorkletNode = (info) => {
    const { memory, table, counter } = info.processorOptions;
    mixer?.failAll('processor lost its audio worklet');
    mixer = createProcessorMixer(
        () => memory.buffer,
        table,
        (message) => nrdp.thread.send(message),
        new Int32Array(counter)
    );
    for (const message of early) {
        mixer.receive(message);
    }
    early.length = 0;
};
worklet.onRenderWorkletNode = (id, inputs, outputs) => {
    const output = outputs[0];
    width = output.numberOfChannels ?? 2;
    raw = output.getRawData();
    raw.fill(0);
    if (mixer !== null) {
        mixer.render(
            raw.length / width,
            Math.round(context.currentTime * context.sampleRate),
            add
        );
    }
};
worklet.onDestroyWorkletNode = () => {
    mixer?.failAll('processor lost its audio worklet');
    mixer = null;
};
nrdp.thread.addEventListener('raw-message', (message) => {
    if (mixer !== null) {
        mixer.receive(message);
    } else {
        early.push(message);
    }
});
nrdp.thread.send({ kind: 'ready' });
