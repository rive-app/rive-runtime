#pragma once

// Emscripten/WASM compatibility shim for microprofile.
// microprofile.h only defines MP_TICK, MP_BREAK, etc. for __APPLE__, _WIN32,
// and __linux__. Emscripten supports the same POSIX APIs as Linux so we
// replicate the Linux platform block here and include this header before
// microprofile.h.

#if defined(__EMSCRIPTEN__) && !defined(MP_TICK)

#include <stdint.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

inline int64_t MicroProfileTicksPerSecondCpu() { return 1000000000ll; }

inline int64_t MicroProfileGetTick()
{
    // Use a MONOTONIC clock, not CLOCK_REALTIME. MicroProfile treats ticks as a
    // monotonic relative counter (native builds use rdtsc / mach_absolute_time)
    // and derives frame durations from tick deltas. CLOCK_REALTIME is the
    // absolute wall clock (emscripten maps it to Date.now()): it can jump
    // (NTP, sleep/wake, background-tab throttling pausing the render loop) and
    // its epoch-scale magnitude turns any stale frame-start slot into an
    // astronomically large duration. CLOCK_MONOTONIC maps to performance.now(),
    // which is monotonic, high-resolution under cross-origin isolation, and
    // relative to page load.
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return 1000000000ll * ts.tv_sec + ts.tv_nsec;
}

#define MP_TICK() MicroProfileGetTick()
#define MP_BREAK() __builtin_trap()
#define MP_THREAD_LOCAL __thread
#define MP_STRCASECMP strcasecmp
#define MP_GETCURRENTTHREADID() (uint64_t)pthread_self()
typedef uint64_t MicroProfileThreadIdType;

#endif // __EMSCRIPTEN__ && !MP_TICK
