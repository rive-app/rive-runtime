import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { SharedRegion } from "rive/shared";

// Counts the blocks it renders.
export class Quiet extends AudioProcessor<SharedRegion> {
    region: SharedRegion? = null;

    override init(sampleRate: f64, maxFrames: i32, channels: i32, region: SharedRegion): void {
        this.region = region;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        this.region!.atomicAdd(0, 1);
    }
}

// Never returns from a block once it ran a few, so in a worklet only its
// fuel ends it.
export class Stuck extends AudioProcessor<i32> {
    blocks: i32 = 0;
    after: i32 = 0;

    override init(sampleRate: f64, maxFrames: i32, channels: i32, after: i32): void {
        this.after = after;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        this.blocks++;
        while (this.blocks > this.after) {
            out[0] = <f32>this.blocks;
        }
    }
}

export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(8);
    quiet: AudioHandle? = null;
    stuck: AudioHandle? = null;
    quietReported: bool = false;
    blocksAtFailure: i64 = -1;
    renderedReported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.quiet = Audio.start(() => new Quiet(), this.region);
        this.stuck = Audio.start(() => new Stuck(), 20);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let quietFailed = this.quiet!.failed;
        if (!this.quietReported && quietFailed != null) {
            this.quietReported = true;
            context.log("quiet failed: " + quietFailed);
        }
        let stuckFailed = this.stuck!.failed;
        let blocks = <i64>this.region.atomicLoad(0);
        if (this.blocksAtFailure < 0 && stuckFailed != null) {
            this.blocksAtFailure = blocks;
            context.log("stuck failed: " + stuckFailed);
        } else if (this.blocksAtFailure >= 0 && !this.renderedReported &&
            blocks > this.blocksAtFailure + 4) {
            this.renderedReported = true;
            context.log("quiet rendered after stuck failed");
        }
        return true;
    }
}
