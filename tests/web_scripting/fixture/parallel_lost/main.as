import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { Parallel, Part, SharedArray, atomicAdd } from "rive/parallel";

@parallel export function tally(part: Part, seen: SharedArray<i32>, total: SharedArray<i32>): void {
    seen[part.index] = part.count;
    atomicAdd(total, 0, 1);
}

// Run with a team thread that never takes its part: the first call fails,
// which ends this layout's advances, and the call after it runs without
// that thread.
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
            Parallel.run(tally, new SharedArray<i32>(64), new SharedArray<i32>(1));
        }
        return true;
    }

    override draw(renderer: Renderer): void {
        if (this.frames < 1 || this.checked) {
            return;
        }
        this.checked = true;
        let seen = new SharedArray<i32>(64);
        let total = new SharedArray<i32>(1);
        Parallel.run(tally, seen, total);
        this.context!.log("parallel after a lost thread parts " + seen[0].toString() +
            " total " + total[0].toString());
    }
}
