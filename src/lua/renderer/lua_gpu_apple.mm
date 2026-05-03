// On Apple platforms, ore_context.hpp imports <Metal/Metal.h> (via the
// globally-defined ORE_BACKEND_METAL), which is only valid in ObjC++ files.
// This wrapper compiles lua_gpu.cpp as ObjC++ on macOS/iOS.
#include "lua_gpu.cpp"
