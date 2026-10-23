import { Layout, Context } from "rive/host";
import { Job, Jobs } from "rive/jobs";

// Only the job uses relaxed SIMD, which an engine validating the main module
// misses.
@job export function fuse(a: f64): i32 {
    let fused = f32x4.relaxed_madd(f32x4.splat(<f32>a), f32x4.splat(3), f32x4.splat(1));
    return i32(f32x4.extract_lane(fused, 0));
}

export class Main extends Layout {
    context: Context? = null;
    fused: Job<i32>? = null;

    override init(context: Context): bool {
        this.context = context;
        // A call with a budget never runs a job here, so without threads it
        // finishes at a later advance.
        let fused = Jobs.run(fuse, 2.0);
        fused.join();
        this.fused = fused;
        this.report();
        return true;
    }

    report(): void {
        let fused = this.fused;
        if (fused != null && !fused.running) {
            this.fused = null;
            let result = fused.peek()!;
            if (result.ok) {
                this.context!.log("job relaxed lane " + result.value.toString());
            }
        }
    }

    override advance(seconds: f64): bool {
        this.report();
        return true;
    }
}
