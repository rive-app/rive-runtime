import { Layout, Context } from "rive/host";
import { Parallel, Part, SharedArray } from "rive/parallel";
import { clock } from "rive/os";

// Not part of test.sh: the web stage benchmark, which logs what a parallel
// call costs on the page's team, as script_parallel_test.cpp's timings do
// natively, and how fast a fixed amount of work runs on all of the team.
// Run it on a page with threads and --parallel-parts 8, or the parts to
// compare.

// Each part runs passes over its own 1024 floats, a cache line apart from the
// next part's, then waits for the others.
@parallel export function stagesKernel(part: Part, data: SharedArray<f32>, stages: i32, passes: i32): void {
    let start = part.index * 1040;
    let end = start + 1024;
    for (let s = 0; s < stages; s++) {
        for (let p = 0; p < passes; p++) {
            let k = <f32>p * 0.001;
            for (let i = start; i < end; i++) {
                data[i] = data[i] * 0.999 + k;
            }
        }
        part.sync();
    }
}

// The same work as one stage of stagesKernel, where the call's end is the
// barrier.
@parallel export function stageCall(part: Part, data: SharedArray<f32>, passes: i32): void {
    let start = part.index * 1040;
    let end = start + 1024;
    for (let p = 0; p < passes; p++) {
        let k = <f32>p * 0.001;
        for (let i = start; i < end; i++) {
            data[i] = data[i] * 0.999 + k;
        }
    }
}

@parallel export function empty(part: Part, out: SharedArray<f32>): void {}

// Stages of the same work however many parts share it, split evenly.
@parallel export function splitKernel(part: Part, data: SharedArray<f32>, stages: i32, passes: i32): void {
    let start = part.start(data.length);
    let end = part.end(data.length);
    for (let s = 0; s < stages; s++) {
        for (let p = 0; p < passes; p++) {
            let k = <f32>p * 0.001;
            for (let i = start; i < end; i++) {
                data[i] = data[i] * 0.999 + k;
            }
        }
        part.sync();
    }
}

// The same stages, each part claiming chunks until none are left, so a
// faster core takes more of them.
@parallel export function claimKernel(part: Part, data: SharedArray<f32>, stages: i32, passes: i32): void {
    let n = data.length;
    for (let s = 0; s < stages; s++) {
        for (let i = part.claim(n, 256); i < n; i = part.claim(n, 256)) {
            let end = min(i + 256, n);
            for (let p = 0; p < passes; p++) {
                let k = <f32>p * 0.001;
                for (let j = i; j < end; j++) {
                    data[j] = data[j] * 0.999 + k;
                }
            }
        }
        part.sync();
    }
}

const kStages = 1000;
const kReps = 5;

function median(times: Array<f64>): f64 {
    times.sort((a: f64, b: f64): i32 => a < b ? -1 : a > b ? 1 : 0);
    return times[times.length >> 1];
}

// Microseconds per stage when one call syncs between stages.
function syncStageTime(parts: i32, passes: i32): f64 {
    let data = new SharedArray<f32>(1040 * parts);
    let times = new Array<f64>();
    for (let r = 0; r < kReps; r++) {
        let start = clock();
        Parallel.runUpTo(parts, stagesKernel, data, kStages, passes);
        times.push((clock() - start) * 1e6 / <f64>kStages);
    }
    return median(times);
}

// Microseconds per stage when each stage is its own call.
function callStageTime(parts: i32, passes: i32): f64 {
    let data = new SharedArray<f32>(1040 * parts);
    let times = new Array<f64>();
    for (let r = 0; r < kReps; r++) {
        let start = clock();
        for (let s = 0; s < kStages; s++) {
            Parallel.runParts(parts, stageCall, data, passes);
        }
        times.push((clock() - start) * 1e6 / <f64>kStages);
    }
    return median(times);
}

function stageRows(context: Context, target: f64): void {
    let passes = 0;
    if (target > 0) {
        passes = 1;
        let one = callStageTime(1, passes);
        while (one < target && passes < 100000) {
            passes = <i32>(<f64>passes * target / one) + 1;
            one = callStageTime(1, passes);
        }
    }
    let one = callStageTime(1, passes);
    for (let parts = 2; parts <= 8; parts *= 2) {
        let sync = syncStageTime(parts, passes);
        context.log("stage " + target.toString() + " parts " + parts.toString() +
            " sync us " + sync.toString() +
            " calls us " + callStageTime(parts, passes).toString() +
            " efficiency " + (one / sync).toString());
    }
}

// Microseconds per stage on up to parts parts, claimed or split evenly.
function fixedStageTime(claimed: bool, parts: i32, data: SharedArray<f32>, stages: i32, passes: i32): f64 {
    let times = new Array<f64>();
    for (let r = 0; r < kReps; r++) {
        let start = clock();
        if (claimed) {
            Parallel.runUpTo(parts, claimKernel, data, stages, passes);
        } else {
            Parallel.runUpTo(parts, splitKernel, data, stages, passes);
        }
        times.push((clock() - start) * 1e6 / <f64>stages);
    }
    return median(times);
}

// Stages of target microseconds on one part, then on every part the page
// gives a call, split evenly and claimed.
function fixedRows(context: Context, target: f64): void {
    let data = new SharedArray<f32>(16384);
    let stages = max(20, <i32>(20000 / target));
    let passes = 1;
    let one = fixedStageTime(false, 1, data, stages, passes);
    while (one < target && passes < 100000) {
        passes = <i32>(<f64>passes * target / one) + 1;
        one = fixedStageTime(false, 1, data, stages, passes);
    }
    let size = Parallel.size();
    let split = fixedStageTime(false, size, data, stages, passes);
    let claimed = fixedStageTime(true, size, data, stages, passes);
    context.log("fixed " + target.toString() + " parts " + size.toString() +
        " one us " + one.toString() + " split us " + split.toString() +
        " claimed us " + claimed.toString() + " speedup " + (one / split).toString() +
        " " + (one / claimed).toString());
}

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("bench size " + Parallel.size().toString());
        let out = new SharedArray<f32>(1);
        for (let parts = 1; parts <= 8; parts *= 2) {
            let times = new Array<f64>();
            for (let r = 0; r < kReps; r++) {
                let start = clock();
                for (let i = 0; i < 20000; i++) {
                    Parallel.runParts(parts, empty, out);
                }
                times.push((clock() - start) * 1e6 / 20000);
            }
            context.log("empty parts " + parts.toString() + " us " + median(times).toString());
        }
        stageRows(context, 0);
        stageRows(context, 1);
        stageRows(context, 5);
        stageRows(context, 10);
        fixedRows(context, 50);
        fixedRows(context, 200);
        fixedRows(context, 1000);
        context.log("bench done");
        return true;
    }
}
