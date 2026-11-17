import { Node, Context } from "rive/host";
import { Audio, AudioSource } from "rive/audio";

// Listens to the view model's score from its first advance and plays the
// sound each time it changes.
export class Listener extends Node {
    context: Context? = null;
    source: AudioSource? = null;
    listening: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.source = context.audio("what");
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context;
        if (this.listening || context == null) {
            return true;
        }
        let vm = context.viewModel();
        if (vm == null) {
            return true;
        }
        let score = vm.getNumber("score");
        if (score != null) {
            this.listening = true;
            score.addListenerWith(this, (self: Listener): void => {
                let source = self.source;
                if (source != null) {
                    Audio.play(source);
                }
            });
        }
        return true;
    }
}
