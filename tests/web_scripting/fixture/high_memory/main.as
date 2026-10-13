import { Layout, Context } from "rive/host";

@external("rive_rt_v1", "log")
declare function rtLog(level: i32, message: usize, length: u32): void;
@external("rive_mat4_v1", "invert")
declare function mat4Invert(out: usize, outBytes: u32, src: usize,
    srcBytes: u32): u32;
@external("rive_shader_v1", "linear")
declare function shaderLinear(sx: f32, sy: f32, ex: f32, ey: f32,
    colors: usize, stops: usize, count: u32): u32;

// Past 2 GiB, where a module address reads as a negative i32 in JS.
const kText: usize = 0x80000010;
const kSrc: usize = 0x80000100;
const kOut: usize = 0x80000200;
const kColors: usize = 0x80000300;
const kStops: usize = 0x80000400;
const kManaged: usize = 0x80000510;

// A managed string at a raw address: its byte length in the word before it.
unsafe function storeManaged(at: usize, text: string): string {
    store<u32>(at - 4, text.length * 2);
    for (let i = 0; i < text.length; i++) {
        store<u16>(at + i * 2, text.charCodeAt(i));
    }
    return changetype<string>(at);
}

unsafe function highMemory(context: Context): void {
    if (memory.grow(32769 - memory.size()) < 0) {
        context.log("memory did not grow");
        return;
    }
    const text = "hi from past 2 GiB";
    for (let i = 0; i < text.length; i++) {
        store<u16>(kText + i * 2, text.charCodeAt(i));
    }
    rtLog(0, kText, text.length * 2);
    const managed = storeManaged(kManaged, "managed past 2 GiB");
    console.log(managed);
    trace(managed, 1, 2);

    for (let i = 0; i < 16; i++) {
        store<f32>(kSrc + i * 4, i % 5 == 0 ? 2 : 0);
    }
    const inverted = mat4Invert(kOut, 64, kSrc, 64);
    context.log("high mat4 inverse " + inverted.toString() + " " +
        load<f32>(kOut).toString());

    store<u32>(kColors, 0xFFFF0000);
    store<u32>(kColors + 4, 0xFF0000FF);
    store<f32>(kStops, 0);
    store<f32>(kStops + 4, 1);
    const gradient = shaderLinear(0, 0, 1, 1, kColors, kStops, 2);
    context.log(gradient != 0 ? "high gradient minted" : "high gradient refused");
}

export class Main extends Layout {
    override init(context: Context): bool {
        highMemory(context);
        return true;
    }
}
