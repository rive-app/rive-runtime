// An image mesh of a 64 by 64 vertex grid whose positions advance rewrites
// from the time advanced so far every frame. The uvs and triangles stay.
import { Layout, Context } from "rive/host";
import { Vector } from "rive/vector";
import { BlendMode } from "rive/paint";
import { Renderer } from "rive/renderer";
import { Image, ImageSampler, ImageWrap, ImageFilter, VertexBuffer, TriangleBuffer } from "rive/image";

const GRID: i32 = 64;
const STEP: f32 = 3.5;
const MARGIN: f32 = 16;

export class Main extends Layout {
    image: Image? = null;
    vertices: VertexBuffer = new VertexBuffer();
    uvs: VertexBuffer = new VertexBuffer();
    triangles: TriangleBuffer = new TriangleBuffer();
    sampler: ImageSampler = ImageSampler(ImageWrap.clamp, ImageWrap.clamp, ImageFilter.nearest);
    time: f32 = 0;

    override init(context: Context): bool {
        this.image = context.image("checker");
        if (this.image == null) {
            context.log("checker image missing");
        }
        let last = <f32>(GRID - 1);
        for (let y = 0; y < GRID; y++) {
            for (let x = 0; x < GRID; x++) {
                this.uvs.add(Vector.xy(<f32>x / last, <f32>y / last));
            }
        }
        for (let y = 0; y < GRID - 1; y++) {
            for (let x = 0; x < GRID - 1; x++) {
                let at = <u32>(y * GRID + x);
                this.triangles.add(at, at + 1, at + <u32>GRID);
                this.triangles.add(at + 1, at + <u32>GRID + 1, at + <u32>GRID);
            }
        }
        return true;
    }

    override advance(seconds: f64): bool {
        this.time += <f32>seconds;
        let vertices = this.vertices;
        vertices.reset();
        for (let y = 0; y < GRID; y++) {
            for (let x = 0; x < GRID; x++) {
                let wave = Mathf.sin(<f32>x * 0.2 + this.time * 3) * 6;
                let sway = Mathf.cos(<f32>y * 0.15 + this.time * 2) * 6;
                vertices.add(Vector.xy(MARGIN + <f32>x * STEP + sway,
                    MARGIN + <f32>y * STEP + wave));
            }
        }
        return true;
    }

    override draw(renderer: Renderer): void {
        let image = this.image;
        if (image == null) {
            return;
        }
        renderer.drawImageMesh(image, this.sampler, this.vertices, this.uvs,
            this.triangles, BlendMode.srcOver, 1);
    }
}
