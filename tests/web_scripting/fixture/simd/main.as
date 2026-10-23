import { Layout, Context } from "rive/host";

export class Main extends Layout {
    override init(context: Context): bool {
        // A random term keeps the optimizer from folding the vector away.
        let base = i32(Math.random() * 0) + 2;
        let sum = i32x4.add(i32x4.splat(base), i32x4.splat(20));
        context.log("simd lane " + i32x4.extract_lane(sum, 1).toString());
        return true;
    }
}
