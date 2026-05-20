#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;
class RenderPassMetal;

class BufferMetal : public LITE_RTTI_OVERRIDE(Buffer, BufferMetal)
{
public:
    BufferMetal(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferMetal() override = default; // ARC releases m_mtlBuffer
    void update(const void* data, uint32_t size, uint32_t offset) override;

private:
    friend class ContextMetal;
    friend class RenderPassMetal;
    id<MTLBuffer> m_mtlBuffer = nil;
};
} // namespace rive::ore
