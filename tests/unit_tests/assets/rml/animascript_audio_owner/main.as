import { Node, Context } from "rive/host";
import { Audio, AudioSource } from "rive/audio";

// Starts the file's sound from init and again from its first advance, and
// drops both sounds.
export class Player extends Node {
    source: AudioSource? = null;
    started: bool = false;

    override init(context: Context): bool {
        let source = context.audio("what");
        this.source = source;
        if (source != null) {
            Audio.play(source);
        }
        return true;
    }

    override advance(seconds: f64): bool {
        let source = this.source;
        if (!this.started && source != null) {
            this.started = true;
            Audio.play(source);
        }
        return true;
    }
}
