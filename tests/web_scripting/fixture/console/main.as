import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        console.log("console log on the web");
        console.time("boot");
        console.timeLog("boot");
        console.timeEnd("boot");
        console.timeEnd("boot");
        return true;
    }
}
