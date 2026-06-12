#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"

namespace rive::ore
{
class ContextGL;
class RenderPassGL;

class BufferGL : public LITE_RTTI_OVERRIDE(Buffer, BufferGL)
{
public:
    BufferGL(uint32_t size, BufferUsage usage) : lite_rtti_override(size, usage)
    {}
    ~BufferGL() override;
    void update(const void* data, uint32_t size, uint32_t offset) override;

    friend class ContextGL;
    friend class RenderPassGL;
    unsigned int m_glBuffer = 0;
    unsigned int m_glTarget = 0;
};
} // namespace rive::ore
