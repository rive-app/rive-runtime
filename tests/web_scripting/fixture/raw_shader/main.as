import { Layout, Context } from "rive/host";

// Only hosts built with tools link this op.
@external("rive_gpu_v1", "shader_module_new")
declare function shaderModuleNew(desc: usize, descByteCount: u32, blob: usize, blobCount: u32): u32;

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init reaches the raw shader op");
        context.log("raw shader op returned " + shaderModuleNew(0, 0, 0, 0).toString());
        return true;
    }
}
