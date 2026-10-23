// An audio worklet that hashes the first blocks of stereo on its input as
// harness_audio in harness.cpp hashes a processor's: FNV-1a over the bytes of
// the interleaved samples, beside each channel's sum. It starts at the first
// block that is not silent, so the processor's own first block must not be.

class Tap extends AudioWorkletProcessor {
    constructor(options) {
        super();
        this.blocks = options.processorOptions.blocks;
        this.started = false;
        this.hash = 2166136261;
        this.sums = [0, 0];
        this.sample = new Float32Array(1);
        this.bytes = new Uint8Array(this.sample.buffer);
    }

    process(inputs) {
        const [left, right] = inputs[0];
        if (this.blocks === 0 || right === undefined) {
            return true;
        }
        const silent = (plane) => plane.every((value) => value === 0);
        if (!this.started && silent(left) && silent(right)) {
            return true;
        }
        this.started = true;
        for (let i = 0; i < left.length; i++) {
            for (const value of [left[i], right[i]]) {
                this.sample[0] = value;
                for (const byte of this.bytes) {
                    this.hash = Math.imul(this.hash ^ byte, 16777619) >>> 0;
                }
            }
            this.sums[0] += left[i];
            this.sums[1] += right[i];
        }
        if (--this.blocks === 0) {
            this.port.postMessage({ hash: this.hash, sums: this.sums });
        }
        return true;
    }
}

registerProcessor('rive-tap', Tap);
