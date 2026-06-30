#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#import <Metal/Metal.h>
#include <string>
#include <vector>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;

// Pool of native backings so an update() after a bind orphans onto a fresh
// backing instead of racing the GPU still reading the bound one. Bindings
// resolve the live backing at encode time. Immutable buffers stay at one.
class BufferMetal : public LITE_RTTI_OVERRIDE(Buffer, BufferMetal)
{
public:
    BufferMetal(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferMetal() override = default; // ARC releases the backings

    void update(const void* data, uint32_t size, uint32_t offset) override;

    // Backing to bind right now.
    id<MTLBuffer> current() const { return m_pool[m_currentIndex].mtl; }

    // Tag the current backing with this frame's serial so a later update()
    // orphans instead of overwriting in-flight memory.
    void markBound();

private:
    friend class ContextMetal;
    friend class RenderPassMetal;

    struct Backing
    {
        id<MTLBuffer> mtl = nil;
        uint64_t serial = 0; // serial it was last bound in
    };

    // Move to a fresh backing, copying current contents so a partial update
    // keeps untouched bytes. The pending write's range lets a full-buffer
    // update skip the copy.
    // Returns false if a fresh backing could not be allocated, in which case
    // the current backing is kept.
    bool acquireFreshBacking(uint32_t writeOffset, uint32_t writeSize);

    std::vector<Backing> m_pool;
    size_t m_currentIndex = 0;
    bool m_boundSinceUpdate = false;
    ContextMetal* m_ctx = nullptr;
    std::string m_label;
};
} // namespace rive::ore
