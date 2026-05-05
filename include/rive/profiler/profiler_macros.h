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
//
// Level-gated variants:
// RIVE_PROF_SCOPE_L(level)
// RIVE_PROF_SCOPENAME_L(level, name)
// RIVE_PROF_GPUNAME_L(level, name)
//
// `level` MUST be a literal integer 0..4. Lower numbers mark higher-level
// functions (frame, scene, flush). Higher numbers mark hot inner-loop
// functions whose per-call overhead matters. The compile-time threshold
// `RIVE_PROF_LEVEL_MAX` drops every marker with `level > RIVE_PROF_LEVEL_MAX`
// to nothing — no token registration, no Enter/Leave call, no stack handler.
// Pass it via the build system, e.g. `-DRIVE_PROF_LEVEL_MAX=2`.
//
// The non-level macros (RIVE_PROF_SCOPE, RIVE_PROF_SCOPENAME,
// RIVE_PROF_GPUNAME) are equivalent to level 0 and are always on whenever the
// active profiler is compiled in.

#ifndef RIVE_PROF_LEVEL_MAX
#define RIVE_PROF_LEVEL_MAX 2
#endif

// Per-level pass-through gates. Each either expands its argument (when the
// level is <= RIVE_PROF_LEVEL_MAX) or drops it.
#if RIVE_PROF_LEVEL_MAX >= 0
#define _RIVE_PROF_LVL_0(body) body
#else
#define _RIVE_PROF_LVL_0(body)
#endif

#if RIVE_PROF_LEVEL_MAX >= 1
#define _RIVE_PROF_LVL_1(body) body
#else
#define _RIVE_PROF_LVL_1(body)
#endif

#if RIVE_PROF_LEVEL_MAX >= 2
#define _RIVE_PROF_LVL_2(body) body
#else
#define _RIVE_PROF_LVL_2(body)
#endif

#if RIVE_PROF_LEVEL_MAX >= 3
#define _RIVE_PROF_LVL_3(body) body
#else
#define _RIVE_PROF_LVL_3(body)
#endif

#if RIVE_PROF_LEVEL_MAX >= 4
#define _RIVE_PROF_LVL_4(body) body
#else
#define _RIVE_PROF_LVL_4(body)
#endif

// Dispatch via token paste so `level` works as a literal integer.
#define _RIVE_PROF_LVL_DISPATCH(level, body) _RIVE_PROF_LVL_##level(body)
#define _RIVE_PROF_LVL(level, body) _RIVE_PROF_LVL_DISPATCH(level, body)

#if defined(RIVE_OPTICK) // Optick integration

#include "optick_core.h"
#include "optick.h"
#define RIVE_PROF_INIT()
#define RIVE_PROF_FRAME() OPTICK_FRAME("RiveFrame")
#define RIVE_PROF_SCOPE() OPTICK_EVENT()
#define RIVE_PROF_SCOPENAME(name) OPTICK_EVENT(name)
#define RIVE_PROF_GPUNAME(name)

#define RIVE_PROF_SCOPE_L(level) _RIVE_PROF_LVL(level, OPTICK_EVENT();)
#define RIVE_PROF_SCOPENAME_L(level, name)                                     \
    _RIVE_PROF_LVL(level, OPTICK_EVENT(name);)
#define RIVE_PROF_GPUNAME_L(level, name) _RIVE_PROF_LVL(level, /* unused */)

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

#define RIVE_PROF_SCOPE_L(level)                                               \
    _RIVE_PROF_LVL(level,                                                      \
                   MICROPROFILE_SCOPEI(__FILE__, __FUNCTION__, 0xffffffff);)
#define RIVE_PROF_SCOPENAME_L(level, name)                                     \
    _RIVE_PROF_LVL(level, MICROPROFILE_SCOPEI("group", name, 0xffffffff);)
#define RIVE_PROF_GPUNAME_L(level, name)                                       \
    _RIVE_PROF_LVL(level, MICROPROFILE_SCOPEGPUI(name, 0xffffffff);)

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
#define RIVE_PROF_SCOPE_L(level)
#define RIVE_PROF_SCOPENAME_L(level, name)
#define RIVE_PROF_GPUNAME_L(level, name)
#define RIVE_PROF_TAG(cat, tag)
#define RIVE_PROF_THREAD(name)
#define RIVE_PROF_DRAW()
#define RIVE_PROF_TOGGLEDRAW()
#define RIVE_PROF_GPUSUBMIT(i)
#define RIVE_PROF_GPUFLIP()
#define RIVE_PROF_ENDFRAME()

#endif
