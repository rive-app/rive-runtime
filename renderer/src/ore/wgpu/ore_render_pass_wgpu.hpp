#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/refcnt.hpp"
#include <webgpu/webgpu_cpp.h>
#include <cstdint>

namespace rive::ore
{
class ContextWGPU;

class RenderPassWGPU : public RenderPass
{
public:
    void setPipeline(Pipeline* pipeline) override;
    void setVertexBuffer(uint32_t slot,
                         Buffer* buffer,
                         uint32_t offset = 0) override;
    void setIndexBuffer(Buffer* buffer,
                        IndexFormat format,
                        uint32_t offset = 0) override;
    void setBindGroup(uint32_t groupIndex,
                      BindGroup* bg,
                      const uint32_t* dynamicOffsets = nullptr,
                      uint32_t dynamicOffsetCount = 0) override;
    void setViewport(float x,
                     float y,
                     float width,
                     float height,
                     float minDepth = 0.0f,
                     float maxDepth = 1.0f) override;
    void setScissorRect(uint32_t x,
                        uint32_t y,
                        uint32_t width,
                        uint32_t height) override;
    void setStencilReference(uint32_t ref) override;
    void setBlendColor(float r, float g, float b, float a) override;
    void draw(uint32_t vertexCount,
              uint32_t instanceCount = 1,
              uint32_t firstVertex = 0,
              uint32_t firstInstance = 0) override;
    void drawIndexed(uint32_t indexCount,
                     uint32_t instanceCount = 1,
                     uint32_t firstIndex = 0,
                     int32_t baseVertex = 0,
                     uint32_t firstInstance = 0) override;
    void finish() override;
    void validate() const override;

    RenderPassWGPU() = default;
    RenderPassWGPU(Context* context) : RenderPass(context) {}
    ~RenderPassWGPU() override;
    RenderPassWGPU(RenderPassWGPU&& other) noexcept;
    RenderPassWGPU& operator=(RenderPassWGPU&&) noexcept;

private:
    friend class ContextWGPU;

    ContextWGPU* m_wgpuContext = nullptr; // weak ref
    rcp<Pipeline> m_currentPipeline;
    wgpu::RenderPassEncoder m_wgpuPassEncoder;
    wgpu::Buffer m_wgpuIndexBuffer;
    wgpu::IndexFormat m_wgpuIndexFormat = wgpu::IndexFormat::Uint16;
    uint32_t m_wgpuIndexOffset = 0;
};
} // namespace rive::ore
