// A GPU Canvas pass per frame: advance rewrites every quad's vertices and
// every draw's uniforms from the time advanced so far, draw uploads them,
// renders the pass and draws the canvas as an image. Nothing reads the
// clock, so frame N renders the same anywhere.
import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { BlendMode } from "rive/paint";
import { ImageSampler, ImageWrap, ImageFilter } from "rive/image";
import {
    GPUBindGroup,
    GPUBindGroupUBO,
    GPUBuffer,
    GPUCanvas,
    GPUColorAttachment,
    GPUColorTargetDesc,
    GPUPassDesc,
    GPUPipeline,
    GPUPipelineDesc,
    GPUVertexAttribute,
    GPUVertexBufferLayout,
} from "rive/gpu";
import { BufferUsage, LoadOp, StoreOp, TextureFormat, VertexFormat } from "rive/gpu_enums";

const SIZE: u32 = 256;
const GRID: i32 = 32;
const DRAWS: i32 = 16;
const QUADS: i32 = GRID * GRID;
const FLOATS_PER_VERTEX: i32 = 3;
const VERTICES: i32 = QUADS * 6;
const UNIFORM_BYTES: u32 = 32;

export class Main extends Layout {
    canvas: GPUCanvas? = null;
    pipeline: GPUPipeline? = null;
    vertices: GPUBuffer? = null;
    staging: Float32Array = new Float32Array(VERTICES * FLOATS_PER_VERTEX);
    uniforms: Array<GPUBuffer> = [];
    groups: Array<GPUBindGroup> = [];
    uniformStaging: Float32Array = new Float32Array(DRAWS * 8);
    pass: GPUPassDesc = new GPUPassDesc();
    sampler: ImageSampler = ImageSampler(ImageWrap.clamp, ImageWrap.clamp, ImageFilter.nearest);
    time: f32 = 0;

    override init(context: Context): bool {
        let canvas = context.gpuCanvas(SIZE, SIZE);
        let shader = context.shader("quads");
        if (canvas == null || shader == null) {
            context.log("gpu canvas unavailable");
            return true;
        }
        let layout = new GPUVertexBufferLayout();
        layout.stride = <u32>FLOATS_PER_VERTEX * 4;
        layout.attributes = [
            new GPUVertexAttribute(VertexFormat.float2, 0, 0),
            new GPUVertexAttribute(VertexFormat.float1, 1, 8),
        ];
        let color = new GPUColorTargetDesc();
        color.format = <TextureFormat>canvas.format;
        let desc = new GPUPipelineDesc();
        desc.vertexModule = shader;
        desc.colorTargets = [color];
        desc.vertexBuffers = [layout];
        let pipeline = new GPUPipeline(desc);
        let groupLayout = pipeline.getBindGroupLayout(0);
        for (let i = 0; i < DRAWS; i++) {
            let buffer = new GPUBuffer(BufferUsage.uniform, UNIFORM_BYTES);
            this.uniforms.push(buffer);
            this.groups.push(new GPUBindGroup(groupLayout, [new GPUBindGroupUBO(0, buffer)]));
        }
        this.vertices = new GPUBuffer(BufferUsage.vertex, <u32>this.staging.byteLength);
        let attachment = new GPUColorAttachment();
        attachment.loadOp = LoadOp.clear;
        attachment.storeOp = StoreOp.store;
        attachment.clearR = 0.1;
        attachment.clearG = 0.1;
        attachment.clearB = 0.2;
        this.pass.color = [attachment];
        this.canvas = canvas;
        this.pipeline = pipeline;
        context.log("gpu canvas ready");
        return true;
    }

    override advance(seconds: f64): bool {
        this.time += <f32>seconds;
        this.writeVertices();
        let u = this.uniformStaging;
        for (let i = 0; i < DRAWS; i++) {
            let t = <f32>i / <f32>DRAWS;
            let at = i * 8;
            u[at] = 0.4 + 0.6 * t;
            u[at + 1] = 0.5 + 0.5 * Mathf.sin(this.time * 6 + t * 6.28);
            u[at + 2] = 1.0 - t;
            u[at + 3] = 1;
            u[at + 4] = 0.02 * Mathf.sin(this.time * 4.2 + <f32>i * 0.07);
            u[at + 5] = 0;
        }
        return true;
    }

    // Quads in a grid that breathe with time, shaded by their cell so
    // a flipped or shifted canvas shows.
    private writeVertices(): void {
        let cell: f32 = 2.0 / <f32>GRID;
        let phase = this.time * 3;
        let at = 0;
        for (let y = 0; y < GRID; y++) {
            for (let x = 0; x < GRID; x++) {
                let grow = 0.3 + 0.2 * Mathf.sin(phase + <f32>(x + y) * 0.3);
                let x0 = -1.0 + <f32>x * cell + cell * (0.5 - grow);
                let y0 = -1.0 + <f32>y * cell + cell * (0.5 - grow);
                let x1 = x0 + cell * grow * 2;
                let y1 = y0 + cell * grow * 2;
                let shade = 0.25 + 0.75 * <f32>(x + GRID * y) / <f32>QUADS;
                at = this.corner(at, x0, y0, shade);
                at = this.corner(at, x1, y0, shade);
                at = this.corner(at, x1, y1, shade);
                at = this.corner(at, x0, y0, shade);
                at = this.corner(at, x1, y1, shade);
                at = this.corner(at, x0, y1, shade);
            }
        }
    }

    private corner(at: i32, x: f32, y: f32, shade: f32): i32 {
        this.staging[at] = x;
        this.staging[at + 1] = y;
        this.staging[at + 2] = shade;
        return at + 3;
    }

    override draw(renderer: Renderer): void {
        let canvas = this.canvas;
        let pipeline = this.pipeline;
        let vertices = this.vertices;
        if (canvas == null || pipeline == null || vertices == null) {
            return;
        }
        vertices.update(0, this.staging.buffer);
        for (let i = 0; i < DRAWS; i++) {
            this.uniforms[i].update(0, this.uniformStaging.buffer, UNIFORM_BYTES, <u32>i * UNIFORM_BYTES);
        }
        let pass = canvas.beginRenderPass(this.pass);
        pass.setPipeline(pipeline);
        pass.setVertexBuffer(0, vertices);
        let perDraw = <u32>(VERTICES / DRAWS);
        for (let i = 0; i < DRAWS; i++) {
            pass.setBindGroup(0, this.groups[i]);
            pass.draw(perDraw, 1, <u32>i * perDraw);
        }
        pass.finish();
        renderer.drawImage(canvas.image, this.sampler, BlendMode.srcOver, 1);
    }
}
