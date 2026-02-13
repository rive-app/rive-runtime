#ifdef RIVE_MICROPROFILE
#define MICROPROFILE_IMPL
#define MICROPROFILE_WEBSERVER 0
#if defined(RIVE_WINDOWS)
#define MICROPROFILE_GPU_TIMERS_D3D11 1
#define MICROPROFILE_GPU_TIMERS_D3D12 1
#endif
#include "rive/profiler/microprofile_emscripten.h"
#ifdef __EMSCRIPTEN__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wformat"
#endif
#include "microprofile.h"
#ifdef __EMSCRIPTEN__
#pragma clang diagnostic pop
#endif
#endif
