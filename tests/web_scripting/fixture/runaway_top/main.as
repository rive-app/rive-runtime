import { Layout, Context } from "rive/host";

// A top level that never ends, which only the module's own fuel can stop.
let spin: f64 = 0;
while (spin >= 0) {
    spin += 1;
}

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init after a runaway top level");
        return true;
    }
}
