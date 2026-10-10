import { Layout, Context } from "rive/host";
import { Renderer } from "rive/renderer";
import { Artboard } from "rive/artboard";

export class Main extends Layout {
    @input deck: Artboard? = null;
    override init(context: Context): bool { context.log("init"); return true; }
    override draw(renderer: Renderer): void {}
}
