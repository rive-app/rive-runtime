import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        // A random term keeps the optimizer from folding the vector away.
        let a = f32(Math.random() * 0) + 2;
        let fused = f32x4.relaxed_madd(f32x4.splat(a), f32x4.splat(3), f32x4.splat(1));
        context.log("relaxed lane " + i32(f32x4.extract_lane(fused, 0)).toString());
        return true;
    }
}
