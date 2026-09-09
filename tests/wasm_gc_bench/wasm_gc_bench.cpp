/*
 * Copyright 2026 Rive
 */

// Headless on-device harness for the wasm scripting collector lanes: loads a
// baked .riv, advances frames through the host frame boundary, and reports
// timing, wasm memory, and live handle counts. No GPU; recording goes into a
// deferred session that is never replayed.

#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/wasm/wasm_scripting_vm.hpp"
#include "utils/no_op_renderer.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <vector>

// nnSdk exports no signal API at all, so consoles get the no-op handler.
#if !defined(_WIN32) && !defined(RIVE_NX)
#include <csignal>

// SIGILL diagnostics for the device AOT lane: adb shell has no tombstone
// access, so print the fault address before dying.
static void onFault(int sig, siginfo_t* info, void*)
{
    fprintf(stderr, "gcbench fault: signal %d at %p\n", sig, info->si_addr);
    fflush(stderr);
    signal(sig, SIG_DFL);
    raise(sig);
}

static void installFaultHandler()
{
    struct sigaction action = {};
    action.sa_sigaction = onFault;
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGILL, &action, nullptr);
    sigaction(SIGSEGV, &action, nullptr);
    sigaction(SIGBUS, &action, nullptr);
}
#else
static void installFaultHandler() {}
#endif

using Clock = std::chrono::steady_clock;

static double msSince(Clock::time_point start)
{
    return std::chrono::duration<double, std::milli>(Clock::now() - start)
        .count();
}

#ifdef EXTERN_TOOLS
// libnnSdk owns main on the consoles; nx_main_wasm_gc_bench.cpp bridges in.
#define main rive_main
#endif

int main(int argc, char** argv)
{
    installFaultHandler();
    if (argc < 2)
    {
        fprintf(stderr, "usage: wasm_gc_bench <file.riv> [frames]\n");
        return 1;
    }
    // atoi folds malformed input to 0, which is a meaningful collector
    // setting; a validation lane must reject what it cannot parse.
    auto parseInt = [](const char* text, int* out) {
        char* end = nullptr;
        errno = 0;
        long value = strtol(text, &end, 10);
        if (errno != 0 || end == text || *end != '\0' || value < INT_MIN ||
            value > INT_MAX)
        {
            return false;
        }
        *out = (int)value;
        return true;
    };
    // The frame count is optional, so flags may start at argv[2].
    int frames = 600;
    int flagStart = 2;
    if (argc > 2 && argv[2][0] != '-')
    {
        flagStart = 3;
        if (!parseInt(argv[2], &frames) || frames <= 0)
        {
            fprintf(stderr, "frame count must be a positive integer\n");
            return 1;
        }
    }
    // Consoles have no environment, so the debug knobs ride the command
    // line: --verify runs the heap verifier after every boundary,
    // --major-budget / --collect-mode feed the collector's tuning exports.
    bool verify = false;
    int majorBudget = -1;
    int collectMode = -1;
    for (int i = flagStart; i < argc; i++)
    {
        if (strcmp(argv[i], "--verify") == 0)
        {
            verify = true;
        }
        else if (strncmp(argv[i], "--major-budget=", 15) == 0)
        {
            if (!parseInt(argv[i] + 15, &majorBudget) || majorBudget < 0)
            {
                fprintf(stderr,
                        "invalid --major-budget value '%s'\n",
                        argv[i] + 15);
                return 1;
            }
        }
        else if (strncmp(argv[i], "--collect-mode=", 15) == 0)
        {
            // Modes: 0 automatic, 1 copying, 2 sliced; anything else would
            // silently run automatic.
            if (!parseInt(argv[i] + 15, &collectMode) || collectMode < 0 ||
                collectMode > 2)
            {
                fprintf(stderr,
                        "invalid --collect-mode value '%s'\n",
                        argv[i] + 15);
                return 1;
            }
        }
        else
        {
            // A typoed flag silently skipping verification would defeat
            // the lane's purpose.
            fprintf(stderr, "unknown argument '%s'\n", argv[i]);
            return 1;
        }
    }

    std::ifstream in(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                               std::istreambuf_iterator<char>());
    if (bytes.empty())
    {
        fprintf(stderr, "cannot read %s\n", argv[1]);
        return 1;
    }

    rive::cmd::DeferredSession session(rive::ore::ReplayCaps{});
    rive::ImportResult result;
    auto importStart = Clock::now();
    auto file = rive::File::import(
        rive::Span<const uint8_t>(bytes.data(), (size_t)bytes.size()),
        &session,
        &result);
    double importMs = msSince(importStart);
    if (result != rive::ImportResult::success || file == nullptr)
    {
        fprintf(stderr, "import failed\n");
        return 1;
    }
    rive::WasmScriptingVM* vm = file->wasmScriptingVM();
    if (vm == nullptr)
    {
        fprintf(stderr, "no wasm scripting vm in this file\n");
        return 1;
    }
    // Multi-module files run one VM per module; boundaries and metrics
    // must cover them all.
    auto live = [&]() {
        uint32_t count = 0;
        for (auto& moduleVm : file->wasmVMs())
        {
            count += (uint32_t)(moduleVm->handles().slots.size() -
                                moduleVm->handles().freeSlots.size());
        }
        return count;
    };
    auto pages = [&]() {
        uint32_t count = 0;
        for (auto& moduleVm : file->wasmVMs())
        {
            count += moduleVm->memoryPages();
        }
        return count;
    };
    printf("gcbench import: %.1fms, %u wasm pages\n", importMs, pages());

    auto instance = file->artboardDefault();
    if (instance == nullptr)
    {
        fprintf(stderr, "no default artboard\n");
        return 1;
    }
    // Scripted content runs through the state machine when one exists.
    auto machine = instance->defaultStateMachine();
    printf("gcbench driving: %s\n",
           machine != nullptr ? "state machine" : "artboard");

    using CallOutcome = rive::WasmScriptingVM::CallOutcome;
    // Knobs and the verifier apply to every module vm in the file.
    auto applyKnob = [&](const char* name, int value) {
        uint32_t args[1] = {(uint32_t)value};
        // Individual modules may omit the export (a stub-runtime module
        // beside a frame one), but a flag nothing accepted would report a
        // configuration that never applied.
        int applied = 0;
        for (auto& moduleVm : file->wasmVMs())
        {
            uint32_t ignored = 0;
            switch (moduleVm->callModuleChecked(name, 1, args, &ignored))
            {
                case CallOutcome::ok:
                    printf("gcbench %s: %d\n", name, value);
                    applied++;
                    break;
                case CallOutcome::missing:
                    printf("gcbench %s: export missing in one module\n", name);
                    break;
                case CallOutcome::trapped:
                    fprintf(stderr, "gcbench %s trapped\n", name);
                    return false;
            }
        }
        if (applied == 0)
        {
            fprintf(stderr,
                    "gcbench %s: no module exports it; flag not applied\n",
                    name);
            return false;
        }
        return true;
    };
    if (majorBudget >= 0 && !applyKnob("__riveSetMajorBudget", majorBudget))
    {
        return 1;
    }
    if (collectMode >= 0 && !applyKnob("__riveSetCollectMode", collectMode))
    {
        return 1;
    }
    // A verify run that cannot verify must fail, not report success.
    auto runVerifier = [&]() {
        for (auto& moduleVm : file->wasmVMs())
        {
            uint32_t ignored = 0;
            switch (moduleVm->callModuleChecked("__riveFrameVerify",
                                                0,
                                                nullptr,
                                                &ignored))
            {
                case CallOutcome::ok:
                    break;
                case CallOutcome::missing:
                    fprintf(stderr,
                            "gcbench verify FAILED: __riveFrameVerify export "
                            "missing\n");
                    return false;
                case CallOutcome::trapped:
                    fprintf(stderr,
                            "gcbench verify FAILED: heap verifier trapped\n");
                    return false;
            }
        }
        return true;
    };

    // Load-time boundary: init's survivors promote here, during load, so
    // the first presented frame never pays for them.
    auto loadCollect = Clock::now();
    if (const char* notice = file->frameBoundary())
    {
        printf("gcbench notice: %s\n", notice);
    }
    printf("gcbench post-init collect: %.1fms\n", msSince(loadCollect));
    if (verify && !runVerifier())
    {
        return 1;
    }

    std::vector<double> frameMs;
    frameMs.reserve(frames);
    std::vector<double> boundaryMs;
    boundaryMs.reserve(frames);
    uint32_t pagesBefore = pages();
    uint32_t livePeak = 0;
    for (int i = 0; i < frames; i++)
    {
        auto start = Clock::now();
        if (machine != nullptr)
        {
            machine->advanceAndApply(1.0f / 60);
        }
        else
        {
            instance->advance(1.0f / 60);
        }
        // The draw pass runs the scripted render callbacks; recording goes
        // nowhere but the script-side work is real.
        rive::NoOpRenderer renderer;
        instance->draw(&renderer);
        auto boundaryStart = Clock::now();
        if (const char* notice = file->frameBoundary())
        {
            printf("gcbench notice: %s\n", notice);
        }
        boundaryMs.push_back(msSince(boundaryStart));
        if (verify && !runVerifier())
        {
            return 1;
        }
        frameMs.push_back(msSince(start));
        livePeak = std::max(livePeak, live());
    }
    std::sort(frameMs.begin(), frameMs.end());
    double total = 0;
    for (double ms : frameMs)
    {
        total += ms;
    }
    printf("gcbench frames: %d, advance+boundary mean %.3fms p50 %.3fms p95 "
           "%.3fms max %.3fms\n",
           frames,
           total / frames,
           frameMs[frames / 2],
           frameMs[(size_t)(frames * 0.95)],
           frameMs.back());
    std::sort(boundaryMs.begin(), boundaryMs.end());
    printf("gcbench boundary: p50 %.1fus p95 %.1fus max %.1fus\n",
           boundaryMs[frames / 2] * 1000.0,
           boundaryMs[(size_t)(frames * 0.95)] * 1000.0,
           boundaryMs.back() * 1000.0);
    if (verify)
    {
        printf("gcbench verify: ran after every boundary\n");
    }
    printf("gcbench memory: %u -> %u wasm pages\n", pagesBefore, pages());
    printf("gcbench handles: %u live, %u peak\n", live(), livePeak);
    return 0;
}
