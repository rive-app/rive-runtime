import { Layout, Context } from "rive/host";

export class Main extends Layout {
    context: Context? = null;

    override init(context: Context): bool {
        this.context = context;
        let vm = context.viewModel();
        // The view model binds after the first init, which then reruns.
        if (vm == null) {
            return false;
        }
        let level = vm.getNumber("level");
        if (level == null) {
            context.log("no level");
            return false;
        }
        // The host calls back into the module for this as the value is set.
        level.addListenerWith(this, (self: Main): void => {
            let context = self.context;
            if (context != null) {
                context.log("listener ran");
            }
            throw new Error("listener threw");
        });
        level.value = 1;
        context.log("init went on after the listener");
        return true;
    }
}
