import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { Lanes, LaneState } from "rive/lanes";
import { clock } from "rive/os";

// Runs until its caller cancels it, polling as a cooperative job does. Only a
// threaded page runs it: on one thread its joiner would run it inline.
@job export function spin(seed: i32): i32 {
    let x = seed;
    while (!Lanes.stopping()) {
        x = (x * 1103515245 + 12345) & 0x7fffffff;
    }
    return x;
}

export class Main extends Layout {
    context: Context? = null;
    spinner: Job<i32>? = null;
    cancelled: bool = false;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.spinner = Jobs.run(spin, 7);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let spinner = this.spinner!;
        if (!this.cancelled && spinner.state == LaneState.running) {
            let began = clock();
            let joined = spinner.join();
            let waited = clock() - began;
            context.log(
                "long join " + (joined == null ? "ended" : "settled") +
                    " within a second " + (waited < 1 ? "yes" : "no"),
            );
            spinner.stop();
            this.cancelled = true;
        }
        if (this.cancelled && !this.reported && !spinner.running) {
            this.reported = true;
            context.log(
                "spinner " + (spinner.state == LaneState.stopped ? "cancelled" : "not cancelled") +
                    ": " + spinner.peek()!.error,
            );
        }
        return true;
    }
}
