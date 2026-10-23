import { Layout, Context } from "rive/host";
import { Vector } from "rive/vector";
import { Path } from "rive/path";
import { Paint, PaintStyle } from "rive/paint";
import { Renderer } from "rive/renderer";
import { clock, time } from "rive/os";

export class Main extends Layout {
    path: Path = new Path();
    paint: Paint = new Paint();
    frames: f64 = 0;
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        this.path.moveTo(Vector.xy(0, 0));
        this.path.lineTo(Vector.xy(100, 0));
        this.path.lineTo(Vector.xy(100, 50));
        this.path.close();
        this.paint.style = PaintStyle.fill;
        this.paint.color = 0xFF336699;
        context.log("init ran on the web");
        if (clock() > 0 && time() > 1700000000) {
            context.log("clocks ok");
        }
        let roll = Math.random();
        if (roll >= 0 && roll < 1) {
            context.log("random ok");
        }
        return true;
    }

    override advance(seconds: f64): bool {
        this.frames += 1;
        return true;
    }

    override draw(renderer: Renderer): void {
        renderer.drawPath(this.path, this.paint);
        if (this.frames == 2) {
            let context = this.context;
            if (context != null) {
                context.log("drew frame 2");
            }
        }
    }
}
