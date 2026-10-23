import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";
import { LaneState } from "rive/lanes";
import { RegionView, Ring, SharedRegion, Signal, Snapshot } from "rive/shared";
import { kWaitStopping, kWaitTimedOut } from "rive/lane_codes";

export class Reply {
    value: f64 = 0;
    count: i32 = 0;
}

// Answers each value it receives with its double and a running count.
@worker export async function echo(inbox: Ring<f64>, outbox: Ring<Reply>): Promise<void> {
    let wake = inbox.dataSignal;
    let seen = 0.0;
    let count = 0;
    while (true) {
        let got = await wake.next(seen);
        if (got == kWaitStopping) {
            return;
        }
        seen = got;
        while (true) {
            let value = inbox.receive();
            if (value == null) {
                break;
            }
            count++;
            let reply = new Reply();
            reply.value = value.value * 2;
            reply.count = count;
            outbox.send(reply);
        }
    }
}

@worker export async function sleeper(outbox: Ring<f64>): Promise<void> {
    let got = await Worker.sleep(0.01);
    outbox.send(got);
}

@worker export async function thrower(code: i32): Promise<void> {
    await Worker.sleep(0.001);
    throw new Error("worker code " + code.toString());
}

@worker export async function adder(region: SharedRegion, wake: Signal): Promise<void> {
    let seen = 0.0;
    while (true) {
        let got = await wake.next(seen, 1.0);
        if (got == kWaitStopping) {
            return;
        }
        if (got != kWaitTimedOut) {
            seen = got;
            region.exclusive((view: RegionView): void => {
                view.setU32(0, view.getU32(0) + 1);
            });
            region.atomicAdd(4, 1);
        }
    }
}

@worker export async function publisher(snapshot: Snapshot<Reply>, count: i32): Promise<void> {
    for (let i = 1; i <= count; i++) {
        let reply = new Reply();
        reply.count = i;
        snapshot.publish(reply);
        await Worker.sleep(0.0005);
    }
}

// Spawns workers at init and watches them each frame: the main lane never
// waits on a worker, so a single threaded page steps them between frames.
export class Main extends Layout {
    context: Context? = null;
    inbox: Ring<f64> = Ring.create<f64>(4096);
    outbox: Ring<Reply> = Ring.create<Reply>(4096);
    echoes: Worker? = null;
    replies: i32 = 0;
    lastReply: Reply = new Reply();
    echoStopped: bool = false;
    sleeps: Ring<f64> = Ring.create<f64>(64);
    // Held, since a worker whose wrapper is collected is stopped.
    sleepWorker: Worker? = null;
    slept: bool = false;
    throws: Worker? = null;
    threw: bool = false;
    region: SharedRegion = SharedRegion.create(8);
    wake: Signal = Signal.create();
    adders: Array<Worker> = new Array<Worker>();
    notified: i32 = 0;
    added: bool = false;
    snapshot: Snapshot<Reply> = Snapshot.create<Reply>();
    publishes: Worker? = null;
    published: bool = false;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.echoes = Worker.spawn(echo, this.inbox, this.outbox);
        for (let i = 1; i <= 10; i++) {
            this.inbox.send(<f64>i);
        }
        this.sleepWorker = Worker.spawn(sleeper, this.sleeps);
        this.throws = Worker.spawn(thrower, 7);
        for (let i = 0; i < 4; i++) {
            this.adders.push(Worker.spawn(adder, this.region, this.wake));
        }
        this.publishes = Worker.spawn(publisher, this.snapshot, 5);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        while (true) {
            let reply = this.outbox.receive();
            if (reply == null) {
                break;
            }
            this.replies++;
            this.lastReply = reply.value;
        }
        let echoes = this.echoes!;
        if (this.replies == 10 && echoes.running) {
            context.log("worker echo " + this.replies.toString() + " last " +
                this.lastReply.value.toString() + " count " + this.lastReply.count.toString());
            echoes.stop();
        }
        if (!this.echoStopped && this.replies == 10 && !echoes.running) {
            this.echoStopped = true;
            context.log("worker echo stopped " + (echoes.state == LaneState.done ? "finished" : "other"));
        }
        let slept = this.sleeps.receive();
        if (slept != null) {
            this.slept = true;
            context.log("worker sleep " + (slept.value == 0 ? "completed" : "woke"));
        }
        let throws = this.throws!;
        if (!this.threw && !throws.running) {
            this.threw = true;
            context.log("worker threw " + (throws.state == LaneState.failed ? "failed" : "other") +
                " " + (throws.failed!.indexOf("worker code 7") >= 0 ? "with code" : "without code"));
        }
        // Notify again each frame until every adder has counted a wake.
        if (!this.added) {
            if (this.region.atomicLoad(4) >= 4) {
                this.added = true;
                let locked = new Array<u32>();
                this.region.exclusive((view: RegionView): void => {
                    locked.push(view.getU32(0));
                });
                context.log("worker adders agree " + (locked[0] == this.region.atomicLoad(4) ? "yes" : "no"));
                for (let i = 0; i < this.adders.length; i++) {
                    this.adders[i].stop();
                }
            } else {
                this.wake.notify();
            }
        }
        let publishes = this.publishes!;
        if (!this.published && !publishes.running) {
            this.published = true;
            context.log("worker snapshot " + this.snapshot.latest()!.value.count.toString());
        }
        if (!this.done && this.echoStopped && this.slept && this.threw && this.added && this.published) {
            let stopped = 0;
            for (let i = 0; i < this.adders.length; i++) {
                stopped += this.adders[i].running ? 0 : 1;
            }
            if (stopped == this.adders.length) {
                this.done = true;
                context.log("workers done");
            }
        }
        return true;
    }
}
