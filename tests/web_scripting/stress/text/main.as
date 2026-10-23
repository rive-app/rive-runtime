// Text rebuilt in advance from the time advanced so far every frame:
// twelve lines of shaped runs, laid out and drawn.
import { Layout, Context } from "rive/host";
import { Paint } from "rive/paint";
import { Renderer } from "rive/renderer";
import { Font, Text, TextRunStyle, TextSizing } from "rive/text";

const LINES: i32 = 12;

export class Main extends Layout {
    font: Font? = null;
    paint: Paint = new Paint();
    accent: Paint = new Paint();
    text: Text = new Text();
    time: f64 = 0;

    override init(context: Context): bool {
        // Digits and capitals are all the label font carries.
        this.font = context.font("label_font");
        if (this.font == null) {
            context.log("label font missing");
        }
        this.paint.color = 0xFFF0F0F0;
        this.accent.color = 0xFFFF8C42;
        return true;
    }

    override advance(seconds: f64): bool {
        this.time += seconds;
        let font = this.font;
        if (font == null) {
            return true;
        }
        let text = this.text;
        text.clear();
        let plain = new TextRunStyle(font);
        plain.size = 16;
        plain.paint = this.paint;
        let bold = new TextRunStyle(font);
        bold.size = 18;
        bold.paint = this.accent;
        let frame = <i32>Math.round(this.time * 60);
        for (let i = 0; i < LINES; i++) {
            text.append("ROW " + i.toString() + " ", plain);
            text.append("FRAME " + (frame + i * 7).toString() + "\n", bold);
        }
        text.sizing = TextSizing.fixed;
        text.maxWidth = 240;
        text.maxHeight = 256;
        return true;
    }

    override draw(renderer: Renderer): void {
        this.text.draw(renderer);
    }
}
