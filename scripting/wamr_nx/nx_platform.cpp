/*
 * Copyright 2026 Rive
 */

// The half of WAMR's platform layer that the Nintendo SDK does not cover.
// nnSdk exports no mmap family, so mapping goes through nn::os virtual
// address memory: AllocateAddressRegion reserves, AllocateMemoryPages commits,
// SetMemoryPermission is mprotect.

#include "platform_api_vmcore.h"

#include <nn/nn_Result.h>
#include <nn/os.h>
#include <nn/os/os_MemoryHeapCommon.h>
#include <nn/os/os_MemoryPermission.h>
#include <nn/os/os_VirtualAddressMemoryApi.h>

#include <malloc.h>
#include <string.h>

// Virtual address memory is opt-in per application. Without it the mapping
// calls all fail, so fall back to aligned allocations from the app heap:
// software bounds checks mean nothing here needs real page permissions.
static bool s_virtualAddressMemory = false;

static size_t roundUpToPage(size_t size)
{
    const size_t page = nn::os::MemoryPageSize;
    return (size + page - 1) & ~(page - 1);
}

static nn::os::MemoryPermission toPermission(int prot)
{
    if (prot & MMAP_PROT_WRITE)
    {
        return nn::os::MemoryPermission_ReadWrite;
    }
    if (prot & MMAP_PROT_READ)
    {
        return nn::os::MemoryPermission_ReadOnly;
    }
    return nn::os::MemoryPermission_None;
}

extern "C"
{

    int bh_platform_init()
    {
        s_virtualAddressMemory = nn::os::IsVirtualAddressMemoryEnabled();
        fprintf(stderr,
                "nx wamr: virtual address memory %s\n",
                s_virtualAddressMemory ? "enabled"
                                       : "disabled (heap fallback)");
        return 0;
    }

    void bh_platform_destroy() {}

    int os_printf(const char* format, ...)
    {
        va_list ap;
        va_start(ap, format);
        int ret = vprintf(format, ap);
        va_end(ap);
        return ret;
    }

    int os_vprintf(const char* format, va_list ap)
    {
        return vprintf(format, ap);
    }

    int os_getpagesize(void) { return (int)nn::os::MemoryPageSize; }

    // WAMR's shared posix thread code calls the libc spelling directly.
    int getpagesize(void) { return (int)nn::os::MemoryPageSize; }

    void* os_mmap(void* hint,
                  size_t size,
                  int prot,
                  int flags,
                  os_file_handle file)
    {
        (void)hint;
        (void)flags;
        (void)file;

        size_t requestSize = roundUpToPage(size);
        if (requestSize < size)
        {
            return NULL;
        }

        if (!s_virtualAddressMemory)
        {
            void* p = memalign(nn::os::MemoryPageSize, requestSize);
            if (p != NULL)
            {
                // mmap semantics: callers, wasm linear memory above all,
                // depend on fresh pages reading zero.
                memset(p, 0, requestSize);
            }
            if (p == NULL)
            {
                fprintf(stderr,
                        "nx wamr: memalign failed for %zu bytes\n",
                        requestSize);
            }
            return p;
        }

        uintptr_t address = 0;
        if (nn::os::AllocateAddressRegion(&address, requestSize).IsFailure())
        {
            fprintf(stderr,
                    "nx wamr: AllocateAddressRegion failed for %zu bytes\n",
                    requestSize);
            return NULL;
        }
        // Committing the whole reservation is right because software bounds
        // checks make WAMR reserve only what it maps.
        if (nn::os::AllocateMemoryPages(address, requestSize).IsFailure())
        {
            fprintf(stderr,
                    "nx wamr: AllocateMemoryPages failed for %zu bytes\n",
                    requestSize);
            static_cast<void>(nn::os::FreeAddressRegion(address));
            return NULL;
        }
        nn::os::SetMemoryPermission(address, requestSize, toPermission(prot));
        return (void*)address;
    }

    void os_munmap(void* addr, size_t size)
    {
        if (addr == NULL)
        {
            return;
        }
        if (!s_virtualAddressMemory)
        {
            free(addr);
            return;
        }

        uintptr_t address = (uintptr_t)addr;
        size_t requestSize = roundUpToPage(size);
        // Permissions must come back before the pages can be released.
        nn::os::SetMemoryPermission(address,
                                    requestSize,
                                    nn::os::MemoryPermission_ReadWrite);
        static_cast<void>(nn::os::FreeMemoryPages(address, requestSize));
        static_cast<void>(nn::os::FreeAddressRegion(address));
    }

    int os_mprotect(void* addr, size_t size, int prot)
    {
        if (addr == NULL || !s_virtualAddressMemory)
        {
            return 0;
        }
        nn::os::SetMemoryPermission((uintptr_t)addr,
                                    roundUpToPage(size),
                                    toPermission(prot));
        return 0;
    }

    // No executable mapping exists on this platform, so nothing ever needs the
    // instruction stream made coherent.
    void os_dcache_flush(void) {}

    void os_icache_flush(void* start, size_t len)
    {
        (void)start;
        (void)len;
    }
}
