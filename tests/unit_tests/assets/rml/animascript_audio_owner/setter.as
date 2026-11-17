import { Node, Context } from "rive/host";

// Sets the view model's score once, from its second advance, after
// Listener has started listening.
export class Setter extends Node {
    context: Context? = null;
    frames: i32 = 0;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        this.frames++;
        let context = this.context;
        if (this.frames != 2 || context == null) {
            return true;
        }
        let vm = context.viewModel();
        if (vm != null) {
            let score = vm.getNumber("score");
            if (score != null) {
                score.value = 1;
            }
        }
        return true;
    }
}
