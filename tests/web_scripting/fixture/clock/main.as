import { Layout, Context } from "rive/host";
import { Audio, AudioHandle, AudioProcessor } from "rive/audio";
import { Ring, Snapshot } from "rive/shared";

export const kLead: f64 = 4800;

export class Note {
    frame: f64 = 0;
}

// The frames a clicker has rendered, and where it played the note.
export class Heard {
    rendered: f64 = 0;
    stamp: f64 = -1;
    played: f64 = -1;
}

export class Link {
    notes: Ring<Note> = Ring.create<Note>(256);
    heard: Snapshot<Heard> = Snapshot.create<Heard>();
}

// Clicks once at the note's frame, or at the start of the block that finds
// it late.
export class Clicker extends AudioProcessor<Link> {
    link: Link? = null;
    channels: i32 = 2;
    note: Note = new Note();
    waiting: bool = false;
    heard: Heard = new Heard();

    override init(sampleRate: f64, maxFrames: i32, channels: i32, link: Link): void {
        this.link = link;
        this.channels = channels;
    }

    override process(out: StaticArray<f32>, frames: i32, startFrame: f64): void {
        let link = this.link!;
        if (!this.waiting) {
            this.waiting = link.notes.receiveInto(this.note);
        }
        if (this.waiting && this.note.frame < startFrame + <f64>frames) {
            this.waiting = false;
            let at = <i32>Math.max(this.note.frame - startFrame, 0);
            for (let c = 0; c < this.channels; c++) {
                out[at * this.channels + c] = 1;
            }
            this.heard.stamp = this.note.frame;
            this.heard.played = startFrame + <f64>at;
        }
        this.heard.rendered = startFrame + <f64>frames;
        link.heard.publish(this.heard);
    }
}

// Once the clicker has rendered past the lead, stamps one note that far
// ahead of the main lane's audio clock.
export class Main extends Layout {
    context: Context? = null;
    link: Link = new Link();
    clicker: AudioHandle? = null;
    last: f64 = -1;
    sent: bool = false;
    reported: bool = false;

    override init(context: Context): bool {
        this.context = context;
        this.clicker = Audio.start(() => new Clicker(), this.link);
        return true;
    }

    override advance(seconds: f64): bool {
        let context = this.context!;
        let now = Audio.timeFrame();
        if (this.last < 0) {
            context.log("clock starts at " + now.toString());
        } else if (now < this.last) {
            context.log("clock went back from " + this.last.toString() + " to " + now.toString());
        }
        this.last = now;
        let heard = this.link.heard.latest();
        if (heard == null) {
            return true;
        }
        if (!this.sent && heard.value.rendered >= kLead) {
            this.sent = true;
            let note = new Note();
            note.frame = now + kLead;
            this.link.notes.send(note);
            context.log("clock sent at " + now.toString() + " rate " +
                Audio.sampleRate().toString() + " time " + Audio.time().toString());
            context.log("clock offset " + (heard.value.rendered - now).toString());
        }
        if (!this.reported && heard.value.played >= 0) {
            this.reported = true;
            context.log("clock played " + (heard.value.played - heard.value.stamp).toString() +
                " frames late");
        }
        return true;
    }
}
