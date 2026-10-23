/*
 * Copyright 2026 Rive
 */

#pragma once

// Written as calls, so testing one without this header fails to build.

// Whether no other thread can reach librive's memory: a web build with no
// -pthread, wasm workers or -sSHARED_MEMORY to share its heap.
#if defined(__EMSCRIPTEN__) && !defined(__EMSCRIPTEN_SHARED_MEMORY__)
#define RIVE_ONE_THREAD() 1
#else
#define RIVE_ONE_THREAD() 0
#endif

// Whether the job and worker pools start threads, which the web can only with
// pthreads.
#if !defined(__EMSCRIPTEN__) || defined(__EMSCRIPTEN_PTHREADS__)
#define RIVE_POOL_THREADS() 1
#else
#define RIVE_POOL_THREADS() 0
#endif

#if RIVE_POOL_THREADS() && RIVE_ONE_THREAD()
#error "pthreads need a shared heap"
#endif
