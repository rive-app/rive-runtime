import { Layout, Context } from "rive/host";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";

// Sleeps past the first frames, so only its timed wait keeps a host advancing.
@worker export async function sleeper(): Promise<void> {
    await Worker.sleep(0.2);
    console.log("sleeper woke");
}

export class Main extends Layout {
    sleeper: Worker? = null;

    override init(context: Context): bool {
        this.sleeper = Worker.spawn(sleeper);
        return true;
    }
}
