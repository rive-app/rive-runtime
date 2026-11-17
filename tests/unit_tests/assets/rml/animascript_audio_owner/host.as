import { Node, Context } from "rive/host";
import { Artboard } from "rive/artboard";
import { Audio, AudioSource } from "rive/audio";

// Makes its own instance of `deck`, whose script plays sounds, and plays its
// own sound from init and its first advance, each after the instance's hook.
export class Host extends Node {
    @input deck: Artboard? = null;
    inner: Artboard? = null;
    source: AudioSource? = null;
    started: bool = false;

    override init(context: Context): bool {
        this.source = context.audio("what");
        let deck = this.deck;
        if (deck != null) {
            this.inner = deck.instance();
        }
        this.playOwn();
        return true;
    }

    override advance(seconds: f64): bool {
        let inner = this.inner;
        if (inner != null) {
            inner.advance(seconds);
        }
        if (!this.started) {
            this.started = true;
            this.playOwn();
        }
        return true;
    }

    playOwn(): void {
        let source = this.source;
        if (source != null) {
            Audio.play(source);
        }
    }
}
