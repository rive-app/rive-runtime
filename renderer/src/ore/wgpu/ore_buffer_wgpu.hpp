#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <webgpu/webgpu_cpp.h>

namespace rive::ore
{
class ContextWGPU;

class BufferWGPU : public LITE_RTTI_OVERRIDE(Buffer, BufferWGPU)
{
public:
    BufferWGPU(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferWGPU() override = default; // wgpu::Buffer RAII destructor releases
    void update(const void* data, uint32_t size, uint32_t offset) override;

private:
    friend class ContextWGPU;
    friend class RenderPassWGPU;
    wgpu::Buffer m_wgpuBuffer;
    wgpu::Queue m_wgpuQueue; // Weak ref (addref'd copy).
};
} // namespace rive::ore
