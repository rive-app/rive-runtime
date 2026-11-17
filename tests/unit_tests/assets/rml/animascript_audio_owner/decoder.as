import { Node, Context } from "rive/host";
import { Audio, AudioSource } from "rive/audio";
import { Blob } from "rive/shared";

// From its first advance, decodes the sound into a blob and plays it once
// the host settles that promise, at a later advance's poll.
export class Decoder extends Node {
    source: AudioSource? = null;
    asked: bool = false;

    override init(context: Context): bool {
        this.source = context.audio("what");
        return true;
    }

    override advance(seconds: f64): bool {
        let source = this.source;
        if (this.asked || source == null) {
            return true;
        }
        this.asked = true;
        source.toBlob().andThen((blob: Blob): void => {
            Audio.play(source);
        });
        return true;
    }
}
