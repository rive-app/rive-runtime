import { Node, Context } from "rive/host";
import { Audio, AudioSource } from "rive/audio";

// Plays the sound each time its level input is set after init.
export class Tuner extends Node {
    source: AudioSource? = null;
    levelValue: f64 = 0;

    @input set level(value: f64) {
        this.levelValue = value;
        let source = this.source;
        if (source != null) {
            Audio.play(source);
        }
    }
    get level(): f64 {
        return this.levelValue;
    }

    override init(context: Context): bool {
        this.source = context.audio("what");
        return true;
    }
}
