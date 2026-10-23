import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { LaneState } from "rive/lanes";
import { Ring, Snapshot } from "rive/shared";

export class Gain {
    value: f64 = 0;
}

// What the main lane hands a ramp: gains in, a meter back.
export class Link {
    gains: Ring<Gain> = Ring.create<Gain>(256);
    meter: Snapshot<Meter> = Snapshot.create<Meter>();
}

export class Meter {
    frames: f64 = 0;
    gain: f64 = 0;
}

// A ramp in period 100 frames on the engine's clock, scaled by the latest
// gain the main lane sent, right channel inverted. Its output depends only
// on its messages and startFrame, so hosts can check it exactly. A block
// reads and publishes through values made up front, so it never allocates.
export class Ramp extends AudioProcessor<Link> {
    link: Link? = null;
    gain: f64 = 1;
    frames: f64 = 0;
    next: Gain = new Gain();
    meter: Meter = new Meter();

    override init(sampleRate: f64, maxFrames: i32, channels: i32, link: Link): void {
        this.link = link;
        console.log("ramp init " + channels.toString() + " channels");
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        let link = this.link!;
        while (link.gains.receiveInto(this.next)) {
            this.gain = this.next.value;
        }
        for (let i = 0; i < frames; i++) {
            let value = <f32>(this.gain * (((startFrame + <f64>i) % 100) / 100));
            out[i * 2] = value;
            out[i * 2 + 1] = -value;
        }
        this.frames += <f64>frames;
        this.meter.frames = this.frames;
        this.meter.gain = this.gain;
        link.meter.publish(this.meter);
    }
}

export class Thrower extends AudioProcessor<string> {
    message: string = "";

    override init(sampleRate: f64, maxFrames: i32, channels: i32, message: string): void {
        this.message = message;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        throw new Error(this.message);
    }
}

export class Main extends Layout {
    context: Context? = null;
    link: Link = new Link();
    ramp: AudioHandle? = null;
    thrower: AudioHandle? = null;
    reportedRunning: bool = false;
    reportedThrow: bool = false;
    reportedMeter: bool = false;

    override init(context: Context): bool {
        this.context = context;
        let gain = new Gain();
        gain.value = 2.0;
        this.link.gains.send(gain);
        this.ramp = Audio.start(() => new Ramp(), this.link);
        this.thrower = Audio.start(() => new Thrower(), "bad block");
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let ramp = this.ramp!;
        if (!this.reportedRunning && ramp.state == LaneState.running) {
            this.reportedRunning = true;
            context.log("processor running");
        }
        let failed = this.thrower!.failed;
        if (!this.reportedThrow && failed != null) {
            this.reportedThrow = true;
            context.log("processor threw " + failed);
        }
        let meter = this.link.meter.latest();
        if (!this.reportedMeter && meter != null && meter.value.frames >= 512) {
            this.reportedMeter = true;
            context.log("processor meter " + meter.value.frames.toString() + " gain " + meter.value.gain.toString());
        }
        return true;
    }
}
