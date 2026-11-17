import { AudioSound } from "rive/audio";

// A sound every script that imports this module shares: one script keeps it
// here and another uses it.
let kept: AudioSound? = null;

export function keep(sound: AudioSound?): void {
    kept = sound;
}

export function keptSound(): AudioSound? {
    return kept;
}
