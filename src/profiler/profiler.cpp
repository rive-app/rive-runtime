#ifdef RIVE_MICROPROFILE
#define MICROPROFILE_IMPL
#if defined(RIVE_WINDOWS)
#define MICROPROFILE_GPU_TIMERS_D3D11 1
#define MICROPROFILE_GPU_TIMERS_D3D12 1
#endif
#include "microprofile.h"
#endif
