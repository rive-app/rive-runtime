import { Layout, Context } from "rive/host";
import { Parallel, Part, SharedArray } from "rive/parallel";

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

// A few calls each frame, with the page idle between frames, so the team
// parks after every frame and each frame's first call wakes it.
export class Main extends Layout {
    frames: i32 = 0;
    wrong: i32 = 0;
    seen: SharedArray<i32> = new SharedArray<i32>(64);
    sums: SharedArray<i32> = new SharedArray<i32>(64);
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        if (this.frames >= 600) {
            return true;
        }
        this.frames++;
        for (let call = 0; call < 3; call++) {
            Parallel.run(counts, this.seen, this.sums);
            let parts = this.seen[0];
            if (this.sums[parts - 1] != parts * parts) {
                this.wrong++;
            }
        }
        if (this.frames % 200 == 0) {
            this.context!.log("frames " + this.frames.toString() + " parts " +
                this.seen[0].toString() + " wrong " + this.wrong.toString());
        }
        return true;
    }
}
