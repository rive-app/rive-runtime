import { Node, Context } from "rive/host";
import { Audio } from "rive/audio";
import { keep } from "./shelf";

// Plays the sound from init and keeps it on the shelf.
export class Keeper extends Node {
    override init(context: Context): bool {
        let source = context.audio("what");
        if (source != null) {
            keep(Audio.play(source));
        }
        return true;
    }
}
