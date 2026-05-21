/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/ore/ore_types.hpp"

namespace rive::ore
{

class Buffer;
class Texture;
class TextureView;
class Sampler;
class Pipeline;
class BindGroup;
class Context;

class RenderPass
{
public:
    virtual void setPipeline(Pipeline* pipeline) = 0;

    virtual void setVertexBuffer(uint32_t slot,
                                 Buffer* buffer,
                                 uint32_t offset = 0) = 0;
    virtual void setIndexBuffer(Buffer* buffer,
                                IndexFormat format,
                                uint32_t offset = 0) = 0;

    // Bind a pre-created BindGroup, optionally overriding dynamic UBO offsets.
    virtual void setBindGroup(uint32_t groupIndex,
                              BindGroup* bg,
                              const uint32_t* dynamicOffsets = nullptr,
                              uint32_t dynamicOffsetCount = 0) = 0;

    virtual void setViewport(float x,
                             float y,
                             float width,
                             float height,
                             float minDepth = 0.0f,
                             float maxDepth = 1.0f) = 0;
    virtual void setScissorRect(uint32_t x,
                                uint32_t y,
                                uint32_t width,
                                uint32_t height) = 0;

    virtual void setStencilReference(uint32_t ref) = 0;
    virtual void setBlendColor(float r, float g, float b, float a) = 0;

    virtual void draw(uint32_t vertexCount,
                      uint32_t instanceCount = 1,
                      uint32_t firstVertex = 0,
                      uint32_t firstInstance = 0) = 0;

    virtual void drawIndexed(uint32_t indexCount,
                             uint32_t instanceCount = 1,
                             uint32_t firstIndex = 0,
                             int32_t baseVertex = 0,
                             uint32_t firstInstance = 0) = 0;

    virtual void finish() = 0;
    bool isFinished() const { return m_finished; }

    virtual ~RenderPass() = default;

    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    // Populate attachment metadata from the descriptor. Called by every
    // backend's beginRenderPass() immediately after constructing the
    // RenderPass; defined inline because it only reads cross-backend
    // texture/view accessors that are uniform across backends.
    inline void populateAttachmentMetadata(const RenderPassDesc& desc);
    virtual void validate() const {};

protected:
    friend class Context;

    RenderPass() = default;
    RenderPass(Context* context) : m_context(context) {}

    bool m_finished = false;

    // Attachment metadata — populated by beginRenderPass() from the
    // RenderPassDesc via populateAttachmentMetadata(). Used by setPipeline()
    // for format/sampleCount validation.
    TextureFormat m_colorFormats[4] = {};
    uint32_t m_colorCount = 0;
    TextureFormat m_depthFormat = {};
    bool m_hasDepth = false;
    uint32_t m_sampleCount = 1;

    // Cross-backend back-pointer used by the pipeline-compat validator to
    // route errors through Context::setLastError(). Weak ref.
    Context* m_context = nullptr;

    // Lifecycle: strong refs to bound BindGroups prevent GC from freeing
    // them between setBindGroup() and finish(). Released in finish().
    rcp<BindGroup> m_boundGroups[kMaxBindGroups];

    // WebGPU-spec pipeline/attachment compatibility check, invoked from
    // every backend's setPipeline().
    inline bool checkPipelineCompat(const Pipeline* pipeline) const;
};

} // namespace rive::ore
