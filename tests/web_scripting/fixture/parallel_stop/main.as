import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { Parallel, Part, SharedArray } from "rive/parallel";

@parallel export function spin(part: Part, out: SharedArray<i32>): void {
    let x = part.index;
    while (x >= 0) {
        x = (x + 1) & 0xffff;
    }
    out[0] = x;
}

@parallel export function fill(part: Part, out: SharedArray<i32>): void {
    let end = part.end(out.length);
    for (let i = part.start(out.length); i < end; i++) {
        out[i] = i;
    }
}

// Every part spins until the call's budget stops them all, which ends this
// layout's advances but not its draws, and the next call starts clean.
export class Main extends Layout {
    stopped: bool = false;
    checked: bool = false;
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        this.stopped = true;
        Parallel.run(spin, new SharedArray<i32>(1));
        return true;
    }

    override draw(renderer: Renderer): void {
        if (!this.stopped || this.checked) {
            return;
        }
        this.checked = true;
        let out = new SharedArray<i32>(100);
        Parallel.run(fill, out);
        this.context!.log("parallel after a stop " + (out[99] == 99 ? "ok" : "wrong"));
    }
}
