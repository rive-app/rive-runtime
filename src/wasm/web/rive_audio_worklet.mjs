// Script audio processors in real time on a browser's audio thread: one node
// runs every processor the page starts and sums them into its output.

import { createProcessorMixer } from './rive_lane.mjs';
import { kProcessorNode } from './rive_lane_imports.mjs';

class RiveProcessors extends AudioWorkletProcessor {
    constructor(options) {
        super();
        const { memory, table, counter } = options.processorOptions;
        this.mixer = createProcessorMixer(
            () => memory.buffer,
            table,
            (message) => this.port.postMessage(message),
            new Int32Array(counter)
        );
        this.planes = null;
        // Interleaved in, planar out, at offset frames into the quantum.
        this.add = (block, width, offset) => {
            const planes = this.planes;
            const frames = block.length / width;
            for (let c = 0; c < planes.length; c++) {
                const plane = planes[c];
                const from = Math.min(c, width - 1);
                for (let i = 0; i < frames; i++) {
                    plane[offset + i] += block[i * width + from];
                }
            }
        };
        this.port.onmessage = (event) => this.mixer.receive(event.data);
    }

    process(inputs, outputs) {
        this.planes = outputs[0];
        this.mixer.render(this.planes[0].length, globalThis.currentFrame ?? 0, this.add);
        return true;
    }
}

registerProcessor(kProcessorNode, RiveProcessors);
