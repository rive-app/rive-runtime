import { Layout, Context } from "rive/host";

// No host has this op, so only calling it may fail.
@external("rive_web_gate_v1", "missing")
declare function missingOp(): void;

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init without the missing op");
        return true;
    }

    override advance(seconds: f64): bool {
        missingOp();
        return true;
    }
}
