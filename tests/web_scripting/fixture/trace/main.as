import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        trace("traced", 3, 1.5, 42, NaN);
        context.log("after trace");
        return true;
    }
}
