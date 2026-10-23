import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";
import { SharedRegion } from "rive/shared";
import { clock } from "rive/os";

// Never awaits again after its first wake, so on the web only its fuel
// checks end it, past its stop's grace or the page's call budget.
@worker export async function spinner(region: SharedRegion): Promise<void> {
    await Worker.sleep(0.002);
    region.atomicStore(0, 1);
    let x = 0;
    while (x >= 0) {
        x = (x + 1) & 0xffff;
    }
}

export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(8);
    spins: Worker? = null;
    stoppedAt: f64 = -1;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.spins = Worker.spawn(spinner, this.region);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let spins = this.spins!;
        if (this.stoppedAt < 0 && this.region.atomicLoad(0) == 1) {
            this.stoppedAt = clock();
            spins.stop();
            context.log("spinner stop sent");
        } else if (this.stoppedAt >= 0 && !this.reported && clock() - this.stoppedAt >= 1) {
            this.reported = true;
            context.log("spinner running after its stop " + (spins.running ? "yes" : "no"));
        }
        return true;
    }
}
