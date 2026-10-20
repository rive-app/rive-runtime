import { Layout, Context } from "rive/host";

// Only hosts built with tools link this op.
@external("rive_gpu_v1", "shader_module_from_rstb")
declare function shaderModuleFromRstb(rstb: usize, rstbCount: u32, assetId: u32, entry: u32): u32;

export class Main extends Layout {
    override init(context: Context): bool {
        context.log("init reaches the raw shader op");
        context.log("raw shader op returned " + shaderModuleFromRstb(0, 0, 0, 0).toString());
        return true;
    }
}
