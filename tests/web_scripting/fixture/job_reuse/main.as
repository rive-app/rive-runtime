import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";
import { Lanes, LaneState } from "rive/lanes";

struct Mark {
    hits: i32 = 0;
}

class Tally {
    static total: i32 = 0;
}

let counter: i32 = 0;
let names: Array<string> = [];
let mark: Mark = Mark();
const table: StaticArray<i32> = [1, 2, 3, 4];

// A job may run in the instance an earlier job of its module finished in, so
// each one here changes a global, a static, static data and the heap.
function change(): void {
    counter += 1;
    Tally.total += 2;
    names.push("kept");
    mark.hits += 3;
    table[0] += 4;
}

// Answers how much of that it found changed, 0 as in a fresh instance, and
// its memory's pages before it grows them by about kilobytes.
@job export function touch(kilobytes: i32): Array<i32> {
    let found: Array<i32> = [
        counter + Tally.total + names.length + mark.hits + table[0] - 1,
        memory.size(),
    ];
    change();
    let grown = new Uint8Array(kilobytes * 1024);
    grown[grown.length - 1] = 1;
    return found;
}

@job export function trips(code: i32): i32 {
    change();
    if (code > 0) {
        throw new Error("tripped " + code.toString());
    }
    return code;
}

@job export function spins(seed: i32): i32 {
    change();
    let x = seed;
    while (!Lanes.stopping()) {
        x = (x * 1103515245 + 12345) & 0x7fffffff;
    }
    return x;
}

const ROUNDS: i32 = 32;
// Past what a job's memory may have grown to for another job to reuse it.
const HUNGRY_KILOBYTES: i32 = 20 * 1024;

function yes(ok: bool): string {
    return ok ? "yes" : "no";
}

export class Main extends Layout {
    context: Context? = null;
    phase: i32 = 0;
    round: i32 = 0;
    pending: Job<Array<i32>>? = null;
    other: Job<i32>? = null;
    stopped: bool = false;
    fresh: i32 = 0;
    least: i32 = i32.MAX_VALUE;
    most: i32 = 0;
    hungrySeen: bool = false;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    // One touch job over as many advances as it takes: null until it ends.
    touched(kilobytes: i32): Array<i32>? {
        let job = this.pending;
        if (job == null) {
            this.pending = Jobs.run(touch, kilobytes);
            return null;
        }
        if (job.running) {
            return null;
        }
        this.pending = null;
        let result = job.peek()!;
        return result.ok ? result.value : [-1, -1];
    }

    // Whether a touch started in an instance of its own, none before it.
    anew(found: Array<i32>): string {
        return yes(found[1] == this.least);
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        if (this.phase == 0) {
            let found = this.touched(256);
            if (found == null) {
                return true;
            }
            this.fresh += found[0] == 0 ? 1 : 0;
            this.least = min(this.least, found[1]);
            this.most = max(this.most, found[1]);
            if (++this.round == ROUNDS) {
                context.log("reuse fresh state " + this.fresh.toString() + " of " + ROUNDS.toString());
                context.log("reuse kept an instance " + yes(this.most > this.least));
                this.phase = 1;
            }
        } else if (this.phase == 1) {
            let tripped = this.other;
            if (tripped == null) {
                this.other = Jobs.run(trips, 3);
                return true;
            }
            if (tripped.running) {
                return true;
            }
            let found = this.touched(64);
            if (found == null) {
                return true;
            }
            context.log("reuse after a throw fresh " + yes(found[0] == 0) + ": " + tripped.peek()!.error);
            context.log("reuse after a throw started anew " + this.anew(found));
            this.other = null;
            this.phase = 2;
        } else if (this.phase == 2) {
            let spinner = this.other;
            if (spinner == null) {
                spinner = Jobs.run(spins, 7);
                this.other = spinner;
            }
            if (!this.stopped) {
                // Without threads it would run at an advance and never end.
                if (Jobs.threads() > 0 && spinner.state != LaneState.running) {
                    return true;
                }
                spinner.stop();
                this.stopped = true;
            }
            if (spinner.running) {
                return true;
            }
            let found = this.touched(64);
            if (found == null) {
                return true;
            }
            context.log("reuse after a stop fresh " + yes(found[0] == 0));
            if (Jobs.threads() > 0) {
                context.log("reuse after a stop started anew " + this.anew(found));
            }
            this.other = null;
            this.phase = 3;
            this.round = 0;
        } else if (this.phase == 3) {
            let found = this.touched(this.round == 0 ? HUNGRY_KILOBYTES : 64);
            if (found == null) {
                return true;
            }
            this.hungrySeen = this.hungrySeen || found[1] >= HUNGRY_KILOBYTES / 64;
            if (++this.round == 9) {
                context.log("reuse dropped the hungry instance " + yes(!this.hungrySeen));
                context.log("reuse done");
                this.phase = 4;
            }
        }
        return true;
    }
}
