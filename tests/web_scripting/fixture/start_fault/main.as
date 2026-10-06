import { Layout, Context } from "rive/host";

// One of the few ops that reach librive from module start, and it prints.
@external("rive_rt_v1", "budget_exceeded")
declare function budgetExceeded(ms: u32): void;

budgetExceeded(1);

export class Main extends Layout {
    override init(context: Context): bool {
        return true;
    }
}
