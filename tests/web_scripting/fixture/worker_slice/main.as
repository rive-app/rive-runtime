import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";
import { Lanes } from "rive/lanes";
import { SharedRegion, Signal } from "rive/shared";

// Yields to the host each pass, so it is due again at once.
@worker export async function yielder(region: SharedRegion): Promise<void> {
    while (!Lanes.stopping()) {
        await Worker.sleep(0);
        region.atomicAdd(0, 1);
    }
}

// Parks on a signal nobody notifies, so its host may settle.
@worker export async function idler(wake: Signal): Promise<void> {
    await wake.next(0.0);
}

export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(8);
    wake: Signal = Signal.create();
    yields: Worker? = null;
    idles: Worker? = null;
    lastRounds: u32 = 0;
    stopped: bool = false;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.yields = Worker.spawn(yielder, this.region);
        this.idles = Worker.spawn(idler, this.wake);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let yields = this.yields!;
        // Stepped once per advance, it would never run two rounds between.
        let rounds = this.region.atomicLoad(0);
        if (!this.stopped && rounds - this.lastRounds > 1) {
            this.stopped = true;
            context.log("yielder rounds past one per advance yes");
            yields.stop();
        }
        this.lastRounds = rounds;
        if (this.stopped && !this.done && !yields.running) {
            this.done = true;
            context.log("worker slice done");
        }
        return true;
    }
}
