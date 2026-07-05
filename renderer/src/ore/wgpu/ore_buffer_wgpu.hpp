#pragma once
#include "rive/renderer/ore/ore_buffer.hpp"
#include <webgpu/webgpu_cpp.h>
#include <vector>

namespace rive::ore
{
class ContextWGPU;

// An update after a bind orphans onto a fresh backing so the GPU keeps reading
// the value it was bound with. WriteBuffer copies are queued ahead of the frame
// submit, so without this both draws in a frame would read the last write.
// Never rebound buffers stay at one backing.
class BufferWGPU : public LITE_RTTI_OVERRIDE(Buffer, BufferWGPU)
{
public:
    BufferWGPU(uint32_t size, BufferUsage usage) :
        lite_rtti_override(size, usage)
    {}
    ~BufferWGPU() override = default; // wgpu::Buffer RAII destructor releases

    void update(const void* data, uint32_t size, uint32_t offset) override;

    // Backing to bind right now.
    const wgpu::Buffer& current() const
    {
        return m_pool[m_currentIndex].buffer;
    }

    // Tag the current backing with this frame's serial so a later update()
    // orphans instead of overwriting in-flight memory.
    void markBound();

private:
    friend class ContextWGPU;
    friend class RenderPassWGPU;

    struct Backing
    {
        wgpu::Buffer buffer;
        uint64_t frameStamp = 0; // serial it was last bound in
    };

    // Move to a fresh backing. Cross frame reuse is safe because the new
    // WriteBuffer is ordered after that frame submit, so only a backing bound
    // this frame is skipped. Returns false if a fresh backing could not be
    // allocated, in which case the current backing is kept.
    bool acquireFreshBacking();

    std::vector<Backing> m_pool;
    size_t m_currentIndex = 0;
    bool m_boundSinceUpdate = false;

    // Lazily allocated CPU mirror of the contents. An orphan rewrites the whole
    // fresh backing from it, since WGPU buffers are not CPU readable and we
    // cannot copy the old backing forward the way Metal and Vulkan do.
    std::vector<uint8_t> m_shadow;

    wgpu::BufferUsage m_wgpuUsage{};
    wgpu::Device m_wgpuDevice;
    wgpu::Queue m_wgpuQueue;
    ContextWGPU* m_ctx = nullptr;
};
} // namespace rive::ore
