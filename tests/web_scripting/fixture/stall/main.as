import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { LaneState } from "rive/lanes";
import { Ring } from "rive/shared";

export class Quiet extends AudioProcessor<i32> {
    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {}
}

// The page freezes the audio thread while that thread seems to hold the
// ring's lock, which the main lane takes only once the stall failed the
// processor.
export class Main extends Layout {
    context: Context? = null;
    ring: Ring<f64> = Ring.create<f64>(64);
    quiet: AudioHandle? = null;
    reportedRunning: bool = false;
    reportedFailed: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.quiet = Audio.start(() => new Quiet(), 0);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let quiet = this.quiet!;
        if (!this.reportedRunning && quiet.state == LaneState.running) {
            this.reportedRunning = true;
            context.log("stall ring " + this.ring.id.toString());
        }
        let failed = quiet.failed;
        if (!this.reportedFailed && failed != null) {
            this.reportedFailed = true;
            context.log("stall failed: " + failed);
            let sent = this.ring.send(1.0);
            context.log("stall ring written after " + (sent ? "yes" : "no"));
        }
        return true;
    }
}
