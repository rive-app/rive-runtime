import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { RegionView, Ring, SharedRegion, Snapshot } from "rive/shared";

export class Point {
    x: f64 = 0;
    y: f64 = 0;
}

@job export function bumpAtomic(region: SharedRegion, times: i32): i32 {
    for (let i = 0; i < times; i++) {
        region.atomicAdd(0, 1);
    }
    return times;
}

@job export function bumpLocked(region: SharedRegion, times: i32): i32 {
    for (let i = 0; i < times; i++) {
        region.exclusive((view: RegionView): void => {
            view.setU32(4, view.getU32(4) + 1);
        });
    }
    return times;
}

@job export function produce(ring: Ring<Point>, count: i32): i32 {
    for (let i = 0; i < count; i++) {
        let p = new Point();
        p.x = <f64>i;
        ring.send(p);
    }
    return count;
}

@job export function publishes(snapshot: Snapshot<Point>, count: i32): i32 {
    for (let i = 0; i < count; i++) {
        let p = new Point();
        p.x = <f64>i;
        snapshot.publish(p);
    }
    return count;
}

@job export function trapsHolding(region: SharedRegion): i32 {
    region.exclusive((view: RegionView): void => {
        throw new Error("trap while holding");
    });
    return 0;
}

// Each step polls its jobs across advances rather than joining them, so no
// one call waits on many jobs within its budget.
export class Main extends Layout {
    context: Context? = null;
    region: SharedRegion = SharedRegion.create(16);
    ring: Ring<Point> = Ring.create<Point>(4096);
    snapshot: Snapshot<Point> = Snapshot.create<Point>();
    poisoned: SharedRegion = SharedRegion.create(8);
    jobs: Array<Job<i32>> = new Array<Job<i32>>();
    step: i32 = 0;

    override init(context: Context): bool {
        this.context = context;
        for (let i = 0; i < 8; i++) {
            this.jobs.push(Jobs.run(bumpAtomic, this.region, 100));
            this.jobs.push(Jobs.run(bumpLocked, this.region, 50));
        }
        // A handle rebuilt from an id takes its own hold, and releasing it
        // inside exclusive still unlocks.
        let probe = SharedRegion.create(8);
        let rebuilt = new SharedRegion(probe.id);
        rebuilt.exclusive((view: RegionView): void => {
            rebuilt.release();
        });
        let unlocked = probe.tryExclusive((view: RegionView): void => {});
        context.log("shared rebuilt handle " + (probe.size == 8 && unlocked ? "held its own" : "took the creator's"));
        // The view reaches the held region after a release, value bounds too.
        let kept = SharedRegion.create(64);
        let loaded = new Array<f64>();
        kept.exclusive((view: RegionView): void => {
            kept.release();
            let p = new Point();
            p.x = 7;
            view.storeValue<Point>(0, p);
            loaded.push(view.loadValue<Point>(0).x);
        });
        context.log("shared value after release " + loaded[0].toString());
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        for (let i = 0; i < this.jobs.length; i++) {
            if (this.jobs[i].running) {
                return true;
            }
        }
        let last = this.jobs.length > 0 ? this.jobs[this.jobs.length - 1].peek() : null;
        this.jobs = new Array<Job<i32>>();
        this.step++;
        if (this.step == 1) {
            let counts = new Array<u32>();
            this.region.exclusive((view: RegionView): void => {
                counts.push(view.getU32(0));
                counts.push(view.getU32(4));
            });
            context.log("shared atomic " + counts[0].toString() + " locked " + counts[1].toString());
            this.jobs.push(Jobs.run(produce, this.ring, 20));
        } else if (this.step == 2) {
            let sum = 0.0;
            let received = 0;
            let p = new Point();
            while (this.ring.receiveInto(p)) {
                sum += p.x;
                received++;
            }
            context.log("shared ring " + received.toString() + " sum " + sum.toString());
            this.jobs.push(Jobs.run(publishes, this.snapshot, 5));
        } else if (this.step == 3) {
            context.log("shared snapshot " + this.snapshot.latest()!.value.x.toString());
            this.jobs.push(Jobs.run(trapsHolding, this.poisoned));
        } else if (this.step == 4) {
            let result = last!;
            context.log("shared trap " + (result.ok ? "ok" : "failed") + " poisoned " + (this.poisoned.poisoned ? "yes" : "no"));
            this.poisoned.exclusive((view: RegionView): void => {});
            context.log("shared lock freed");
        }
        return true;
    }
}
