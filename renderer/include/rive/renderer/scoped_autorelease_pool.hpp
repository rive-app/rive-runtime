/*
 * Copyright 2026 Rive
 */

#pragma once

#ifdef __APPLE__
extern "C"
{
    void* objc_autoreleasePoolPush(void);
    void objc_autoreleasePoolPop(void*);
}
#endif

namespace rive::gpu
{

// Drains ObjC autoreleased objects (drawables, command buffers) at scope
// end. Threads without a run loop never drain otherwise, so any thread that
// drives frames must hold one per iteration. No-op off Apple.
class ScopedAutoreleasePool
{
public:
#ifdef __APPLE__
    ScopedAutoreleasePool() : m_pool(objc_autoreleasePoolPush()) {}
    ~ScopedAutoreleasePool() { objc_autoreleasePoolPop(m_pool); }
#else
    ScopedAutoreleasePool() {}
    ~ScopedAutoreleasePool() {}
#endif
    ScopedAutoreleasePool(const ScopedAutoreleasePool&) = delete;
    ScopedAutoreleasePool& operator=(const ScopedAutoreleasePool&) = delete;

#ifdef __APPLE__
private:
    void* m_pool;
#endif
};

} // namespace rive::gpu
