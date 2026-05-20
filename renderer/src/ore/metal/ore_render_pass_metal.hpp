#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/refcnt.hpp"

#import <Metal/Metal.h>

namespace rive::ore
{
class ContextMetal;

class RenderPassMetal : public RenderPass
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

    RenderPassMetal() = default;
    RenderPassMetal(Context* context) : RenderPass(context) {}
    ~RenderPassMetal() override;
    RenderPassMetal(RenderPassMetal&& other) noexcept;
    RenderPassMetal& operator=(RenderPassMetal&&) noexcept;

private:
    friend class ContextMetal;

    id<MTLRenderCommandEncoder> m_mtlEncoder = nil;
    id<MTLCommandBuffer> m_mtlCommandBuffer = nil;
    id<MTLBuffer> m_mtlIndexBuffer = nil;
    MTLIndexType m_mtlIndexType = MTLIndexTypeUInt16;
    NSUInteger m_mtlIndexBufferOffset = 0;
    MTLPrimitiveType m_mtlPrimitiveType = MTLPrimitiveTypeTriangle;
    rcp<Pipeline> m_currentPipeline;
};
} // namespace rive::ore
