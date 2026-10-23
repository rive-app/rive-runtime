import { Layout } from "rive/host";
import { SharedRegion } from "rive/shared";

// Fails the file holding the region, which no file loaded after it may find
// still held.
let region = SharedRegion.create(16);
region.exclusive((view): void => {
    throw new Error("threw inside exclusive");
});

export class Main extends Layout {}
