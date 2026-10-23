import { Layout, Context } from "rive/host";
import { Jobs } from "rive/jobs";
import { RtNative } from "rive/natives";
import { SharedRegion } from "rive/shared";

@job export function square(x: f64): f64 {
    return x * x;
}

// Module start has no VM on WAMR, so an op like this one resolves nothing.
@external("rive_path_v1", "new")
declare function pathNew(): u32;

unsafe function boot(): i32 {
    RtNative.log(0, "top level log");
    return 1;
}
let booted: i32 = boot();
let topLevelPath: u32 = pathNew();
// Shared memory and lanes see the VM there, so a top level may use them.
let topRegion = SharedRegion.create(8);
// A join here gives up with the top level's budget and never runs its job,
// so a result is read once the job is done.
let topJob = Jobs.run(square, 3.0);
let ranHere = Jobs.run(square, 4.0).join() != null && Jobs.threads() == 0;

export class Main extends Layout {
    context: Context? = null;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        context.log("init after top level");
        context.log("top level path handle " + topLevelPath.toString());
        context.log("top level join ran its job here " + (ranHere ? "yes" : "no"));
        return booted == 1;
    }

    override advance(seconds: f64): bool {
        let context = this.context;
        if (!this.reported && context != null && !topJob.running) {
            this.reported = true;
            let result = topJob.peek();
            let squared = result != null && result.ok ? result.value.toString() : "none";
            context.log("top level job " + squared + " region " + topRegion.size.toString());
        }
        return true;
    }
}
