import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("release init ran");
        return true;
    }
}
