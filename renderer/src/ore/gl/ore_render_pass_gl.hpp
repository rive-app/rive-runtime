#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/ore/ore_pipeline.hpp"
#include "rive/refcnt.hpp"
#include <cstdint>

namespace rive::ore
{
class ContextGL;

class RenderPassGL : public RenderPass
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

    RenderPassGL() = default;
    RenderPassGL(Context* context) : RenderPass(context) {}
    ~RenderPassGL() override;
    RenderPassGL(RenderPassGL&& other) noexcept;
    RenderPassGL& operator=(RenderPassGL&&) noexcept;

    struct GLResolveEntry
    {
        unsigned int colorIndex = 0;    // attachment index
        unsigned int resolveTarget = 0; // GLenum (e.g. GL_TEXTURE_2D)
        unsigned int resolveTex = 0;    // GLuint
        uint32_t width = 0;
        uint32_t height = 0;
    };

private:
    friend class ContextGL;

    unsigned int m_glFBO = 0;   // GLuint
    unsigned int m_glVAO = 0;   // GLuint
    unsigned int m_prevVAO = 0; // GLuint saved before this pass
    unsigned int m_prevFBO = 0; // GLuint saved before this pass
    bool m_ownsFBO = false;
    bool m_ownsVAO = false;
    rcp<Pipeline> m_currentPipeline;
    uint32_t m_viewportWidth = 0;
    uint32_t m_viewportHeight = 0;
    uint32_t m_maxSamplerSlot = 0;
    uint32_t m_maxAttribSlot = 0;
    bool m_usedSamplers = false;
    bool m_usedAttribs = false;
    IndexFormat m_glIndexFormat = IndexFormat::uint16;
    uint32_t m_glStencilRef = 0;
    uint32_t m_glResolveCount = 0;
    GLResolveEntry m_glResolves[4];
};
} // namespace rive::ore
