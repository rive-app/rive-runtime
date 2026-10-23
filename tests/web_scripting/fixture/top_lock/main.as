import { Layout, Context } from "rive/host";
import { Jobs } from "rive/jobs";
import { SharedRegion, Signal } from "rive/shared";

// Holds the region until the top level lets go. Only a threaded page runs it
// while the top level waits.
@job export function holds(region: SharedRegion, flag: SharedRegion, held: Signal): i32 {
    region.exclusive((view): void => {
        held.notify();
        while (flag.atomicLoad(0) == 0) {}
    });
    return 1;
}

let region = SharedRegion.create(16);
let flag = SharedRegion.create(16);
let held = Signal.create();
let job = Jobs.run(holds, region, flag, held);
// The page's thread waits a tenth of a second at most, so a late pool thread
// gets a second wait, and one of the locks ends with the top level's budget.
let holding = job.running && (held.waitWithin(0, 0.1) > 0 || held.waitWithin(0, 0.1) > 0);
let got = region.tryExclusive((view): void => {}, 30) || region.tryExclusive((view): void => {}, 30);
flag.atomicStore(0, 1);
// Enough turns to meet a fuel check.
let turns = 0;
for (let i = 0; i < 2000; i++) {
    turns++;
}

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("top level lock " + (!holding ? "never held" : got ? "taken" : "timed out") +
            " turns " + turns.toString());
        return true;
    }
}
