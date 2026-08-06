/*
 * Copyright 2024 Rive
 */

#pragma once

#include "common/testing_window.hpp"

// Interface for a test tool that renders one frame per call.
//
// A tool implementing FrameRunner never owns a loop of its own. The host
// parses, inits, and then pumps doFrame() at whatever cadence it likes. The
// standalone tools drive it from a for(;;) in main(). Unreal creates a runner
// from its "-rivetool=" argument and calls doFrame() once per engine tick, so
// its game thread keeps ticking while the test runs.
class FrameRunner
{
public:
    // Everything parseArgs() discovers that the host needs *before* init().
    // Because it has to exist before there is a window to init against. Hosts
    // that bring their own window (Unreal) ignore these.
    struct LaunchOptions
    {
        TestingWindow::Backend backend =
#ifdef __APPLE__
            TestingWindow::Backend::metal;
#else
            TestingWindow::Backend::vk;
#endif
        TestingWindow::BackendParams backendParams;
        TestingWindow::Visibility visibility =
            TestingWindow::Visibility::fullscreen;
    };

    virtual ~FrameRunner() = default;

    // Whether doFrame() draws into whatever target the host hands it (true), or
    // sizes and owns targets of its own (false).
    //
    // The gms are the latter. each one renders at its own size and reads the
    // pixels straight back, so it needs a frame it fully controls. A host that
    // draws through a render graph (unreal) has to keep those tools out of its
    // graph and tick them on their own instead.
    virtual bool rendersIntoHostTarget() const { return true; }

    // Parses the tool's command line. Returns false if the arguments don't
    // describe a runnable test (no .riv, for goldens); it is up to the host to
    // decide whether that is fatal. Called before there is a TestingWindow.
    virtual bool parseArgs(int argc,
                           const char* const argv[],
                           LaunchOptions& options) = 0;

    // One-time setup. Requires a live TestingWindow.
    virtual void init() = 0;

    // Advances and draws a single frame. Returns false once the runner is
    // finished (or has been asked to quit), after which the host is responsible
    // for tearing everything down. Afterwhich doFrame() must not be called
    // again.
    virtual bool doFrame() = 0;
};
