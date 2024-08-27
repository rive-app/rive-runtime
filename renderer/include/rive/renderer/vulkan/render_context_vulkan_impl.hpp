/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <chrono>
#include <map>
#include <vulkan/vulkan.h>
#include <deque>

namespace rive::gpu
{
class PLSTextureVulkanImpl;

class PLSRenderTargetVulkan : public PLSRenderTarget
{
public:
    void setTargetTextureView(rcp<vkutil::TextureView> view)
    {
        m_targetTextureView = std::move(view);
    }

    bool targetViewContainsUsageFlag(VkImageUsageFlagBits bit) const
    {
        return m_targetTextureView ? static_cast<bool>(m_targetTextureView->usageFlags() & bit)
                                   : false;
    }

    const VkFormat framebufferFormat() const { return m_framebufferFormat; }

    VkImage targetTexture() const { return m_targetTextureView->info().image; }

    VkImageView targetTextureView() const { return *m_targetTextureView; }

    VkImageView offscreenColorTextureView() const
    {
        return m_offscreenColorTextureView ? *m_offscreenColorTextureView : nullptr;
    }

    VkImage offscreenColorTexture() const { return *m_offscreenColorTexture; }
    VkImage coverageTexture() const { return *m_coverageTexture; }
    VkImage clipTexture() const { return *m_clipTexture; }
    VkImage scratchColorTexture() const { return *m_scratchColorTexture; }
    VkImage coverageAtomicTexture() const { return *m_coverageAtomicTexture; }

    // getters that lazy load if needed.

    VkImageView ensureOffscreenColorTextureView(VkCommandBuffer);
    VkImageView ensureCoverageTextureView(VkCommandBuffer);
    VkImageView ensureClipTextureView(VkCommandBuffer);
    VkImageView ensureScratchColorTextureView(VkCommandBuffer);
    VkImageView ensureCoverageAtomicTextureView(VkCommandBuffer);

private:
    friend class PLSRenderContextVulkanImpl;

    PLSRenderTargetVulkan(rcp<VulkanContext> vk,
                          uint32_t width,
                          uint32_t height,
                          VkFormat framebufferFormat) :
        PLSRenderTarget(width, height), m_vk(std::move(vk)), m_framebufferFormat(framebufferFormat)
    {}

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    rcp<vkutil::TextureView> m_targetTextureView;

    rcp<vkutil::Texture> m_offscreenColorTexture; // Used when m_targetTextureView does not have
                                                  // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture> m_coverageTexture;       // gpu::InterlockMode::rasterOrdering.
    rcp<vkutil::Texture> m_clipTexture;
    rcp<vkutil::Texture> m_scratchColorTexture;
    rcp<vkutil::Texture> m_coverageAtomicTexture; // gpu::InterlockMode::atomics.

    rcp<vkutil::TextureView> m_offscreenColorTextureView;
    rcp<vkutil::TextureView> m_coverageTextureView;
    rcp<vkutil::TextureView> m_clipTextureView;
    rcp<vkutil::TextureView> m_scratchColorTextureView;
    rcp<vkutil::TextureView> m_coverageAtomicTextureView;
};

class PLSRenderContextVulkanImpl : public PLSRenderContextImpl
{
public:
    static std::unique_ptr<PLSRenderContext> MakeContext(VkInstance,
                                                         VkPhysicalDevice,
                                                         VkDevice,
                                                         const VulkanFeatures&,
                                                         PFN_vkGetInstanceProcAddr,
                                                         PFN_vkGetDeviceProcAddr);
    ~PLSRenderContextVulkanImpl();

    VulkanContext* vulkanContext() const { return m_vk.get(); }

    rcp<PLSRenderTargetVulkan> makeRenderTarget(uint32_t width,
                                                uint32_t height,
                                                VkFormat framebufferFormat)
    {
        return rcp(new PLSRenderTargetVulkan(m_vk, width, height, framebufferFormat));
    }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) override;

private:
    PLSRenderContextVulkanImpl(VkInstance instance,
                               VkPhysicalDevice physicalDevice,
                               VkDevice device,
                               const VulkanFeatures& features,
                               PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr,
                               PFN_vkGetDeviceProcAddr fp_vkGetDeviceProcAddr);

    // Called outside the constructor so we can use virtual methods.
    void initGPUObjects();

    void prepareToMapBuffers() override;

#define IMPLEMENT_PLS_BUFFER(Name, m_name)                                                         \
    void resize##Name(size_t sizeInBytes) override { m_name.setTargetSize(sizeInBytes); }          \
    void* map##Name(size_t mapSizeInBytes) override                                                \
    {                                                                                              \
        return m_name.contentsAt(m_bufferRingIdx, mapSizeInBytes);                                 \
    }                                                                                              \
    void unmap##Name() override { m_name.flushMappedContentsAt(m_bufferRingIdx); }

#define IMPLEMENT_PLS_STRUCTURED_BUFFER(Name, m_name)                                              \
    void resize##Name(size_t sizeInBytes, gpu::StorageBufferStructure) override                    \
    {                                                                                              \
        m_name.setTargetSize(sizeInBytes);                                                         \
    }                                                                                              \
    void* map##Name(size_t mapSizeInBytes) override                                                \
    {                                                                                              \
        return m_name.contentsAt(m_bufferRingIdx, mapSizeInBytes);                                 \
    }                                                                                              \
    void unmap##Name() override { m_name.flushMappedContentsAt(m_bufferRingIdx); }

    IMPLEMENT_PLS_BUFFER(FlushUniformBuffer, m_flushUniformBufferRing)
    IMPLEMENT_PLS_BUFFER(ImageDrawUniformBuffer, m_imageDrawUniformBufferRing)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PathBuffer, m_pathBufferRing)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PaintBuffer, m_paintBufferRing)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PaintAuxBuffer, m_paintAuxBufferRing)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(ContourBuffer, m_contourBufferRing)
    IMPLEMENT_PLS_BUFFER(SimpleColorRampsBuffer, m_simpleColorRampsBufferRing)
    IMPLEMENT_PLS_BUFFER(GradSpanBuffer, m_gradSpanBufferRing)
    IMPLEMENT_PLS_BUFFER(TessVertexSpanBuffer, m_tessSpanBufferRing)
    IMPLEMENT_PLS_BUFFER(TriangleVertexBuffer, m_triangleBufferRing)

#undef IMPLEMENT_PLS_BUFFER
#undef IMPLEMENT_PLS_STRUCTURED_BUFFER

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;

    // Wraps a VkDescriptorPool created specifically for a PLS flush, and tracks
    // its allocated descriptor sets.
    // The vkutil::RenderingResource base ensures this class stays alive until its
    // command buffer finishes, at which point we free the allocated descriptor
    // sets and return the VkDescriptor to the plsContext.
    class DescriptorSetPool final : public vkutil::RenderingResource
    {
    public:
        DescriptorSetPool(PLSRenderContextVulkanImpl*);
        ~DescriptorSetPool() final;

        VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout);
        void freeDescriptorSets();

    private:
        // Recycles this instance in the plsContext's m_descriptorSetPoolPool, if
        // the plsContext still exists.
        void onRefCntReachedZero() const;

        PLSRenderContextVulkanImpl* const m_plsImplVulkan;
        VkDescriptorPool m_vkDescriptorPool;
        std::vector<VkDescriptorSet> m_descriptorSets;
    };

    rcp<DescriptorSetPool> makeDescriptorSetPool();

    void flush(const FlushDescriptor&) override;

    double secondsNow() const override
    {
        auto elapsed = std::chrono::steady_clock::now() - m_localEpoch;
        return std::chrono::duration<double>(elapsed).count();
    }

    const rcp<VulkanContext> m_vk;

    // PLS buffers.
    vkutil::BufferRing m_flushUniformBufferRing;
    vkutil::BufferRing m_imageDrawUniformBufferRing;
    vkutil::BufferRing m_pathBufferRing;
    vkutil::BufferRing m_paintBufferRing;
    vkutil::BufferRing m_paintAuxBufferRing;
    vkutil::BufferRing m_contourBufferRing;
    vkutil::BufferRing m_simpleColorRampsBufferRing;
    vkutil::BufferRing m_gradSpanBufferRing;
    vkutil::BufferRing m_tessSpanBufferRing;
    vkutil::BufferRing m_triangleBufferRing;
    std::chrono::steady_clock::time_point m_localEpoch = std::chrono::steady_clock::now();

    // Renders color ramps to the gradient texture.
    class ColorRampPipeline;
    std::unique_ptr<ColorRampPipeline> m_colorRampPipeline;
    rcp<vkutil::Texture> m_gradientTexture;
    rcp<vkutil::TextureView> m_gradTextureView;
    rcp<vkutil::Framebuffer> m_gradTextureFramebuffer;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessellatePipeline;
    rcp<vkutil::Buffer> m_tessSpanIndexBuffer;
    rcp<vkutil::Texture> m_tessVertexTexture;
    rcp<vkutil::TextureView> m_tessVertexTextureView;
    rcp<vkutil::Framebuffer> m_tessTextureFramebuffer;

    // A pipeline for each
    // [rasterOrdering, atomics] x [all DrawPipelineLayoutOptions permutations].
    class DrawPipelineLayout;
    constexpr static int kDrawPipelineLayoutOptionCount = 1;
    std::array<std::unique_ptr<DrawPipelineLayout>, 2 * (1 << kDrawPipelineLayoutOptionCount)>
        m_drawPipelineLayouts;

    class DrawShader;
    std::map<uint32_t, DrawShader> m_drawShaders;

    class DrawPipeline;
    std::map<uint32_t, DrawPipeline> m_drawPipelines;

    rcp<PLSTextureVulkanImpl> m_nullImageTexture; // Bound when there is not an image paint.
    VkSampler m_linearSampler;
    VkSampler m_mipmapSampler;
    rcp<vkutil::Buffer> m_pathPatchVertexBuffer;
    rcp<vkutil::Buffer> m_pathPatchIndexBuffer;
    rcp<vkutil::Buffer> m_imageRectVertexBuffer;
    rcp<vkutil::Buffer> m_imageRectIndexBuffer;

    rcp<gpu::CommandBufferCompletionFence> m_frameCompletionFences[gpu::kBufferRingSize];
    int m_bufferRingIdx = -1;

    // Pool of DescriptorSetPools that have been fully released. These can be
    // recycled once their expirationFrameIdx is reached.
    std::deque<vkutil::ZombieResource<DescriptorSetPool>> m_descriptorSetPoolPool;
};
} // namespace rive::gpu
