import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";
import { SharedRegion, Signal } from "rive/shared";
import { kWaitStopping } from "rive/lane_codes";

// Region words: one tick count per member, then one spin order per member.
const kMembers: i32 = 6;
const kSpinners: i32 = 2;

// Counts each wake, unless told to spin, which it then does without ever
// awaiting again after saying so.
@worker export async function member(region: SharedRegion, wake: Signal, index: i32): Promise<void> {
    let tick = <u32>index * 4;
    let order = <u32>(kMembers + index) * 4;
    let seen = 0.0;
    while (true) {
        let got = await wake.next(seen);
        if (got == kWaitStopping) {
            return;
        }
        seen = got;
        if (region.atomicLoad(order) == 1) {
            region.atomicStore(order, 2);
            let x = 0;
            while (x >= 0) {
                x = (x + 1) & 0xffff;
            }
        }
        region.atomicAdd(tick, 1);
    }
}

// Every member steps once, so a page pins each to a runner, then two spin.
// More members than a page's runners leave some sharing a spinner's runner
// whatever the pins, and they must still progress.
export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(<u32>kMembers * 8);
    wake: Signal = Signal.create();
    members: Array<Worker> = new Array<Worker>();
    ordered: bool = false;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        for (let i = 0; i < kMembers; i++) {
            this.members.push(Worker.spawn(member, this.region, this.wake, i));
        }
        this.wake.notify();
        return true;
    }

    private ticks(index: i32): u32 {
        return this.region.atomicLoad(<u32>index * 4);
    }

    private spinning(index: i32): bool {
        return this.region.atomicLoad(<u32>(kMembers + index) * 4) == 2;
    }

    override advance(seconds: f64): bool {
        if (this.done) {
            return true;
        }
        if (!this.ordered) {
            for (let i = 0; i < kMembers; i++) {
                if (this.ticks(i) == 0) {
                    return true;
                }
            }
            this.ordered = true;
            for (let i = 0; i < kSpinners; i++) {
                this.region.atomicStore(<u32>(kMembers + i) * 4, 1);
            }
            this.wake.notify();
            return true;
        }
        for (let i = 0; i < kMembers; i++) {
            if (i < kSpinners ? !this.spinning(i) : this.ticks(i) < 2) {
                return true;
            }
        }
        this.done = true;
        this.context!.log("pinned workers progress yes");
        return true;
    }
}
