import { Layout, Context } from "rive/host";

@external("rive_rt_v1", "log")
declare function rtLog(level: i32, message: usize, length: u32): void;
@external("rive_shader_v1", "linear")
declare function shaderLinear(sx: f32, sy: f32, ex: f32, ey: f32,
    colors: usize, stops: usize, count: u32): u32;

// The top of a full 4 GiB memory, whose page count overflows 16 bits.
const kColors: usize = 0xFFFFFF00;
const kStops: usize = 0xFFFFFF80;
const kText: usize = 0xFFFFFFC0;

unsafe function topMemory(minted: string, refused: string): void {
    if (memory.grow(65536 - memory.size()) < 0) {
        return;
    }
    store<u32>(kColors, 0xFFFF0000);
    store<u32>(kColors + 4, 0xFF0000FF);
    store<f32>(kStops, 0);
    store<f32>(kStops + 4, 1);
    const handle = shaderLinear(0, 0, 1, 1, kColors, kStops, 2);
    // Logged from raw memory: the collector cannot allocate in a full 4 GiB.
    const text = handle != 0 ? minted : refused;
    for (let i = 0; i < text.length; i++) {
        store<u16>(kText + i * 2, text.charCodeAt(i));
    }
    rtLog(0, kText, text.length * 2);
}

export class Main extends Layout {
    override init(context: Context): bool {
        topMemory("top gradient minted", "top gradient refused");
        return true;
    }
}
