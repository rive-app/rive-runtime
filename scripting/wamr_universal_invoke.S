// A universal build assembles once for every arch slice, so each slice picks
// its own invoke shim.
#if defined(__aarch64__)
#include "invokeNative_aarch64_simd.s"
#else
#include "invokeNative_em64_simd.s"
#endif
