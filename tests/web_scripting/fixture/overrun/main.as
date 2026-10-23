import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";

// Takes far longer than a block every block: natively the watchdog
// interrupts it; a worklet ends it at a fuel check, or without fuel after.
export class Slow extends AudioProcessor<i32> {
    spins: i32 = 0;

    override init(sampleRate: f64, maxFrames: i32, channels: i32, spins: i32): void {
        this.spins = spins;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        let x: f64 = 0;
        for (let i = 0; i < this.spins; i++) {
            x += Math.sqrt(<f64>i);
        }
        out[0] = <f32>(x * 0);
    }
}

export class Main extends Layout {
    context: Context? = null;
    slow: AudioHandle? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.slow = Audio.start(() => new Slow(), 50000000);
        return true;
    }

    override advance(seconds: f64): bool {
        let failed = this.slow!.failed;
        if (!this.reported && failed != null) {
            this.reported = true;
            this.context!.log("slow failed: " + failed);
        }
        return true;
    }
}
