import { Layout } from "rive/host";

class Holder {
    leaf: Leaf? = null;
}

class Leaf {
    v: i32 = 0;
}

let keep: Holder? = null;
let bigs: Array<StaticArray<u8>> = [];

export class Main extends Layout {
    // More young data than the page ceiling has room to promote.
    override advance(seconds: f64): bool {
        if (keep == null) {
            let holder = new Holder();
            holder.leaf = new Leaf();
            keep = holder;
            for (let i = 0; i < 6; i++) {
                bigs.push(new StaticArray<u8>(200000));
            }
        }
        return true;
    }
}
