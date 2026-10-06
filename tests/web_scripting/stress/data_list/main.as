// A view model list of a thousand rows, every row's value written from the
// time advanced so far each frame, read back to draw bars. Their mean
// drives a markup bar through a data bind.
import { Layout, Context } from "rive/host";
import { Vector } from "rive/vector";
import { Path } from "rive/path";
import { Paint } from "rive/paint";
import { Renderer } from "rive/renderer";
import { ViewModel, PropertyNumber } from "rive/data";

const ROWS: i32 = 1024;
const PER_BAR: i32 = 4;
const BAR_HEIGHT: f64 = 220;

export class Main extends Layout {
    context: Context? = null;
    rows: Array<PropertyNumber> = [];
    total: PropertyNumber? = null;
    path: Path = new Path();
    paint: Paint = new Paint();
    time: f64 = 0;

    override init(context: Context): bool {
        this.context = context;
        this.paint.color = 0xFF4ECDC4;
        return true;
    }

    // The data context binds after init, so the list fills on first use.
    private fill(): bool {
        if (this.rows.length > 0) {
            return true;
        }
        let context = this.context;
        let feed = context != null ? context.viewModel() : null;
        if (feed == null) {
            return false;
        }
        let list = feed.getList("rows");
        this.total = feed.getNumber("total");
        if (list == null || this.total == null) {
            return false;
        }
        for (let i = 0; i < ROWS; i++) {
            let row = ViewModel.create("Row");
            list.push(row);
            let value = row.getNumber("value");
            if (value != null) {
                this.rows.push(value);
            }
        }
        if (context != null) {
            context.log("rows " + list.length.toString());
        }
        return true;
    }

    override advance(seconds: f64): bool {
        this.time += seconds;
        if (!this.fill()) {
            return true;
        }
        let sum: f64 = 0;
        for (let i = 0; i < this.rows.length; i++) {
            let value = 0.5 + 0.5 * Math.sin(<f64>i * 0.05 + this.time * 2) *
                Math.cos(<f64>i * 0.013 - this.time);
            this.rows[i].value = value;
            sum += value;
        }
        let total = this.total;
        if (total != null) {
            total.value = sum / <f64>ROWS * 232;
        }
        return true;
    }

    override draw(renderer: Renderer): void {
        let path = this.path;
        path.reset();
        let bars = this.rows.length / PER_BAR;
        for (let b = 0; b < bars; b++) {
            let value: f64 = 0;
            for (let k = 0; k < PER_BAR; k++) {
                value += this.rows[b * PER_BAR + k].value;
            }
            let height = <f32>(value / <f64>PER_BAR * BAR_HEIGHT);
            let x = <f32>b;
            path.moveTo(Vector.xy(x, 230 - height));
            path.lineTo(Vector.xy(x + 0.8, 230 - height));
            path.lineTo(Vector.xy(x + 0.8, 230));
            path.lineTo(Vector.xy(x, 230));
            path.close();
        }
        renderer.drawPath(path, this.paint);
    }
}
