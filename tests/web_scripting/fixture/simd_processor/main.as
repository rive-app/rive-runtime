import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";

// Only the processor uses relaxed SIMD, which an engine validating the main
// module misses.
export class Fused extends AudioProcessor<f64> {
    override init(sampleRate: f64, maxFrames: i32, channels: i32, a: f64): void {
        let fused = f32x4.relaxed_madd(f32x4.splat(<f32>a), f32x4.splat(3), f32x4.splat(1));
        console.log("processor relaxed lane " + i32(f32x4.extract_lane(fused, 0)).toString());
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {}
}

export class Main extends Layout {
    fused: AudioHandle? = null;

    override init(context: Context): bool {
        this.fused = Audio.start(() => new Fused(), 2.0);
        return true;
    }
}
