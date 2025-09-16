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
#define RIVE_PROF_FRAME() OPTICK_FRAME("RiveFrame")
#define RIVE_PROF_SCOPE() OPTICK_EVENT()
#define RIVE_PROF_SCOPENAME(name) OPTICK_EVENT(name)
#define RIVE_PROF_TAG(cat, tag) OPTICK_TAG(cat, tag)
#define RIVE_PROF_THREAD(name) OPTICK_THREAD(name)
#else // No profiler selected - fallback to no-op
#define RIVE_PROF_FRAME()
#define RIVE_PROF_SCOPE()
#define RIVE_PROF_SCOPENAME(name)
#define RIVE_PROF_TAG(cat, tag)
#define RIVE_PROF_THREAD(name)
#endif
