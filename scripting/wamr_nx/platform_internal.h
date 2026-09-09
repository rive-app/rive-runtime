/*
 * Copyright 2026 Rive
 */

// WAMR platform layer for the Nintendo consoles. The SDK's libc is musl
// derived and nnSdk exports the pthread, clock, file and socket families, so
// everything here defers to WAMR's shared posix layer except memory mapping,
// which nnSdk does not implement at all.

#ifndef _PLATFORM_INTERNAL_H
#define _PLATFORM_INTERNAL_H

#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C"
{
#endif

#ifndef BH_PLATFORM_NX
#define BH_PLATFORM_NX
#endif

// No dlopen in the sandbox; modules arrive as bytes.
#define BH_HAS_DLFCN 0

#define BH_APPLET_PRESERVED_STACK_SIZE (32 * 1024)

#define BH_THREAD_DEFAULT_PRIORITY 0

    typedef pthread_t korp_tid;
    typedef pthread_mutex_t korp_mutex;
    typedef pthread_cond_t korp_cond;
    typedef pthread_t korp_thread;
    typedef pthread_rwlock_t korp_rwlock;
    typedef sem_t korp_sem;

#define OS_THREAD_MUTEX_INITIALIZER PTHREAD_MUTEX_INITIALIZER

#define os_thread_local_attribute __thread

#define bh_socket_t int

    // OS_ENABLE_HW_BOUND_CHECK stays undefined: nnSdk exports no signal API, so
    // the trap based bounds checks cannot be serviced. Artifacts must be built
    // with software bounds checks.

    // nnSdk has no getpagesize; nx_platform.cpp defines both this and the libc
    // spelling that WAMR's shared posix thread code calls directly.
    int os_getpagesize(void);

    typedef int os_file_handle;
    typedef DIR* os_dir_stream;
    typedef int os_raw_file_handle;

    static inline os_file_handle os_get_invalid_handle(void) { return -1; }

#ifdef __cplusplus
}
#endif

#endif /* end of _PLATFORM_INTERNAL_H */
