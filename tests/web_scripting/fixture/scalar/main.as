import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        let lane = i32(Math.random() * 0) + 3;
        context.log("scalar lane " + lane.toString());
        return true;
    }
}
