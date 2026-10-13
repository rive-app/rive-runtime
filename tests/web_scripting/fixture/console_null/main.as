import { Layout, Context } from "rive/host";

let label: string? = null;

unsafe function logPastMemory(): void {
    console.log(changetype<string>(<usize>memory.size() * 65536 + 64));
}

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("before the null log");
        console.log(label);
        context.log("after the null log");
        logPastMemory();
        context.log("after the wild log");
        return true;
    }
}
