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
#if defined(__APPLE__)
#include <mach/mach.h>
#include <mach/vm_page_size.h>
#endif

namespace rive
{
#if defined(__APPLE__)
// Pages mapped over stay in a memory object that other mappings still hold,
// so they are first marked for the system to take.
static void markReusable(uintptr_t lo, uintptr_t hi)
{
  // A translated process maps smaller pages than the kernel's, and in such
  // a map the kernel marks none.
  if (vm_page_size < vm_kernel_page_size)
  {
    return;
  }
  for (vm_address_t at = lo; at < hi;)
  {
    vm_address_t start = at;
    vm_size_t size = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t count = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t name = MACH_PORT_NULL;
    if (vm_region_64(
            mach_task_self(),
            &start,
            &size,
            VM_REGION_BASIC_INFO_64,
            (vm_region_info_t)&info,
            &count,
            &name) != KERN_SUCCESS ||
        start >= hi)
    {
      return;
    }
    vm_address_t end = start + size;
    // A private mapping replaced whole takes its memory object with it.
    if (info.shared || start < lo || end > hi)
    {
      // Older systems mark pages only in an object with one mapping or a shared
      // one, and a move shadows a split private one, so share it while whole.
      memory_object_size_t shared = size;
      mach_port_t entry = MACH_PORT_NULL;
      if (!info.shared && mach_make_memory_entry_64(
                              mach_task_self(),
                              &shared,
                              start,
                              VM_PROT_READ | VM_PROT_WRITE | MAP_MEM_VM_SHARE,
                              &entry,
                              MACH_PORT_NULL) == KERN_SUCCESS)
      {
        mach_port_deallocate(mach_task_self(), entry);
      }
      vm_address_t from = start > lo ? start : lo;
      madvise((void*)from, (end < hi ? end : hi) - from, MADV_FREE_REUSABLE);
    }
    at = end;
  }
}
#endif

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
  // Darwin's madvise leaves reused pages uncharged and unzeroed, so a fresh
  // mapping replaces them.
  markReusable(lo, hi);
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
