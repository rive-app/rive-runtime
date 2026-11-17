import { Node, Context } from "rive/host";
import { Audio, AudioSound, AudioSource } from "rive/audio";
import { ViewModel } from "rive/data";
import { keptSound } from "./shelf";

// From its advance, runs op on the shelf's sound and clears it: 1 plays it,
// 2 resumes it, 3 plays another sound and stops that one, 4 runs every other
// operation. Then reports whether the sound says it has completed.
export class Poker extends Node {
    context: Context? = null;
    source: AudioSource? = null;

    override init(context: Context): bool {
        this.context = context;
        this.source = context.audio("what");
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context;
        let sound = keptSound();
        if (context == null || sound == null) {
            return true;
        }
        let vm = context.viewModel();
        if (vm == null) {
            return true;
        }
        let op = vm.getNumber("op");
        if (op == null || op.value == 0) {
            return true;
        }
        this.runOp(vm, sound, op.value);
        op.value = 0;
        let completed = vm.getBoolean("completed");
        if (completed != null) {
            completed.value = sound.completed();
        }
        return true;
    }

    runOp(vm: ViewModel, sound: AudioSound, op: f64): void {
        if (op == 1) {
            sound.play();
        } else if (op == 2) {
            sound.resume();
        } else if (op == 3) {
            let source = this.source;
            if (source != null) {
                let other = Audio.play(source);
                if (other != null) {
                    other.stop();
                }
            }
        } else if (op == 4) {
            sound.pause();
            sound.stop();
            sound.stop(100);
            let seeked = vm.getBoolean("seeked");
            if (seeked != null) {
                seeked.value = sound.seek(0) || sound.seekFrame(0);
            }
            let time = vm.getNumber("time");
            if (time != null) {
                time.value = sound.time() + sound.timeFrame();
            }
            sound.volume = 0.5;
            let volume = vm.getNumber("volume");
            if (volume != null) {
                volume.value = sound.volume;
            }
        }
    }
}
