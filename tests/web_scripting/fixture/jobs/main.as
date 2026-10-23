import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";

export class Tile {
    name: string = "";
    sizes: Array<f32> = [];
    next: Tile? = null;
}

@job export function square(x: f64): f64 {
    return x * x;
}

@job export function build(name: string, count: i32): Tile {
    let tile = new Tile();
    tile.name = name;
    for (let i = 0; i < count; i++) {
        tile.sizes.push(<f32>i * 0.5);
    }
    let inner = new Tile();
    inner.name = name + "/inner";
    tile.next = inner;
    return tile;
}

@job export function fails(code: i32): i32 {
    if (code > 0) {
        throw new Error("bad code " + code.toString());
    }
    return code;
}

@job export function speaks(n: i32): i32 {
    console.log("job says " + n.toString());
    return n;
}

@job export function collatz(seed: i32): i32 {
    let n = <i64>seed;
    let steps = 0;
    while (n != 1) {
        n = (n & 1) == 0 ? n / 2 : 3 * n + 1;
        steps++;
    }
    return steps;
}

function hostCollatz(seed: i32): i32 {
    let n = <i64>seed;
    let steps = 0;
    while (n != 1) {
        n = (n & 1) == 0 ? n / 2 : 3 * n + 1;
        steps++;
    }
    return steps;
}

export class Main extends Layout {
    context: Context? = null;
    squared: Job<f64>? = null;
    built: Job<Tile>? = null;
    failed: Job<i32>? = null;
    spoke: Job<i32>? = null;
    later: Job<f64>? = null;
    chatty: Job<i32>? = null;
    collatz: Array<Job<i32>> = new Array<Job<i32>>();
    reported: bool = false;
    matched: bool = false;

    override init(context: Context): bool {
        this.context = context;
        context.log("job threads " + Jobs.threads().toString());
        // A call with a budget never runs a job here, so without threads
        // these finish at a later advance.
        let squared = Jobs.run(square, 3.0);
        squared.join();
        this.squared = squared;
        let built = Jobs.run(build, "munich", 4);
        built.join();
        this.built = built;
        let failed = Jobs.run(fails, 3);
        failed.join();
        this.failed = failed;
        let spoke = Jobs.run(speaks, 42);
        spoke.join();
        this.spoke = spoke;
        this.reportJoined();
        let jobs = new Array<Job<i32>>();
        for (let i = 1; i <= 24; i++) {
            jobs.push(Jobs.run(collatz, i * 7919));
        }
        // A join that would overrun this call leaves its job for an advance.
        for (let i = 0; i < jobs.length; i++) {
            jobs[i].join();
        }
        this.collatz = jobs;
        this.later = Jobs.run(square, 12.0);
        // Nobody joins it, so its line arrives with the next advance.
        this.chatty = Jobs.run(speaks, 7);
        return true;
    }

    reportJoined(): void {
        let context = this.context!;
        let squared = this.squared;
        if (squared != null && !squared.running) {
            this.squared = null;
            let result = squared.peek()!;
            if (result.ok) {
                context.log("job square " + result.value.toString());
            }
        }
        let built = this.built;
        if (built != null && !built.running) {
            this.built = null;
            let result = built.peek()!;
            if (result.ok) {
                let tile = result.value;
                let inner = tile.next;
                context.log(
                    "job tile " + tile.name + " " + tile.sizes.length.toString() +
                        " " + tile.sizes[3].toString() + " " +
                        (inner != null ? inner.name : "none"),
                );
            }
        }
        let failed = this.failed;
        if (failed != null && !failed.running) {
            this.failed = null;
            let result = failed.peek()!;
            if (!result.ok) {
                context.log("job error " + result.error);
            }
        }
    }

    override advance(seconds: f64): bool {
        this.reportJoined();
        let later = this.later;
        let context = this.context;
        if (!this.reported && later != null && !later.running && context != null) {
            this.reported = true;
            context.log("unjoined job done " + later.peek()!.value.toString());
        }
        let collatz = this.collatz;
        let over = 0;
        let matches = 0;
        for (let i = 0; i < collatz.length; i++) {
            let job = collatz[i];
            if (job.running) {
                continue;
            }
            over++;
            let result = job.peek()!;
            if (result.ok && result.value == hostCollatz((i + 1) * 7919)) {
                matches++;
            }
        }
        if (!this.matched && collatz.length > 0 && over == collatz.length && context != null) {
            this.matched = true;
            context.log("job matches " + matches.toString());
        }
        return true;
    }
}
