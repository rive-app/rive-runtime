/*
 * Copyright 2026 Rive
 *
 * Regression test for the deferred playback thread drawable leak (PR #13818).
 * A bare std::thread never drains ObjC autoreleased objects, so per-frame
 * drawables and command buffers accumulated until CAMetalLayer's allocation
 * failed and nextDrawable returned nil. Every deferred playback thread now
 * holds a ScopedAutoreleasePool per replayed frame. CoreFoundation objects
 * participate in the same pools, so the invariant is pinned here without
 * Metal: an autorelease inside the pool resolves when the pool pops, not at
 * thread exit.
 */

#include "rive/renderer/scoped_autorelease_pool.hpp"
#include <catch.hpp>

#ifdef __APPLE__

#include <CoreFoundation/CoreFoundation.h>
#include <thread>

TEST_CASE("pool drains autoreleases on a bare thread",
          "[ScopedAutoreleasePool]")
{
    // A mutable string is always heap allocated, never a tagged pointer, so
    // its retain count is real.
    CFMutableStringRef obj = CFStringCreateMutable(kCFAllocatorDefault, 0);
    REQUIRE(CFGetRetainCount(obj) == 1);
    std::thread([obj] {
        {
            rive::gpu::ScopedAutoreleasePool pool;
            CFAutorelease(CFRetain(obj));
            CHECK(CFGetRetainCount(obj) == 2);
        }
        CHECK(CFGetRetainCount(obj) == 1);
    }).join();
    CHECK(CFGetRetainCount(obj) == 1);
    CFRelease(obj);
}

TEST_CASE("pools nest", "[ScopedAutoreleasePool]")
{
    CFMutableStringRef obj = CFStringCreateMutable(kCFAllocatorDefault, 0);
    std::thread([obj] {
        rive::gpu::ScopedAutoreleasePool outer;
        CFAutorelease(CFRetain(obj));
        {
            rive::gpu::ScopedAutoreleasePool inner;
            CFAutorelease(CFRetain(obj));
            CHECK(CFGetRetainCount(obj) == 3);
        }
        CHECK(CFGetRetainCount(obj) == 2);
    }).join();
    CHECK(CFGetRetainCount(obj) == 1);
    CFRelease(obj);
}

#endif // __APPLE__
