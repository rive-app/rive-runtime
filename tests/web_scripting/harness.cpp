// Imports a riv the way a web host does: import, let the page prepare the
// script modules, start the scripts, then run an artboard for a few frames.
#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/scroll_event.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

#include <emscripten/emscripten.h>
#include <emscripten/stack.h>

using namespace rive;

static NoOpFactory s_factory;
static rcp<File> s_file;
static bool s_scroll = false;

extern "C"
{
    EMSCRIPTEN_KEEPALIVE int harness_import(const uint8_t* bytes, uint32_t size)
    {
        s_file = File::import(Span<const uint8_t>(bytes, size), &s_factory);
        return s_file != nullptr;
    }

    // Returns how many script VMs are running.
    EMSCRIPTEN_KEEPALIVE int harness_run(int frames)
    {
        s_file->startScripts();
        printf("script vms kept: %zu\n", s_file->wasmVMs().size());
        int running = 0;
        for (auto& vm : s_file->wasmVMs())
        {
            running += vm->valid() ? 1 : 0;
        }
        auto artboard = s_file->artboardDefault();
        if (artboard == nullptr)
        {
            return -1;
        }
        auto machine = artboard->defaultStateMachine();
        auto data = s_file->createDefaultViewModelInstance(artboard.get());
        if (machine != nullptr && data != nullptr)
        {
            machine->bindViewModelInstance(data);
        }
        NoOpRenderer renderer;
        for (int i = 0; i < frames; i++)
        {
            if (machine != nullptr)
            {
                machine->advanceAndApply(1.0f / 60.0f);
            }
            else
            {
                artboard->advance(1.0f / 60.0f);
            }
            artboard->draw(&renderer);
            s_file->frameBoundary();
        }
        printf("artboard drew %d frames\n", frames);
        if (s_scroll && machine != nullptr)
        {
            ScrollEvent event;
            event.delta = Vec2D(0.0f, -30.0f);
            event.phase = ScrollPhase::update;
            event.precise = true;
            HitResult hit =
                machine->pointerScroll(Vec2D(50.0f, 60.0f), event, 1.5f, 2);
            printf("scroll %s\n",
                   hit != HitResult::none ? "claimed" : "declined");
        }
        int alive = 0;
        for (auto& vm : s_file->wasmVMs())
        {
            alive += vm->valid() ? 1 : 0;
            if (!vm->lastTrap().message.empty())
            {
                printf("last trap: %s\n", vm->lastTrap().message.c_str());
            }
        }
        printf("script vms alive after the frames: %d\n", alive);
        return running;
    }

    // Runs frames with only this much stack left, as deeply nested script
    // and host calls would.
    EMSCRIPTEN_KEEPALIVE int harness_run_low_on_stack(int frames, int left)
    {
        if (emscripten_stack_get_free() <= (size_t)left)
        {
            return harness_run(frames);
        }
        volatile char pad[1024];
        pad[0] = 0;
        return harness_run_low_on_stack(frames, left) + pad[0];
    }

    // Sends one scroll over the artboard after the frames.
    EMSCRIPTEN_KEEPALIVE void harness_scroll() { s_scroll = true; }

    EMSCRIPTEN_KEEPALIVE uintptr_t harness_stack()
    {
        return emscripten_stack_get_current();
    }

    EMSCRIPTEN_KEEPALIVE void harness_release() { s_file = nullptr; }
}
