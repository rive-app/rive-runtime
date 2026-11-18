import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { clock } from "rive/os";
import { Parallel, Part, SharedArray, atomicAdd, atomicLoad } from "rive/parallel";

// Part 0 waits for every other part to count in, without a sync, and writes
// 1 if they all had when its wait ended, or 2.
@parallel export function countsIn(part: Part, seen: SharedArray<i32>): void {
    if (part.index != 0) {
        atomicAdd(seen, 0, 1);
        return;
    }
    let others = part.count - 1;
    while (atomicLoad(seen, 0) != others && !part.stopping()) {}
    seen[1] = atomicLoad(seen, 0) == others ? 1 : 2;
}

// The last part throws while the others wait on a flag that nothing sets.
@parallel export function failsWhileOthersWait(part: Part, flag: SharedArray<i32>): void {
    if (part.index == part.count - 1) {
        let until = clock() + 0.01;
        while (clock() < until) {}
        throw new Error("the last part gave up while the others waited");
    }
    while (atomicLoad(flag, 0) == 0) {
        if (part.stopping()) {
            return;
        }
    }
}

// Each part writes 1, or 2 where it finds its call stopped.
@parallel export function readsStopping(part: Part, out: SharedArray<i32>): void {
    out[part.index] = part.stopping() ? 2 : 1;
}

// The second frame's call fails, which ends this layout's advances but not
// its draws.
export class Main extends Layout {
    frames: i32 = 0;
    checked: bool = false;
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        this.frames++;
        if (this.frames == 1) {
            let seen = new SharedArray<i32>(2);
            Parallel.run(countsIn, seen);
            this.context!.log("parallel counted in " + (seen[1] == 1 ? "ok" : "wrong"));
        } else if (this.frames == 2) {
            Parallel.run(failsWhileOthersWait, new SharedArray<i32>(1));
        }
        return true;
    }

    // The call after a stopped one starts unstopped.
    override draw(renderer: Renderer): void {
        if (this.frames < 2 || this.checked) {
            return;
        }
        this.checked = true;
        let out = new SharedArray<i32>(64);
        Parallel.run(readsStopping, out);
        let stopped = 0;
        for (let i = 0; i < out.length; i++) {
            stopped += out[i] == 2 ? 1 : 0;
        }
        this.context!.log("parallel after a stopped call " + (out[0] == 1 && stopped == 0 ? "ok" : "wrong"));
    }
}
