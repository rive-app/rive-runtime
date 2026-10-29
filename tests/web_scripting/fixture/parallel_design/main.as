import { Layout, Context } from "rive/host";
import { Parallel, Part, PerPart, SharedArray } from "rive/parallel";

let mainCalls = 0;

function lane(out: SharedArray<i32>, i: i32): void {
    out[i] = Parallel.inPart() ? 1 : 0;
    if (!Parallel.inPart()) {
        mainCalls++;
    }
}

@parallel export function lanes(part: Part, out: SharedArray<i32>): void {
    lane(out, part.index);
}

export class Scratch {
    owner: i32 = -1;
}

const scratch = new PerPart<Scratch>(() => new Scratch());

@parallel.inplace export function claim(part: Part, out: SharedArray<i32>): void {
    let own = scratch.get(part);
    own.owner = part.index;
    out[part.index] = own.owner;
}

export type Friction = @parallel (a: f32, b: f32) => f32;

export class World {
    friction: Friction = (a: f32, b: f32): f32 => Mathf.sqrt(a * b);
}

@parallel export function frictions(part: Part, world: World, out: SharedArray<f32>): void {
    for (let i = part.start(out.length); i < part.end(out.length); i++) {
        out[i] = world.friction(<f32>i + 0.5, 2);
    }
}

export class Main extends Layout {
    frames: i32 = 0;
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        this.frames++;
        if (this.frames != 1) {
            return true;
        }
        let context = this.context!;
        let out = new SharedArray<i32>(64);
        lane(out, 63);
        Parallel.runParts(9, lanes, out);
        let parts = 0;
        for (let i = 0; i < 9; i++) {
            parts += out[i];
        }
        context.log("design in part " + (parts == 9 && out[63] == 0 && mainCalls == 1 ? "ok" : "wrong"));
        let owned = new SharedArray<i32>(64);
        Parallel.runParts(9, claim, owned);
        let wrong = 0;
        for (let i = 0; i < 9; i++) {
            if (owned[i] != i) wrong++;
        }
        context.log("design per part wrong " + wrong.toString() + " objects " + (scratch.length >= 9 ? "enough" : "short"));
        let world = new World();
        let f = new SharedArray<f32>(200);
        Parallel.run(frictions, world, f);
        let bits = 0;
        for (let i = 0; i < f.length; i++) {
            if (reinterpret<i32>(f[i]) != reinterpret<i32>(world.friction(<f32>i + 0.5, 2))) bits++;
        }
        context.log("design friction wrong " + bits.toString());
        return true;
    }
}
