// A universal build compiles once for every arch slice, so each slice picks
// its own relocations and the target name its AOT artifacts must carry.
#if defined(__aarch64__)
#define BUILD_TARGET "AARCH64"
#include "aot_reloc_aarch64.c"
#else
#define BUILD_TARGET "X86_64"
#include "aot_reloc_x86_64.c"
#endif
