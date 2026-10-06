import { Layout, Context } from "rive/host";
import { ScrollEvent, ScrollPhase } from "rive/events";

// Claims only the event the harness sends, so a claim proves every field
// crossed intact.
export class Main extends Layout {
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        return true;
    }

    override pointerScroll(event: ScrollEvent): void {
        if (event.phase == ScrollPhase.update && event.precise &&
            event.id == 2 && event.position.x == 50 &&
            event.position.y == 60 && event.delta.x == 0 &&
            event.delta.y == -30 && event.timeStamp == 1.5) {
            let context = this.context;
            if (context != null) {
                context.log("scroll crossed intact");
            }
            event.hit();
        }
    }
}
