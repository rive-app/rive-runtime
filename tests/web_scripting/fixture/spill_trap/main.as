import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";

struct V {
    x: i32 = 0;
}

function dive(n: i32): i32 {
    let v = V();
    v.x = n;
    ref r = v;
    if (n == 0) {
        return r.x;
    }
    return dive(n - 1) + r.x - n;
}

export class Main extends Layout {
    frames: i32 = 0;
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    // Draw runs every frame, after one that trapped too.
    override draw(renderer: Renderer): void {
        this.frames++;
        if (this.frames == 1) {
            // Past what the spill zone holds, so its floor check traps.
            dive(4000);
        }
        let context = this.context;
        if (context != null) {
            context.log("spill ok on frame " + this.frames.toString() + ": " +
                dive(10).toString());
        }
    }
}
