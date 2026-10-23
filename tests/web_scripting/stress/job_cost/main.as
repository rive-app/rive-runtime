// Times what a job costs the frame that runs and joins it, or without threads
// the advances a batch takes: rive_tests "[job-cost]", or run.mjs on the web.
import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { clock } from "rive/os";

const WARMUP: i32 = 20;
const FRAMES: i32 = 300;
const SAMPLES: i32 = 300000;
const STEPPED: i32 = 2000;

@job export function noop(x: i32): i32 {
    return x;
}

@job export function samples(count: i32): Float32Array {
    let out = new Float32Array(count);
    for (let i = 0; i < count; i++) {
        out[i] = <f32>i;
    }
    return out;
}

function ms(began: f64): f64 {
    return (clock() - began) * 1000.0;
}

function round(value: f64): string {
    return (Math.round(value * 10000.0) / 10000.0).toString();
}

function report(context: Context, name: string, times: Array<f64>): void {
    times.sort((a: f64, b: f64): i32 => (a < b ? -1 : a > b ? 1 : 0));
    context.log(
        "job cost " + name + " median " + round(times[times.length >> 1]) +
            " min " + round(times[0]) + " ms",
    );
}

export class Main extends Layout {
    context: Context? = null;
    frame: i32 = 0;
    phase: i32 = 0;
    missed: i32 = 0;
    times: Array<f64> = [];
    stepped: Array<Job<i32>> = [];
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    // Each kind of job has frames to itself, so none pays for what another
    // left in the instance it takes.
    override advance(seconds: f64): bool {
        if (this.done) {
            return true;
        }
        let frame = this.frame++;
        if (Jobs.threads() == 0) {
            this.advanceStepped(frame);
            return true;
        }
        let phase = this.phase;
        let began = clock();
        let joined = true;
        if (phase == 0) {
            joined = Jobs.run(noop, frame).join() != null;
        } else if (phase == 1) {
            let jobs = new Array<Job<i32>>();
            for (let i = 0; i < 6; i++) {
                jobs.push(Jobs.run(noop, i));
            }
            for (let i = 0; i < 6; i++) {
                joined = jobs[i].join() != null && joined;
            }
        } else {
            let result = Jobs.run(samples, SAMPLES).join();
            joined = result != null && result.value.length == SAMPLES;
        }
        let took = ms(began);
        if (!joined) {
            this.missed++;
        } else if (frame >= WARMUP) {
            this.times.push(took);
        }
        if (frame < WARMUP + FRAMES - 1) {
            return true;
        }
        let context = this.context!;
        report(context, phase == 0 ? "one no-op" : phase == 1 ? "six no-ops" : "1.2 MB result", this.times);
        this.times = [];
        this.frame = 0;
        if (++this.phase == 3) {
            context.log("job cost done, " + this.missed.toString() + " frames missed a join");
            this.done = true;
        }
        return true;
    }

    advanceStepped(frame: i32): void {
        let jobs = this.stepped;
        if (frame == 0) {
            for (let i = 0; i < STEPPED; i++) {
                jobs.push(Jobs.run(noop, i));
            }
            return;
        }
        if (jobs[jobs.length - 1].running) {
            return;
        }
        this.context!.log(
            "job cost stepped " + STEPPED.toString() + " no-ops over " + frame.toString() +
                " advances",
        );
        this.context!.log("job cost done, stepped");
        this.done = true;
    }
}
