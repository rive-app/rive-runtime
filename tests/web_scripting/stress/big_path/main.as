// One path of thousands of segments, rebuilt in advance from the time
// advanced so far every frame, then filled and stroked.
import { Layout, Context } from "rive/host";
import { Vector } from "rive/vector";
import { Path } from "rive/path";
import { Paint, PaintStyle } from "rive/paint";
import { Renderer } from "rive/renderer";

const SEGMENTS: i32 = 4096;
const CENTER: f32 = 128;

export class Main extends Layout {
    path: Path = new Path();
    fill: Paint = new Paint();
    stroke: Paint = new Paint();
    time: f32 = 0;

    override init(context: Context): bool {
        this.fill.style = PaintStyle.fill;
        this.fill.color = 0xFF2E86DE;
        this.stroke.style = PaintStyle.stroke;
        this.stroke.thickness = 1.5;
        this.stroke.color = 0xFFFFD166;
        return true;
    }

    override advance(seconds: f64): bool {
        this.time += <f32>seconds;
        let path = this.path;
        path.reset();
        path.moveTo(this.point(0));
        // Every fourth segment curves, so both verbs cross.
        for (let i = 1; i < SEGMENTS; i++) {
            if ((i & 3) == 0 && i + 2 < SEGMENTS) {
                path.cubicTo(this.point(i), this.point(i + 1), this.point(i + 2));
                i += 2;
            } else {
                path.lineTo(this.point(i));
            }
        }
        path.close();
        return true;
    }

    private point(i: i32): Vector {
        let angle = <f32>i / <f32>SEGMENTS * 6.2831853;
        let radius = 70 + 40 * Mathf.sin(angle * 12 + this.time * 2) *
            Mathf.cos(angle * 7 - this.time);
        return Vector.xy(CENTER + radius * Mathf.cos(angle),
            CENTER + radius * Mathf.sin(angle));
    }

    override draw(renderer: Renderer): void {
        renderer.drawPath(this.path, this.fill);
        renderer.drawPath(this.path, this.stroke);
    }
}
