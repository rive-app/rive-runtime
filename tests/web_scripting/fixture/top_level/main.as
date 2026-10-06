import { Layout, Context } from "rive/host";
import { RtNative } from "rive/natives";

// Module start has no VM on WAMR, so an op like this one resolves nothing.
@external("rive_path_v1", "new")
declare function pathNew(): u32;

unsafe function boot(): i32 {
    RtNative.log(0, "top level log");
    return 1;
}
let booted: i32 = boot();
let topLevelPath: u32 = pathNew();

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init after top level");
        context.log("top level path handle " + topLevelPath.toString());
        return booted == 1;
    }
}
