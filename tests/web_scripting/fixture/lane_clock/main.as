import { Layout, Context } from "rive/host";
import { Jobs } from "rive/jobs";
import { Worker } from "rive/workers";
import { clock } from "rive/os";

@job export function jobClock(): f64 {
    return clock();
}

@worker export async function workerClock(): Promise<f64> {
    return clock();
}

// A lane reads the main lane's clock, so its reading falls between its submit
// and the main lane seeing the result.
function agrees(context: Context, lane: string, submitted: f64, read: f64): void {
    let seen = clock();
    let lag = (read - submitted) * 1000;
    context.log(
        "lane clock " + lane + " agrees " + (read >= submitted && read <= seen ? "yes" : "no") +
            ", lag " + lag.toString() + " ms",
    );
}

export class Main extends Layout {
    override init(context: Context): bool {
        let jobSubmitted = clock();
        Jobs.run(jobClock).promise().andThen((read: f64): void => {
            agrees(context, "job", jobSubmitted, read);
        });
        let workerSubmitted = clock();
        Worker.spawn(workerClock).promise().andThen((read: f64): void => {
            agrees(context, "worker", workerSubmitted, read);
        });
        return true;
    }
}
