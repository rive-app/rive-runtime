import { Layout, Context } from "rive/host";

function boot(): i32 {
    unreachable();
    return 1;
}
let booted: i32 = boot();

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init after a top level that trapped");
        return booted == 1;
    }
}
