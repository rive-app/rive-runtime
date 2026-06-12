#pragma once
#include "rive/renderer/ore/ore_render_pass.hpp"
#include "rive/renderer/vulkan/vkutil.hpp"
#include "rive/renderer/ore/ore_texture.hpp"
#include "ore_pipeline_vulkan.hpp"
#include "rive/refcnt.hpp"
#include <vulkan/vulkan.h>
#include <cstdint>

namespace rive::gpu
{
class RenderTargetVulkan;
}

namespace rive::ore
{
class ContextVulkan;

class RenderPassVulkan : public RenderPass
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

    RenderPassVulkan() = default;
    RenderPassVulkan(Context* context) : RenderPass(context) {}
    ~RenderPassVulkan() override;
    RenderPassVulkan(RenderPassVulkan&& other) noexcept;
    RenderPassVulkan& operator=(RenderPassVulkan&&) noexcept;

private:
    friend class ContextVulkan;

    ContextVulkan* m_vkContext = nullptr; // weak ref
    rcp<PipelineVulkan> m_currentPipeline;
    VkCommandBuffer m_vkCmdBuf = VK_NULL_HANDLE;
    rcp<rive::gpu::vkutil::Framebuffer> m_framebuffer;
    VkBuffer m_vkIndexBuffer = VK_NULL_HANDLE;
    VkIndexType m_vkIndexType = VK_INDEX_TYPE_UINT16;
    uint32_t m_vkIndexOffset = 0;
    VkImage m_vkColorImages[4] = {};
    uint32_t m_vkColorBaseLayer[4] = {};
    uint32_t m_vkColorLayerCount[4] = {};
    uint32_t m_vkColorCount = 0;
    gpu::RenderTargetVulkan* m_vkColorRenderTargets[4] = {}; // weak refs
    // Pin textures alive until finish() updates m_vkLayout post-pass.
    rcp<Texture> m_vkColorTextures[4];
    // MSAA resolve target state captured at beginRenderPass.
    // image == VK_NULL_HANDLE means "no resolve".
    struct ResolveTarget
    {
        VkImage image = VK_NULL_HANDLE;
        uint32_t baseMip = 0;
        uint32_t baseLayer = 0;
        uint32_t layerCount = 1;
        gpu::RenderTargetVulkan* renderTarget = nullptr; // weak ref
        // Pins the texture alive until finish() updates m_vkLayout.
        rcp<Texture> texture;
    };
    ResolveTarget m_vkResolveTargets[4];
    VkImage m_vkDepthImage = VK_NULL_HANDLE;
    uint32_t m_vkDepthBaseLayer = 0;
    uint32_t m_vkDepthLayerCount = 1;
    rcp<Texture> m_vkDepthTexture;
    // Pass-state stencil ref; re-emitted on every setPipeline.
    uint32_t m_vkStencilRef = 0;
};
} // namespace rive::ore
