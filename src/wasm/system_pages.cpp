/*
 * Copyright 2026 Rive
 */

#ifdef WITH_RIVE_SCRIPTING_WASM

#include "system_pages.hpp"

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#elif defined(__APPLE__) || defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace rive
{
#if defined(_WIN32) || defined(__APPLE__) || defined(__linux__)
void releaseSystemPages(uint8_t* begin, uint8_t* end)
{
#if defined(_WIN32)
  SYSTEM_INFO info;
  GetSystemInfo(&info);
  uintptr_t page = info.dwPageSize;
#else
  uintptr_t page = (uintptr_t)sysconf(_SC_PAGESIZE);
#endif
  uintptr_t lo = ((uintptr_t)begin + page - 1) & ~(page - 1);
  uintptr_t hi = (uintptr_t)end & ~(page - 1);
  if (hi <= lo)
  {
    return;
  }
  void* at = (void*)lo;
  size_t length = hi - lo;
#if defined(_WIN32)
  // Decommitting frees the pages now; committed again, they stay unbacked
  // until touched.
  VirtualFree(at, length, MEM_DECOMMIT);
  VirtualAlloc(at, length, MEM_COMMIT, PAGE_READWRITE);
#elif defined(__APPLE__)
  // Darwin's madvise leaves the pages charged to the process, so a fresh
  // mapping replaces them.
  mmap(
      at,
      length,
      PROT_READ | PROT_WRITE,
      MAP_FIXED | MAP_PRIVATE | MAP_ANON,
      -1,
      0);
#else
  madvise(at, length, MADV_DONTNEED);
#endif
}
#else
void releaseSystemPages(uint8_t*, uint8_t*) {}
#endif
} // namespace rive

#endif
