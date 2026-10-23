import { Layout, Context } from "rive/host";

export class Main extends Layout {
    sum: f64 = 0;

    override init(context: Context): bool {
        return true;
    }

    // Garbage the stub runtime never frees, two pages a frame.
    override advance(seconds: f64): bool {
        const values = new Array<f64>(16384);
        values[0] = seconds;
        this.sum += values[0];
        return true;
    }
}
