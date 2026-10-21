import { Layout, Context } from "rive/host";

@external("rive_shader_v1", "linear")
declare function shaderLinear(sx: f32, sy: f32, ex: f32, ey: f32,
    colors: usize, stops: usize, count: u32): u32;

// The top of a full 4 GiB memory, whose page count overflows 16 bits.
const kColors: usize = 0xFFFFFF00;
const kStops: usize = 0xFFFFFF80;

unsafe function topMemory(context: Context): void {
    if (memory.grow(65536 - memory.size()) < 0) {
        context.log("memory did not grow");
        return;
    }
    store<u32>(kColors, 0xFFFF0000);
    store<u32>(kColors + 4, 0xFF0000FF);
    store<f32>(kStops, 0);
    store<f32>(kStops + 4, 1);
    const handle = shaderLinear(0, 0, 1, 1, kColors, kStops, 2);
    context.log(handle != 0 ? "top gradient minted" : "top gradient refused");
    // Big enough that the collector carves a fresh chunk.
    const big = "x".repeat(300000);
    context.log("top carve " + big.length.toString());
}

export class Main extends Layout {
    override init(context: Context): bool {
        topMemory(context);
        return true;
    }
}
