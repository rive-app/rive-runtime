import { Layout, Context } from "rive/host";

unsafe function pages(): i32 {
    return memory.size();
}

export class Main extends Layout {
    context: Context? = null;
    advances: i32 = 0;
    settledPages: i32 = 0;
    sum: f64 = 0;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    // Two MiB of garbage a frame, which only a frame boundary frees.
    override advance(seconds: f64): bool {
        for (let i = 0; i < 32; i++) {
            const values = new Array<f64>(8192);
            values[i] = seconds;
            this.sum += values[i];
        }
        this.advances++;
        if (this.advances == 2) {
            this.settledPages = pages();
        } else if (this.advances == 32) {
            let context = this.context;
            if (context != null) {
                const grew = pages() - this.settledPages;
                context.log("heap grew " + grew.toString() + " pages over 30 frames");
            }
        }
        return true;
    }
}
