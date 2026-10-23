import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { Worker } from "rive/workers";
import { Lanes } from "rive/lanes";
import { Promise, Settled } from "rive/promise";
import { Port } from "rive/ports";
import { Blob } from "rive/shared";

export class Request {
    a: f64 = 0;
    b: f64 = 0;
    // Where the answer goes; without one it goes to the service's replies.
    reply: Port<f64>? = null;
}

// A service: answers each request with its sum until it is stopped.
@worker export async function adder(requests: Port<Request>, replies: Port<f64>): Promise<i32> {
    let answered = 0;
    while (true) {
        let request = await requests.receive();
        if (request == null) {
            return answered;
        }
        let asked = request.value;
        let to = asked.reply;
        if (to != null) {
            to.post(asked.a + asked.b);
            to.release();
        } else {
            replies.post(asked.a + asked.b);
        }
        answered++;
    }
}

// Posts count values into a port too small for them, waiting for room as
// the main lane drains it; answers how many posts were refused.
@worker export async function filler(out: Port<f64>, count: i32): Promise<i32> {
    let refused = 0;
    for (let i = 0; i < count; i++) {
        while (!out.post(<f64>i)) {
            refused++;
            let room = await out.writable();
            if (!room) {
                return -1;
            }
        }
    }
    return refused;
}

// Sums the bytes of the first blob it is sent.
@worker export async function summer(blobs: Port<Blob>, sums: Port<f64>): Promise<void> {
    let got = await blobs.receive();
    if (got == null) {
        return;
    }
    let bytes = Uint8Array.wrap(got.value.toArrayBuffer());
    let sum = 0.0;
    for (let i = 0; i < bytes.length; i++) {
        sum += <f64>bytes[i];
    }
    sums.post(sum);
    got.value.release();
}

@job export function counting(count: i32): Blob {
    let bytes = new Uint8Array(count);
    for (let i = 0; i < count; i++) {
        bytes[i] = <u8>i;
    }
    return Blob.from(bytes.buffer);
}

@job export function spin(): i32 {
    let x = 0;
    while (!Lanes.stopping()) {
        x = (x + 1) & 0xffff;
    }
    return x;
}

function request(a: f64, b: f64, reply: Port<f64>?): Request {
    let made = new Request();
    made.a = a;
    made.b = b;
    made.reply = reply;
    return made;
}

async function awaitReply(context: Context, requests: Port<Request>): Promise<void> {
    let reply = Port.create<f64>(64);
    requests.post(request(10, 20, reply));
    let got = await reply.receive();
    context.log("port awaited " + (got != null ? got.value.toString() : "nothing"));
}

const kChecks: i32 = 7;
// How many advances ask for an answer within the call before giving up.
const kAsks: i32 = 20;

// The main lane talks to workers over ports: it posts, awaits replies at
// the poll and drains them in advance.
export class Main extends Layout {
    context: Context? = null;
    requests: Port<Request> = Port.create<Request>();
    replies: Port<f64> = Port.create<f64>();
    filled: Port<f64> = Port.create<f64>(64);
    sums: Port<f64> = Port.create<f64>();
    service: Worker<i32>? = null;
    // Its own service, so the asks leave the first one's count alone.
    asked: Port<Request> = Port.create<Request>();
    asker: Worker<i32>? = null;
    asks: i32 = 0;
    filler: Worker<i32>? = null;
    summer: Worker? = null;
    counted: Job<Blob>? = null;
    blobs: Port<Blob>? = null;
    drained: i32 = 0;
    total: f64 = 0;
    fills: i32 = 0;
    fillTotal: f64 = 0;
    frames: i32 = 0;
    checks: i32 = 0;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        let self = this;
        this.service = Worker.spawn(adder, this.requests, this.replies);
        for (let i = 0; i < 3; i++) {
            this.requests.post(request(<f64>(i * 2 + 1), <f64>(i * 2 + 2), null));
        }
        awaitReply(context, this.requests).andThen((): void => {
            self.checks++;
        });
        this.asker = Worker.spawn(adder, this.asked, Port.create<f64>());

        let small = Port.create<f64>(32);
        let first = small.post(1);
        let second = small.post(2);
        let third = small.post(3);
        context.log("port full refused " + (first && second && !third ? "yes" : "no"));
        this.checks++;

        this.filler = Worker.spawn(filler, this.filled, 20);

        // The job's blob crosses to a worker after the job and the main lane
        // let go of it.
        let blobs = Port.create<Blob>();
        this.summer = Worker.spawn(summer, blobs, this.sums);
        this.blobs = blobs;
        // A call with a budget never runs a job here, so without threads it
        // finishes at a later advance.
        let job = Jobs.run(counting, 200);
        job.join();
        this.counted = job;
        this.postCounted();

        let dropped = Jobs.run(spin);
        dropped.stop();
        dropped.promise().settled().andThen((outcome: Settled<i32>): void => {
            context.log("cancelled job promise: " + outcome.error);
            self.checks++;
        });
        return true;
    }

    postCounted(): void {
        let job = this.counted;
        if (job == null || job.running) {
            return;
        }
        this.counted = null;
        let made = job.peek()!.value;
        job.release();
        let blobs = this.blobs!;
        blobs.post(made);
        made.release();
        blobs.release();
    }

    // Blocks for a worker's answer inside this advance. Without threads a
    // worker only steps between advances, so there it never comes.
    askWithin(): void {
        if (this.asks >= kAsks) {
            return;
        }
        let context = this.context!;
        let reply = Port.create<f64>(64);
        this.asked.post(request(3, 4, reply));
        let got = reply.receiveWithin(1);
        reply.release();
        this.asks++;
        if (got == null && this.asks < kAsks) {
            return;
        }
        this.asks = kAsks;
        context.log("port answered within the call " + (got != null ? got.value.toString() : "never"));
        this.asker!.stop();
        this.checks++;
    }

    override advance(seconds: f64): bool {
        this.postCounted();
        this.askWithin();
        let context = this.context!;
        let self = this;
        this.frames++;
        if (this.drained < 3) {
            this.replies.drain((sum: f64): void => {
                self.drained++;
                self.total += sum;
            });
            if (this.drained == 3) {
                context.log("port drained 3 total " + this.total.toString());
                this.checks++;
            }
        }
        // A few values each advance, so the filler runs out of room.
        if (this.fills < 20 && this.frames % 2 == 0) {
            this.filled.drain((value: f64): void => {
                self.fills++;
                self.fillTotal += value;
            }, 3);
        }
        let filler = this.filler!;
        if (this.fills == 20 && !filler.running) {
            this.fills++;
            let refused = filler.peek()!;
            context.log("port filled 20 total " + this.fillTotal.toString() +
                " refused some " + (refused.ok && refused.value > 0 ? "yes" : "no"));
            this.checks++;
        }
        this.sums.drain((sum: f64): void => {
            context.log("port blob sum " + sum.toString());
            self.checks++;
        });
        if (!this.done && this.checks == kChecks) {
            this.done = true;
            let service = this.service!;
            service.stop();
            service.promise().andThen((answered: i32): void => {
                context.log("port service answered " + answered.toString());
                context.log("ports done");
            });
        }
        return true;
    }
}
