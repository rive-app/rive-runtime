import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { Blob, Ring } from "rive/shared";

export class Reader extends AudioProcessor<Ring<Blob>> {
    ring: Ring<Blob>? = null;
    override init(sampleRate: f64, maxFrames: i32, channels: i32, ring: Ring<Blob>): void {
        this.ring = ring;
    }
    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        this.ring!.receive();
    }
}

// A record naming a blob, which no processor may take, so the reader fails
// and the record stays, as rive-cli's script_jobs_test.cpp asserts natively.
export class Main extends Layout {
    context: Context? = null;
    ring: Ring<Blob> = Ring.create<Blob>(64);
    reader: AudioHandle? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.ring.send(Blob.from(new ArrayBuffer(4)));
        this.reader = Audio.start(() => new Reader(), this.ring);
        return true;
    }

    override advance(seconds: f64): bool {
        let failed = this.reader!.failed;
        if (!this.reported && failed != null) {
            this.reported = true;
            this.context!.log("reader failed: " + failed);
            this.context!.log("ring keeps " + (this.ring.used > 0 ? "its record" : "nothing"));
        }
        return true;
    }
}
