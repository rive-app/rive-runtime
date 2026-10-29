import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { clock } from "rive/os";
import {
    Parallel,
    Part,
    SharedArray,
    atomicAdd,
    atomicLoad,
    compareExchange,
} from "rive/parallel";

struct Velocity {
    x: f32 = 0;
    y: f32 = 0;
    constructor(x: f32, y: f32) {
        this.x = x;
        this.y = y;
    }
}

export class Body {
    x: f32 = 0;
    v: f32 = 0;
    constructor(x: f32, v: f32) {
        this.x = x;
        this.v = v;
    }
}

let threshold: f32 = 0;
const kTable: StaticArray<i32> = [0, 4, 0, 4];

@parallel export function fill(part: Part, out: SharedArray<i32>, salt: i32): void {
    let end = part.end(out.length);
    for (let i = part.start(out.length); i < end; i++) {
        out[i] = i * 3 + salt;
    }
}

@parallel export function wrong(part: Part, out: SharedArray<i32>, salt: i32, bad: SharedArray<i32>): void {
    let count = 0;
    let end = part.end(out.length);
    for (let i = part.start(out.length); i < end; i++) {
        if (out[i] != i * 3 + salt) {
            count++;
        }
    }
    atomicAdd(bad, 0, count);
}

@parallel export function integrate(part: Part, bodies: Array<Body>, out: SharedArray<Velocity>, dt: f32): void {
    let end = part.end(bodies.length);
    for (let i = part.start(bodies.length); i < end; i++) {
        let b = bodies[i];
        out[i] = Velocity(b.x + b.v * dt, <f32>i);
    }
}

// Reads a global and a constant table as the call found them.
@parallel export function above(part: Part, out: SharedArray<i32>): void {
    out[part.index] = <f32>part.index > threshold ? kTable[1] : 0;
}

@parallel.inplace export function nudge(part: Part, bodies: Array<Body>, by: f32): void {
    let end = part.end(bodies.length);
    for (let i = part.start(bodies.length); i < end; i++) {
        bodies[i].x += by;
    }
}

// Each part counts its parts, then after the sync adds up every part's.
@parallel export function counts(part: Part, seen: SharedArray<i32>, sums: SharedArray<i32>): void {
    seen[part.index] = part.count;
    part.sync();
    let sum = 0;
    for (let i = 0; i < part.count; i++) {
        sum += seen[i];
    }
    sums[part.index] = sum;
}

@parallel export function tally(part: Part, seen: SharedArray<i32>, total: SharedArray<i32>): void {
    seen[part.index] = part.count;
    atomicAdd(total, 0, part.index + 1);
}

// Every index is claimed once in each of two stages.
@parallel export function claims(part: Part, hits: SharedArray<i32>): void {
    let n = hits.length;
    for (let stage = 0; stage < 2; stage++) {
        for (let i = part.claim(n, 7); i < n; i = part.claim(n, 7)) {
            let end = min(i + 7, n);
            for (let j = i; j < end; j++) {
                atomicAdd(hits, j, 1);
            }
        }
        part.sync();
    }
}

@parallel export function exchange(part: Part, owner: SharedArray<i64>): void {
    compareExchange(owner, 0, <i64>-1, <i64>part.index);
}

@parallel export function timed(part: Part, spans: SharedArray<f64>): void {
    let start = clock();
    let x: f64 = 0;
    for (let i = 0; i < 10000; i++) {
        x += Math.sqrt(<f64>i);
    }
    spans[part.index] = x > 0 ? clock() - start : -1;
}

@parallel export function throwsOnLast(part: Part, out: SharedArray<i32>): void {
    if (part.index == part.count - 1) {
        throw new Error("the last part gave up");
    }
    out[part.index] = 1;
}

export class Main extends Layout {
    frames: i32 = 0;
    checked: bool = false;
    kept: Array<SharedArray<i32>> = [];
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        let size = Parallel.size();
        context.log("parallel size " + size.toString());
        context.log("parallel threads " + (size > 1 ? "yes" : "no"));
        return true;
    }

    // As Box3D does, the first parallel call comes in the first step. The
    // second fails, which ends this layout's advances but not its draws.
    override advance(seconds: f64): bool {
        this.frames++;
        if (this.frames == 1) {
            this.firstStep(this.context!);
        } else if (this.frames == 2) {
            Parallel.runParts(4, throwsOnLast, new SharedArray<i32>(4));
        }
        return true;
    }

    // The call after a failed one starts clean.
    override draw(renderer: Renderer): void {
        if (this.frames < 2 || this.checked) {
            return;
        }
        this.checked = true;
        let out = new SharedArray<i32>(100);
        Parallel.run(fill, out, 1);
        this.context!.log("parallel after a throw " + (out[99] == 298 ? "ok" : "wrong"));
    }

    firstStep(context: Context): void {
        let size = Parallel.size();
        let out = new SharedArray<i32>(1000);
        Parallel.run(fill, out, 5);
        let bad = new SharedArray<i32>(1);
        Parallel.run(wrong, out, 5, bad);
        context.log("parallel fill wrong " + atomicLoad(bad, 0).toString());

        let bodies = new Array<Body>();
        for (let i = 0; i < 100; i++) bodies.push(new Body(<f32>i, 2));
        let velocities = new SharedArray<Velocity>(bodies.length);
        Parallel.run(integrate, bodies, velocities, 0.25);
        let structs = 0;
        for (let i = 0; i < velocities.length; i++) {
            let v = velocities[i];
            if (v.x != <f32>i + 0.5 || v.y != <f32>i) structs++;
        }
        context.log("parallel structs wrong " + structs.toString());
        Parallel.run(nudge, bodies, 3);
        context.log("parallel inplace " + (bodies[99].x == 102 && bodies[0].x == 3 ? "ok" : "wrong"));

        threshold = 1.5;
        let flags = new SharedArray<i32>(4);
        Parallel.runParts(4, above, flags);
        context.log("parallel globals " + (flags[1] == 0 && flags[2] == 4 && flags[3] == 4 ? "ok" : "wrong"));

        let seen = new SharedArray<i32>(64);
        let sums = new SharedArray<i32>(64);
        Parallel.run(counts, seen, sums);
        let parts = seen[0];
        context.log("parallel sync " + (sums[parts - 1] == parts * parts ? "ok" : "wrong"));
        context.log("parallel parts all " + (parts == size ? "yes" : "no"));
        Parallel.runUpTo(2, counts, seen, sums);
        let upTo = seen[0];
        context.log("parallel up to 2 " +
            (upTo <= 2 && sums[upTo - 1] == upTo * upTo ? "ok" : "wrong"));

        let total = new SharedArray<i32>(1);
        Parallel.runParts(13, tally, seen, total);
        context.log("parallel fixed parts " +
            (seen[12] == 13 && atomicLoad(total, 0) == 91 ? "ok" : "wrong"));

        let hits = new SharedArray<i32>(1001);
        Parallel.run(claims, hits);
        let missed = 0;
        for (let i = 0; i < hits.length; i++) {
            if (atomicLoad(hits, i) != 2) missed++;
        }
        context.log("parallel claims wrong " + missed.toString());

        let owner = new SharedArray<i64>(1);
        owner[0] = -1;
        Parallel.runParts(5, exchange, owner);
        context.log("parallel exchange " + (owner[0] >= 0 && owner[0] < 5 ? "ok" : "wrong"));

        let spans = new SharedArray<f64>(64);
        Parallel.run(timed, spans);
        let clocks = 0;
        for (let i = 0; i < parts; i++) {
            if (!(spans[i] > 0 && spans[i] < 10)) clocks++;
        }
        context.log("parallel clock wrong " + clocks.toString());

        // Half a MB a round, so the memory grows between calls, and the
        // first and the newest arrays still read back where they lie.
        let grown = 0;
        let pages = memory.size();
        for (let round = 0; round < 3; round++) {
            let big = new SharedArray<i32>(1 << 17);
            this.kept.push(big);
            Parallel.runParts(3, fill, big, round);
            bad[0] = 0;
            Parallel.run(wrong, this.kept[0], 0, bad);
            Parallel.run(wrong, big, round, bad);
            grown += atomicLoad(bad, 0);
        }
        context.log("parallel memory grew " + (memory.size() > pages ? "yes" : "no"));
        context.log("parallel grown wrong " + grown.toString());
    }
}
