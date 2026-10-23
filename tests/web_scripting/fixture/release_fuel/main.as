import { Layout, Context } from "rive/host";

let spin: f64 = 0;

// A runaway loop in a release bake, which only its own fuel can stop here.
export class Main extends Layout {
    override init(context: Context): bool {
        context.log("metered release init");
        while (spin >= 0) {
            spin += 1;
        }
        return true;
    }
}
