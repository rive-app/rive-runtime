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
#include <fstream>
#include <vector>

#ifndef _WIN32
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

int main(int argc, char** argv)
{
    installFaultHandler();
    if (argc < 2)
    {
        fprintf(stderr, "usage: wasm_gc_bench <file.riv> [frames]\n");
        return 1;
    }
    int frames = argc > 2 ? atoi(argv[2]) : 600;
    if (frames <= 0)
    {
        fprintf(stderr, "frame count must be positive\n");
        return 1;
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

    // Load-time boundary: init's survivors promote here, during load, so
    // the first presented frame never pays for them.
    auto loadCollect = Clock::now();
    if (const char* notice = file->frameBoundary())
    {
        printf("gcbench notice: %s\n", notice);
    }
    printf("gcbench post-init collect: %.1fms\n", msSince(loadCollect));

    std::vector<double> frameMs;
    frameMs.reserve(frames);
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
        if (const char* notice = file->frameBoundary())
        {
            printf("gcbench notice: %s\n", notice);
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
    printf("gcbench memory: %u -> %u wasm pages\n", pagesBefore, pages());
    printf("gcbench handles: %u live, %u peak\n", live(), livePeak);
    return 0;
}
