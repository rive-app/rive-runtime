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

// Its init never returns, so in a worklet only its fuel ends it.
export class Spinner extends AudioProcessor<i32> {
    override init(sampleRate: f64, maxFrames: i32, channels: i32, seed: i32): void {
        let x = seed;
        while (x >= 0) {
            x = (x + 1) & 0xffff;
        }
    }
}

export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(8);
    spinners: Array<AudioHandle> = new Array<AudioHandle>();
    quiet: AudioHandle? = null;
    blocksAtFailure: i64 = -1;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        // Both start in one batch, so the page must wait out each init.
        this.spinners.push(Audio.start(() => new Spinner(), 0));
        this.spinners.push(Audio.start(() => new Spinner(), 1));
        this.quiet = Audio.start(() => new Quiet(), this.region);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let spinners = this.spinners;
        let blocks = <i64>this.region.atomicLoad(0);
        if (this.blocksAtFailure < 0) {
            for (let i = 0; i < spinners.length; i++) {
                if (spinners[i].failed == null) {
                    return true;
                }
            }
            this.blocksAtFailure = blocks;
            for (let i = 0; i < spinners.length; i++) {
                context.log("spinner " + i.toString() + " failed: " + spinners[i].failed!);
            }
        } else if (!this.reported && blocks > this.blocksAtFailure + 4) {
            this.reported = true;
            context.log("quiet rendered after the spinners failed");
        }
        return true;
    }
}
