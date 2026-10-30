import { Layout, Context } from "rive/host";
import { Parallel, Part, SharedArray, atomicAdd } from "rive/parallel";

@parallel export function tally(part: Part, seen: SharedArray<i32>, total: SharedArray<i32>): void {
    seen[part.index] = part.count;
    atomicAdd(total, 0, 1);
}

// rive.yaml's wasmMaxPages. The stub runtime never frees, so filling memory
// to two and a half spill zones below the cap leaves room for two members.
const kCapPages: i32 = 256;
const kSpillZone: i32 = 65536;
fill();

unsafe function fill(): StaticArray<u8> {
    let at = <i32>changetype<usize>(new StaticArray<u8>(0));
    return new StaticArray<u8>((kCapPages << 16) - at - (kSpillZone * 5) / 2);
}

export class Main extends Layout {
    context: Context? = null;
    checked: bool = false;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override advance(seconds: f64): bool {
        if (this.checked) {
            return true;
        }
        this.checked = true;
        let seen = new SharedArray<i32>(64);
        let total = new SharedArray<i32>(1);
        Parallel.run(tally, seen, total);
        let parts = seen[0];
        this.context!.log("capped parts " + parts.toString() + " total " + total[0].toString());
        this.context!.log("capped fewer parts " + (parts < Parallel.size() ? "yes" : "no"));
        return true;
    }
}
