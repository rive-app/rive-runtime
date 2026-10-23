import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { Blob, Snapshot } from "rive/shared";

export class Tile {
    name: string = "";
    pbf: Blob? = null;
}

export class Meter {
    played: i32 = 0;
    sum: f64 = 0;
}

export class Link {
    samples: Blob;
    meter: Snapshot<Meter> = Snapshot.create<Meter>(64);

    constructor(samples: Blob) {
        this.samples = samples;
    }
}

export const kSamples: i32 = 4096;

function counting(count: i32): ArrayBuffer {
    let bytes = new Uint8Array(count);
    for (let i = 0; i < count; i++) {
        bytes[i] = <u8>(i * 3);
    }
    return bytes.buffer;
}

function sumOf(bytes: ArrayBuffer): f64 {
    let view = Uint8Array.wrap(bytes);
    let sum = 0.0;
    for (let i = 0; i < view.length; i++) {
        sum += <f64>view[i];
    }
    return sum;
}

@job export function sums(blob: Blob): f64 {
    return sumOf(blob.toArrayBuffer());
}

// Reads through part of one small buffer, as a lane that must not allocate
// does.
@job export function sumsSlices(tile: Tile): f64 {
    let scratch = new ArrayBuffer(16);
    let blob = tile.pbf!;
    let sum = 0.0;
    let offset: u32 = 0;
    let got = blob.readInto(offset, scratch, 4, 7);
    while (got != 0) {
        sum += sumOf(scratch.slice(4, 4 + <i32>got));
        offset += got;
        got = blob.readInto(offset, scratch, 4, 7);
    }
    return sum;
}

// Its result is part of a larger buffer, sent without a slice.
@job export function doubles(blob: Blob): Blob {
    let size = <i32>blob.byteLength;
    let loose = new ArrayBuffer(size + 24);
    blob.readInto(0, loose, 8);
    let bytes = Uint8Array.wrap(loose, 8, size);
    for (let i = 0; i < size; i++) {
        bytes[i] = <u8>(bytes[i] * 2);
    }
    return Blob.from(loose, 8, size);
}

// Plays the blob's samples a block at a time, read without allocating.
export class Sampler extends AudioProcessor<Link> {
    link: Link? = null;
    bytes: ArrayBuffer = new ArrayBuffer(0);
    samples: Float32Array = new Float32Array(0);
    meter: Meter = new Meter();
    at: u32 = 0;

    override init(sampleRate: f64, maxFrames: i32, channels: i32, link: Link): void {
        this.link = link;
        this.bytes = new ArrayBuffer(maxFrames * 4 + 8);
        this.samples = Float32Array.wrap(this.bytes, 8);
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        let link = this.link!;
        let got = <i32>(link.samples.readInto(this.at, this.bytes, 8) / 4);
        if (got > frames) {
            got = frames;
        }
        for (let i = 0; i < got; i++) {
            let sample = this.samples[i];
            out[i * 2] = sample;
            out[i * 2 + 1] = sample;
            this.meter.sum += <f64>sample;
        }
        this.at += <u32>got * 4;
        this.meter.played += got;
        link.meter.publish(this.meter);
    }
}

export class Main extends Layout {
    context: Context? = null;
    link: Link? = null;
    sampler: AudioHandle? = null;
    read: Job<f64>? = null;
    sliced: Job<f64>? = null;
    doubled: Job<Blob>? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        let source = counting(1000);
        let blob = Blob.from(source);
        // The blob holds a copy, so changing the source leaves it alone.
        Uint8Array.wrap(source)[0] = 99;
        context.log("blob bytes " + blob.byteLength.toString() + " sum " +
            sumOf(blob.toArrayBuffer()).toString());
        let part = Blob.from(source, 100, 50);
        let padded = new ArrayBuffer(60);
        let got = part.readInto(10, padded, 5, 30);
        context.log("blob part " + part.byteLength.toString() + " read " + got.toString() +
            " sum " + sumOf(padded).toString());

        let tile = new Tile();
        tile.name = "munich";
        tile.pbf = blob;
        // A call with a budget never runs a job here, so without threads
        // these finish at a later advance.
        this.read = Jobs.run(sums, blob);
        this.sliced = Jobs.run(sumsSlices, tile);
        this.doubled = Jobs.run(doubles, blob);
        this.read!.join();
        this.sliced!.join();
        this.doubled!.join();
        this.reportJobs();

        let samples = new Float32Array(kSamples);
        for (let i = 0; i < kSamples; i++) {
            samples[i] = <f32>i * 0.5;
        }
        let link = new Link(Blob.from(samples.buffer));
        this.link = link;
        this.sampler = Audio.start(() => new Sampler(), link);
        return true;
    }

    reportJobs(): void {
        let context = this.context!;
        let read = this.read;
        let sliced = this.sliced;
        if (read != null && sliced != null && !read.running && !sliced.running) {
            this.read = null;
            context.log("blob job sum " + read.peek()!.value.toString() + " slices " +
                sliced.peek()!.value.toString());
        }
        let job = this.doubled;
        if (job != null && !job.running) {
            this.doubled = null;
            let made = job.peek()!.value;
            // The main lane holds what it decoded, so the job may go.
            job.release();
            context.log("blob made " + made.byteLength.toString() + " doubled " +
                sumOf(made.toArrayBuffer()).toString());
        }
    }

    override advance(seconds: f64): bool {
        this.reportJobs();
        let meter = this.link!.meter.latest();
        if (!this.reported && meter != null && meter.value.played >= kSamples) {
            this.reported = true;
            this.context!.log("sampler played " + meter.value.played.toString() +
                " sum " + meter.value.sum.toString());
        }
        return true;
    }
}
