import { Layout, Context } from "rive/host";

@external("rive_shader_v1", "linear")
declare function shaderLinear(sx: f32, sy: f32, ex: f32, ey: f32,
    colors: usize, stops: usize, count: u32): u32;

export class Main extends Layout {
    override init(context: Context): bool {
        // Stops far past the module's memory, just under the size cap.
        let handle = shaderLinear(0, 0, 1, 1, 16, 16, 0x3FFFFFF0);
        context.log("huge gradient handle " + handle.toString());
        return true;
    }
}
