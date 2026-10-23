import { Layout, Context } from "rive/host";

// A cast, since the compiler guards a nullable argument.
unsafe function logNull(): void {
    console.log(changetype<string>(0));
}

unsafe function logPastMemory(): void {
    console.log(changetype<string>(<usize>memory.size() * 65536 + 64));
}

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("before the null log");
        logNull();
        context.log("after the null log");
        logPastMemory();
        context.log("after the wild log");
        return true;
    }
}
