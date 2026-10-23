import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";
import { Lanes, LaneState } from "rive/lanes";
import { SharedRegion } from "rive/shared";

// Ticks on a timer and never checks for kWaitStopping, so a stop must end it
// instead of waking it forever.
@worker export async function ticker(region: SharedRegion): Promise<void> {
    while (true) {
        await Worker.sleep(0.002);
        region.atomicAdd(0, 1);
    }
}

// Yields to the host each pass, so it is always due.
@worker export async function yielder(region: SharedRegion): Promise<void> {
    while (!Lanes.stopping()) {
        await Worker.sleep(0);
        region.atomicAdd(4, 1);
    }
}

export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(8);
    ticks: Worker? = null;
    yields: Worker? = null;
    stopped: bool = false;
    done: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.ticks = Worker.spawn(ticker, this.region);
        this.yields = Worker.spawn(yielder, this.region);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let ticks = this.ticks!;
        let yields = this.yields!;
        if (!this.stopped && this.region.atomicLoad(0) >= 3 && this.region.atomicLoad(4) >= 3) {
            this.stopped = true;
            ticks.stop();
            yields.stop();
        }
        if (this.stopped && !this.done && !ticks.running && !yields.running) {
            this.done = true;
            context.log("ticker " + (ticks.state == LaneState.stopped ? "stopped" : "other") +
                " yielder " + (yields.state == LaneState.done ? "finished" : "other"));
            context.log("worker stops done");
        }
        return true;
    }
}
