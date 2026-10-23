import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { LaneState } from "rive/lanes";

// Never polls, so only a stop reaching its fuel checks ends it, or on the
// page's thread the page's call budget.
@job export function spin(seed: i32): i32 {
    let x = seed;
    while (x >= 0) {
        x = (x * 1103515245 + 12345) & 0x7fffffff;
    }
    return x;
}

@job export function square(x: f64): f64 {
    return x * x;
}

export class Main extends Layout {
    context: Context? = null;
    spinners: Array<Job<i32>> = new Array<Job<i32>>();
    frames: i32 = 0;
    after: Job<f64>? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        // One more than the pool has threads, and every thread holds one
        // before init returns, so even a release at once has them to end.
        for (let i: u32 = 0; i <= Jobs.threads(); i++) {
            this.spinners.push(Jobs.run(spin, <i32>i));
        }
        while (this.count(LaneState.running) < <i32>Jobs.threads()) {}
        return true;
    }

    count(state: LaneState): i32 {
        let found = 0;
        for (let i = 0; i < this.spinners.length; i++) {
            if (this.spinners[i].state == state) {
                found++;
            }
        }
        return found;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let spinners = this.spinners;
        if (++this.frames == 3) {
            for (let i = 0; i < spinners.length; i++) {
                spinners[i].stop();
            }
        }
        let after = this.after;
        if (after == null) {
            let over = 0;
            for (let i = 0; i < spinners.length; i++) {
                over += spinners[i].running ? 0 : 1;
            }
            if (this.frames < 3 || over < spinners.length) {
                return true;
            }
            for (let i = 0; i < spinners.length; i++) {
                let failed = spinners[i].failed;
                if (failed != null) {
                    context.log("spinner failed: " + failed);
                }
            }
            let stopped = this.count(LaneState.stopped) == spinners.length;
            context.log("spinners stopped " + (stopped ? "yes" : "no"));
            this.after = Jobs.run(square, 3.0);
        } else if (!this.reported && !after.running) {
            this.reported = true;
            context.log("job after the spinners " + after.peek()!.value.toString());
        }
        return true;
    }
}
