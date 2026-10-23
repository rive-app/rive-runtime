import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { LaneState } from "rive/lanes";

const kVoices: i32 = 16;

// A bank of damped rotators through a one pole filter: float recurrences a
// host hashes, expecting the same bits from every tier. Four voices turn in a
// v128 where the build has SIMD and one at a time where it has not, and both
// sum the voices in the same order.
export class Bank extends AudioProcessor<f64> {
    re: StaticArray<f32> = new StaticArray<f32>(kVoices);
    im: StaticArray<f32> = new StaticArray<f32>(kVoices);
    cos: StaticArray<f32> = new StaticArray<f32>(kVoices);
    sin: StaticArray<f32> = new StaticArray<f32>(kVoices);
    decay: StaticArray<f32> = new StaticArray<f32>(kVoices);
    low: f64 = 0;

    override init(sampleRate: f64, maxFrames: i32, channels: i32, pitch: f64): void {
        for (let v = 0; v < kVoices; v++) {
            let turn = 2 * Math.PI * pitch * 1.37 * <f64>(v + 1) / sampleRate;
            this.cos[v] = <f32>Math.cos(turn);
            this.sin[v] = <f32>Math.sin(turn);
            this.decay[v] = <f32>(1 - 0.0003 * <f64>(v + 1));
            this.re[v] = <f32>(1 / <f64>(v + 1));
        }
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        for (let i = 0; i < frames; i++) {
            let sum = turnVoices(this) * 0.25;
            this.low += (<f64>sum - this.low) * 0.125;
            out[i * 2] = sum;
            out[i * 2 + 1] = <f32>Math.sqrt(Math.abs(this.low));
        }
    }
}

// Turns every voice one frame on and answers their sum.
unsafe function turnVoices(bank: Bank): f32 {
    let sum: f32 = 0;
    if (ASC_FEATURE_SIMD) {
        let reAt = changetype<usize>(bank.re);
        let imAt = changetype<usize>(bank.im);
        let cosAt = changetype<usize>(bank.cos);
        let sinAt = changetype<usize>(bank.sin);
        let decayAt = changetype<usize>(bank.decay);
        for (let at: usize = 0; at < <usize>kVoices << 2; at += 16) {
            let re = v128.load(reAt + at);
            let im = v128.load(imAt + at);
            let cos = v128.load(cosAt + at);
            let sin = v128.load(sinAt + at);
            let decay = v128.load(decayAt + at);
            let turned = f32x4.mul(f32x4.sub(f32x4.mul(re, cos), f32x4.mul(im, sin)), decay);
            im = f32x4.mul(f32x4.add(f32x4.mul(re, sin), f32x4.mul(im, cos)), decay);
            v128.store(reAt + at, turned);
            v128.store(imAt + at, im);
            sum += f32x4.extract_lane(turned, 0);
            sum += f32x4.extract_lane(turned, 1);
            sum += f32x4.extract_lane(turned, 2);
            sum += f32x4.extract_lane(turned, 3);
        }
        return sum;
    }
    for (let v = 0; v < kVoices; v++) {
        let re = bank.re[v];
        let im = bank.im[v];
        let turned = (re * bank.cos[v] - im * bank.sin[v]) * bank.decay[v];
        bank.im[v] = (re * bank.sin[v] + im * bank.cos[v]) * bank.decay[v];
        bank.re[v] = turned;
        sum += turned;
    }
    return sum;
}

export class Main extends Layout {
    context: Context? = null;
    bank: AudioHandle? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.bank = Audio.start(() => new Bank(), 220.0);
        return true;
    }

    override advance(seconds: f64): bool {
        if (!this.reported && this.bank!.state == LaneState.running) {
            this.reported = true;
            this.context!.log("bank running");
        }
        return true;
    }
}
