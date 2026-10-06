import { Layout, Context } from "rive/host";

// Where the std read its clocks before rive_rt_v1 carried them.
@external("env", "emscripten_get_now")
declare function legacyNow(): f64;
@external("env", "emscripten_date_now")
declare function legacyDateNow(): f64;

export class Main extends Layout {
    override init(context: Context): bool {
        if (legacyNow() > 0 && legacyDateNow() > 1700000000000) {
            context.log("legacy clocks ok");
        }
        return true;
    }
}
