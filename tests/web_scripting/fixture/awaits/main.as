import { Layout, Context } from "rive/host";
import { Jobs } from "rive/jobs";
import { Worker } from "rive/workers";
import { Promise, Settled } from "rive/promise";
import { Signal } from "rive/shared";
import { kWaitTimedOut } from "rive/lane_codes";

export class Reply {
    value: f64 = 0;
    label: string = "";
}

@job export function square(x: f64): f64 {
    return x * x;
}

@job export function fails(code: i32): i32 {
    if (code > 0) {
        throw new Error("bad code " + code.toString());
    }
    return code;
}

@worker export async function answer(n: i32): Promise<Reply> {
    await Worker.sleep(0.001);
    let reply = new Reply();
    reply.value = <f64>n * 1.5;
    reply.label = "answer " + n.toString();
    return reply;
}

@worker export async function quiet(): Promise<void> {
    await Worker.sleep(0.001);
}

@worker export async function thrower(code: i32): Promise<f64> {
    await Worker.sleep(0.001);
    throw new Error("worker code " + code.toString());
}

async function chained(context: Context): Promise<void> {
    let squared = await Jobs.run(square, 6.0).promise();
    let slept = await Worker.sleep(0.005);
    context.log("awaited square " + squared.toString() + " then slept " +
        (slept == 0 ? "yes" : "no"));
}

const kAwaits: i32 = 10;

// The main lane awaits jobs, workers, signals and sleeps without blocking:
// each promise settles on a later advance.
export class Main extends Layout {
    context: Context? = null;
    signal: Signal = Signal.create();
    answers: Worker<Reply>? = null;
    frames: i32 = 0;
    peeked: bool = false;
    // Awaits settled so far, of kAwaits.
    settled: i32 = 0;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        let self = this;
        Jobs.run(square, 4.0).promise().andThen((value: f64): void => {
            context.log("promise square " + value.toString());
            self.settled++;
        });
        let joined = Jobs.run(square, 5.0);
        joined.join();
        let late = joined.promise();
        context.log("joined promise " + late.getStatus());
        late.andThen((value: f64): void => {
            context.log("joined promise settled " + value.toString());
            self.settled++;
        });
        Jobs.run(fails, 3).promise().catch((error: string): void => {
            context.log("promise rejected " + error);
            self.settled++;
        });
        this.signal.next(0).andThen((seq: f64): void => {
            context.log("main signal " + seq.toString());
            self.settled++;
        });
        Signal.create().next(0, 0.01).andThen((seq: f64): void => {
            context.log("main signal timed out " + (seq == kWaitTimedOut ? "yes" : "no"));
            self.settled++;
        });
        Worker.sleep(0.01).andThen((value: f64): void => {
            context.log("main sleep " + (value == 0 ? "completed" : "early"));
            self.settled++;
        });
        chained(context).andThen((): void => {
            self.settled++;
        });
        let answers = Worker.spawn(answer, 4);
        this.answers = answers;
        answers.promise().andThen((reply: Reply): void => {
            context.log("worker result " + reply.label + " " + reply.value.toString());
            self.settled++;
        });
        Worker.spawn(quiet).promise().andThen((): void => {
            context.log("worker void done");
            self.settled++;
        });
        Worker.spawn(thrower, 7).promise().settled().andThen((outcome: Settled<f64>): void => {
            context.log("worker rejected " +
                (outcome.error.indexOf("worker code 7") >= 0 ? "with code" : outcome.error));
            self.settled++;
        });
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        this.frames++;
        if (this.frames == 3) {
            this.signal.notify();
        }
        let answers = this.answers!;
        if (!this.peeked && !answers.running) {
            this.peeked = true;
            let peek = answers.peek()!;
            context.log("worker peek " + (peek.ok ? peek.value.value.toString() : peek.error));
        }
        if (!this.done && this.peeked && this.settled == kAwaits) {
            this.done = true;
            context.log("awaits done");
        }
        return true;
    }
}
