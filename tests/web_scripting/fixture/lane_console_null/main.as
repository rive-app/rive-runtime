import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { Job, Jobs } from "rive/jobs";
import { Promise } from "rive/promise";
import { Worker } from "rive/workers";

// A cast, since the compiler guards a nullable argument.
unsafe function logNull(): void {
    console.log(changetype<string>(0));
}

@job export function speaks(n: i32): i32 {
    logNull();
    console.log("job after the null log");
    return n;
}

@worker export async function talks(): Promise<void> {
    logNull();
    console.log("worker after the null log");
}

export class Speaker extends AudioProcessor<i32> {
    spoke: bool = false;

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        if (!this.spoke) {
            this.spoke = true;
            logNull();
            console.log("processor after the null log");
        }
    }
}

export class Main extends Layout {
    job: Job<i32>? = null;
    worker: Worker? = null;
    speaker: AudioHandle? = null;

    override init(context: Context): bool {
        this.job = Jobs.run(speaks, 1);
        this.worker = Worker.spawn(talks);
        this.speaker = Audio.start(() => new Speaker(), 0);
        return true;
    }
}
