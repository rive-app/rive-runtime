import { Layout, Context } from "rive/host";
import { Vector } from "rive/vector";
import { Path } from "rive/path";
import { Paint, Gradient, GradientStop } from "rive/paint";
import { Renderer } from "rive/renderer";

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
        this.paint.gradient = Gradient.linear(Vector.xy(0, 0), Vector.xy(100, 50), [
            GradientStop(0, 0xFFFF0000),
            GradientStop(1, 0xFF0000FF),
        ]);
        context.log("gradient ready");
        return true;
    }

    override advance(seconds: f64): bool {
        this.frames += 1;
        if (this.frames == 3) {
            unreachable();
        }
        return true;
    }

    override draw(renderer: Renderer): void {
        renderer.drawPath(this.path, this.paint);
        let context = this.context;
        if (context != null) {
            context.log("drew after advance " + this.frames.toString());
        }
    }
}
