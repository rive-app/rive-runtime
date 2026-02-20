#pragma once

// RIVE_PROF_FRAME()
// This function should be called at the start of the main loop
//
// RIVE_PROF_SCOPE()
// Add to a function or scope to profile, the function name is
// automatically filled in
//
// RIVE_PROF_SCOPENAME(name)
// Same as above but manually supplied name
//
// RIVE_PROF_THREAD(name)
// Add to new threads

#if defined(RIVE_OPTICK) // Optick integration

#include "optick_core.h"
#include "optick.h"
#define RIVE_PROF_INIT()
#define RIVE_PROF_FRAME() OPTICK_FRAME("RiveFrame")
#define RIVE_PROF_SCOPE() OPTICK_EVENT()
#define RIVE_PROF_SCOPENAME(name) OPTICK_EVENT(name)
#define RIVE_PROF_GPUNAME(name)

#define RIVE_PROF_TAG(cat, tag) OPTICK_TAG(cat, tag)
#define RIVE_PROF_THREAD(name) OPTICK_THREAD(name)
#define RIVE_PROF_DRAW()
#define RIVE_PROF_TOGGLEDRAW()
#define RIVE_PROF_GPUSUBMIT(i)
#define RIVE_PROF_GPUFLIP()
#define RIVE_PROF_ENDFRAME()

#elif defined(RIVE_MICROPROFILE) // Microprofile integration
#include "rive/profiler/microprofile_emscripten.h"
#include "microprofile.h"
#include "microprofiledraw.h"
#include "microprofileui.h"

#define MICROPROFILE_GPU_TIMERS 1

#define RIVE_PROF_INIT()                                                       \
    MicroProfileSetEnableAllGroups(true);                                      \
    MicroProfileSetForceEnable(true);                                          \
    MicroProfileWebServerStart();                                              \
    MicroProfileOnThreadCreate("MainThread");                                  \
    MicroProfileInit();

#define RIVE_PROF_FRAME()
#define RIVE_PROF_SCOPE()                                                      \
    MICROPROFILE_SCOPEI(__FILE__, __FUNCTION__, 0xffffffff);
#define RIVE_PROF_SCOPENAME(name)                                              \
    MICROPROFILE_SCOPEI("group", name, 0xffffffff);
#define RIVE_PROF_GPUNAME(name) MICROPROFILE_SCOPEGPUI(name, 0xffffffff);
#define RIVE_PROF_TAG(cat, tag)
#define RIVE_PROF_THREAD(name)
#define RIVE_PROF_DRAW() MicroProfileDraw();
#define RIVE_PROF_TOGGLEDRAW() // MicroProfileToggleDisplayMode();
#define RIVE_PROF_GPUSUBMIT(i) MicroProfileGpuSubmit(i);
#define RIVE_PROF_GPUFLIP() // MicroProfileGpuFlip();
#define RIVE_PROF_ENDFRAME() MicroProfileFlip();

#else // No profiler selected - fallback to no-op

#define RIVE_PROF_INIT()
#define RIVE_PROF_FRAME()
#define RIVE_PROF_SCOPE()
#define RIVE_PROF_SCOPENAME(name)
#define RIVE_PROF_GPUNAME(name)
#define RIVE_PROF_TAG(cat, tag)
#define RIVE_PROF_THREAD(name)
#define RIVE_PROF_DRAW()
#define RIVE_PROF_TOGGLEDRAW()
#define RIVE_PROF_GPUSUBMIT(i)
#define RIVE_PROF_GPUFLIP()
#define RIVE_PROF_ENDFRAME()

#endif
