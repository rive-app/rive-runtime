import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { Worker } from "rive/workers";
import { Ring, Snapshot } from "rive/shared";
import { kWaitStopping } from "rive/lane_codes";

// The ring holds two blocks, so the feeder keeps waiting for the player to
// make space: a read on one lane wakes a worker on another.
export class Link {
    samples: Ring<f32> = Ring.create<f32>(2 * (128 * 4 + 4));
    meter: Snapshot<Meter> = Snapshot.create<Meter>(64);
}

export class Meter {
    received: i32 = 0;
    sum: f64 = 0;
}

export const kTotal: i32 = 4096;

// Writes kTotal numbered samples, a block at a time.
@worker export async function feeder(link: Link): Promise<void> {
    let ring = link.samples;
    let space = ring.spaceSignal;
    let block = new StaticArray<f32>(128);
    let seen = space.seq;
    for (let sent = 0; sent < kTotal; sent += 128) {
        for (let i = 0; i < 128; i++) {
            block[i] = <f32>(sent + i);
        }
        while (!ring.writeF32(block, 128)) {
            let got = await space.next(seen);
            if (got == kWaitStopping) {
                return;
            }
            seen = got;
        }
    }
}

// Plays whatever blocks have arrived and counts what it received.
export class Player extends AudioProcessor<Link> {
    link: Link? = null;
    scratch: StaticArray<f32> = new StaticArray<f32>(128);
    meter: Meter = new Meter();

    override init(sampleRate: f64, maxFrames: i32, channels: i32, link: Link): void {
        this.link = link;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        let link = this.link!;
        let got = link.samples.readF32(this.scratch);
        for (let i = 0; i < got && i < frames; i++) {
            out[i * 2] = this.scratch[i];
            out[i * 2 + 1] = this.scratch[i];
            this.meter.sum += <f64>this.scratch[i];
        }
        this.meter.received += got;
        link.meter.publish(this.meter);
    }
}

export class Main extends Layout {
    context: Context? = null;
    link: Link = new Link();
    worker: Worker? = null;
    player: AudioHandle? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.worker = Worker.spawn(feeder, this.link);
        this.player = Audio.start(() => new Player(), this.link);
        return true;
    }

    override advance(seconds: f64): bool {
        let meter = this.link.meter.latest();
        if (!this.reported && meter != null && meter.value.received >= kTotal) {
            this.reported = true;
            this.context!.log("feed received " + meter.value.received.toString() +
                " sum " + meter.value.sum.toString());
        }
        return true;
    }
}
