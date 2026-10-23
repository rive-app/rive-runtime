import { Layout, Context } from "rive/host";
import { Worker } from "rive/workers";
import { Port } from "rive/ports";
import { Barrier, SharedRegion } from "rive/shared";

const kWorkers: i32 = 4;
const kChunkRounds: i32 = 48;
const kChunkedRounds: i32 = 12 * kChunkRounds;
const kRounds: i32 = kChunkedRounds + 480;
const kValues: i32 = 64;

// The voice engine's 2, 2 and 4 worker chunks, where two sit out and run
// ahead, then counts that alternate so shares land on the last result.
function participants(round: i32): i32 {
    if (round < kChunkedRounds) {
        return (round / kChunkRounds) % 3 < 2 ? 2 : kWorkers;
    }
    return round % 2 == 0 ? 2 : kWorkers;
}

// Large values among small ones, so a sum in another order rounds otherwise,
// and a NaN and signed zeros in every order, which comparisons get wrong.
function value(round: i32, index: i32, i: i32): f32 {
    let turn = round / 4 + i / 16;
    if (i % 16 == 1) {
        return turn % kWorkers == index ? NaN : <f32>index;
    }
    if (i % 16 == 2) {
        return (turn + index) % 2 == 0 ? -0.0 : 0.0;
    }
    if (i % 16 == 3) {
        return -0.0;
    }
    let small = <f32>((round * 7 + i * 3 + index) % 11 - 5);
    return (round + i + index) % 4 == 0 ? small * 1e7 : small;
}

function combined(op: i32, a: f32, b: f32): f32 {
    return op == 1 ? a + b : op == 2 ? min<f32>(a, b) : max<f32>(a, b);
}

// NaN's bits differ between tiers, so any NaN matches another.
function same(a: f32, b: f32): bool {
    return isNaN(a) ? isNaN(b) : reinterpret<u32>(a) == reinterpret<u32>(b);
}

export class Report {
    index: i32 = 0;
    met: i32 = 0;
    wrong: i32 = 0;
}

// Meets each of its rounds with arrive, sum, min and max in turn, checking
// each result against the shares combined in index order.
@worker export async function part(index: i32, barrier: Barrier, done: Port<Report>): Promise<void> {
    let share = new Float32Array(kValues);
    let out = new Float32Array(kValues);
    let report = new Report();
    report.index = index;
    for (let round = 0; round < kRounds; round++) {
        let count = participants(round);
        if (index >= count) {
            continue;
        }
        let op = round % 4;
        for (let i = 0; i < kValues; i++) {
            share[i] = value(round, index, i);
        }
        let met = false;
        if (op == 0) {
            met = await barrier.arrive(round, count);
        } else if (op == 1) {
            met = await barrier.sum(round, count, index, share, out);
        } else if (op == 2) {
            met = await barrier.min(round, count, index, share, out);
        } else {
            met = await barrier.max(round, count, index, share, out);
        }
        if (!met) {
            done.post(report);
            return;
        }
        report.met++;
        if (op == 0) {
            continue;
        }
        for (let i = 0; i < kValues; i++) {
            let want = value(round, 0, i);
            for (let o = 1; o < count; o++) {
                want = combined(op, want, value(round, o, i));
            }
            if (!same(out[i], want)) {
                report.wrong++;
                break;
            }
        }
    }
    done.post(report);
}

// Two wait in round 0 of three and one at round 1's gate, which would fill
// round 0 if it went on to arrive. A call after a break must settle false.
@worker export async function waiter(barrier: Barrier, round: i32, waiting: SharedRegion, again: bool, done: Port<i32>): Promise<void> {
    waiting.atomicAdd(0, 1);
    let met = await barrier.arrive(round, 3);
    let later = false;
    if (again) {
        later = await barrier.arrive(round + 1, 3);
    }
    done.post((met ? 1 : 0) + (later ? 2 : 0));
}

// Alone it meets without parking, so only the call can see the stop that
// ended its sleep.
@worker export async function loner(barrier: Barrier, started: SharedRegion, done: Port<i32>): Promise<void> {
    started.atomicStore(0, 1);
    await Worker.sleep(60);
    let met = await barrier.arrive(0, 1);
    done.post(met ? 1 : 0);
}

export class Main extends Layout {
    context: Context? = null;
    barrier: Barrier = Barrier.create(kWorkers, kValues);
    reports: Port<Report> = Port.create<Report>();
    workers: Array<Worker> = new Array<Worker>();
    breaking: Barrier = Barrier.create(3);
    breakWaiting: SharedRegion = SharedRegion.create(4);
    breakDone: Port<i32> = Port.create<i32>();
    breakWaiters: Array<Worker> = new Array<Worker>();
    brokeAt: i32 = -1;
    breakSettled: i32 = 0;
    stopping: Barrier = Barrier.create(3);
    stopWaiting: SharedRegion = SharedRegion.create(4);
    stopDone: Port<i32> = Port.create<i32>();
    stopWaiters: Array<Worker> = new Array<Worker>();
    stoppedAt: i32 = -1;
    stopSettled: i32 = 0;
    lone: Barrier = Barrier.create(1);
    loneStarted: SharedRegion = SharedRegion.create(4);
    loneDone: Port<i32> = Port.create<i32>();
    loneWorker: Worker? = null;
    loneStopped: bool = false;
    frame: i32 = 0;

    override init(context: Context): bool {
        this.context = context;
        for (let i = 0; i < kWorkers; i++) {
            this.workers.push(Worker.spawn(part, i, this.barrier, this.reports));
        }
        for (let i = 0; i < 3; i++) {
            let round = i < 2 ? 0 : 1;
            this.breakWaiters.push(Worker.spawn(waiter, this.breaking, round, this.breakWaiting, true, this.breakDone));
            this.stopWaiters.push(Worker.spawn(waiter, this.stopping, round, this.stopWaiting, false, this.stopDone));
        }
        this.loneWorker = Worker.spawn(loner, this.lone, this.loneStarted, this.loneDone);
        this.checkReleased();
        return true;
    }

    async checkReleased(): Promise<void> {
        let gone = Barrier.create(1);
        gone.release();
        let met = await gone.arrive(0, 1);
        this.context!.log("barrier released settles " + (met ? "true" : "false"));
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        this.frame++;
        let self = this;
        this.reports.drain((report: Report): void => {
            context.log("barrier worker " + report.index.toString() + " met " + report.met.toString() +
                " rounds, " + report.wrong.toString() + " wrong");
        });
        // A few frames on, the waiters are parked.
        if (this.brokeAt < 0 && this.breakWaiting.atomicLoad(0) == 3) {
            this.brokeAt = this.frame + 3;
        }
        if (this.frame == this.brokeAt) {
            this.breaking.break();
        }
        this.breakDone.drain((settled: i32): void => {
            if (settled == 0 && ++self.breakSettled == 3) {
                context.log("barrier break settled 3 waits, 1 at a gate, and 3 later calls false");
            }
        });
        if (this.stoppedAt < 0 && this.stopWaiting.atomicLoad(0) == 3) {
            this.stoppedAt = this.frame + 3;
        }
        if (this.frame == this.stoppedAt) {
            for (let i = 0; i < this.stopWaiters.length; i++) {
                this.stopWaiters[i].stop();
            }
        }
        this.stopDone.drain((settled: i32): void => {
            if (settled == 0 && ++self.stopSettled == 3) {
                context.log("barrier stop settled 3 waits, 1 at a gate, false");
            }
        });
        if (!this.loneStopped && this.loneStarted.atomicLoad(0) == 1) {
            this.loneStopped = true;
            this.loneWorker!.stop();
        }
        this.loneDone.drain((met: i32): void => {
            context.log("barrier stop settles a lone participant " + (met == 1 ? "true" : "false"));
        });
        return true;
    }
}
