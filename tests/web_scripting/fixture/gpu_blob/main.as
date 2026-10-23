import { Layout, Context } from "rive/host";
import { GPUBuffer } from "rive/gpu";
import { BufferUsage } from "rive/gpu_enums";
import { Job, Jobs } from "rive/jobs";
import { Blob } from "rive/shared";

@job export function skins(count: i32): Blob {
    let bytes = new Uint8Array(count);
    for (let i = 0; i < count; i++) {
        bytes[i] = <u8>(i * 3);
    }
    return Blob.from(bytes.buffer);
}

// A job's blob reaches GPU buffers without passing through this lane's
// memory, and is gone before anything replays the upload or the make.
export class Main extends Layout {
    context: Context? = null;
    vertices: GPUBuffer? = null;
    mesh: GPUBuffer? = null;
    skinning: Job<Blob>? = null;

    override init(context: Context): bool {
        this.context = context;
        this.vertices = new GPUBuffer(BufferUsage.vertex, 64);
        this.skinning = Jobs.run(skins, 48);
        return true;
    }

    override advance(seconds: f64): bool {
        let skinning = this.skinning;
        if (skinning != null && !skinning.running) {
            this.skinning = null;
            let skinned = skinning.peek()!.value;
            skinning.release();
            this.vertices!.updateFromBlob(8, skinned, 32, 16);
            this.mesh = GPUBuffer.fromBlob(BufferUsage.vertex, skinned, true, 8, 24);
            skinned.release();
            this.context!.log("skin uploaded");
        }
        return true;
    }
}
