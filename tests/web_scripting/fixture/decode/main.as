import { Layout, Context, DecodedImage, ImageDecodeListener } from "rive/host";

class Report extends ImageDecodeListener {
    context: Context;

    constructor(context: Context) {
        super();
        this.context = context;
    }

    override onImageDecoded(image: DecodedImage): void {
        let pixels = Uint8Array.wrap(image.data);
        this.context.log("decoded " + image.width.toString() + "x" +
            image.height.toString() + " first pixel " + pixels[0].toString() +
            "," + pixels[3].toString());
    }

    override onImageDecodeFailed(message: string): void {
        this.context.log("decode failed: " + message);
    }
}

export class Main extends Layout {
    override init(context: Context): bool {
        // The gate's stand in decoder takes a PNG signature byte.
        let image = new Uint8Array(4);
        image[0] = 0x89;
        context.decodeImage(image.buffer, new Report(context));
        context.decodeImage(new Uint8Array(4).buffer, new Report(context));
        return true;
    }
}
