/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <chrono>
#include <map>
#include <vulkan/vulkan.h>

namespace rive::gpu
{
class TextureVulkanImpl;

class RenderTargetVulkan : public RenderTarget
{
public:
    const VkFormat framebufferFormat() const { return m_framebufferFormat; }
    const VkImageUsageFlags targetUsageFlags() const
    {
        return m_targetUsageFlags;
    }

    void setTargetImageView(VkImageView imageView,
                            VkImage image,
                            VulkanContext::TextureAccess targetLastAccess)
    {
        m_targetImageView = imageView;
        m_targetImage = image;
        setTargetLastAccess(targetLastAccess);
    }

    void setTargetLastAccess(VulkanContext::TextureAccess lastAccess)
    {
        m_targetLastAccess = lastAccess;
    }

    VkImageView targetImageView() const { return m_targetImageView; }
    VkImage targetImage() const { return m_targetImage; }
    const VulkanContext::TextureAccess& targetLastAccess() const
    {
        return m_targetLastAccess;
    }

private:
    friend class RenderContextVulkanImpl;

    RenderTargetVulkan(rcp<VulkanContext> vk,
                       uint32_t width,
                       uint32_t height,
                       VkFormat framebufferFormat,
                       VkImageUsageFlags targetUsageFlags) :
        RenderTarget(width, height),
        m_vk(std::move(vk)),
        m_framebufferFormat(framebufferFormat),
        m_targetUsageFlags(targetUsageFlags)
    {
        // In order to implement blend modes, the target texture needs to either
        // support input attachment usage (ideal), or else transfers.
        constexpr static VkImageUsageFlags TRANSFER_SRC_AND_DST =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        assert((m_targetUsageFlags & VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT) ||
               (m_targetUsageFlags & TRANSFER_SRC_AND_DST) ==
                   TRANSFER_SRC_AND_DST);
    }

    vkutil::TextureView* offscreenColorTextureView() const
    {
        return m_offscreenColorTextureView.get();
    }

    VkImage offscreenColorTexture() const { return *m_offscreenColorTexture; }
    VkImage coverageTexture() const { return *m_coverageTexture; }
    VkImage clipTexture() const { return *m_clipTexture; }
    VkImage scratchColorTexture() const { return *m_scratchColorTexture; }
    VkImage coverageAtomicTexture() const { return *m_coverageAtomicTexture; }
    VkImage depthStencilTexture() const { return *m_depthStencilTexture; }

    // getters that lazy load if needed.

    vkutil::TextureView* ensureOffscreenColorTextureView();
    vkutil::TextureView* ensureClipTextureView();
    vkutil::TextureView* ensureScratchColorTextureView();
    vkutil::TextureView* ensureCoverageTextureView();
    vkutil::TextureView* ensureCoverageAtomicTextureView();
    vkutil::TextureView* ensureDepthStencilTextureView();

    const rcp<VulkanContext> m_vk;
    const VkFormat m_framebufferFormat;
    const VkImageUsageFlags m_targetUsageFlags;

    VkImageView m_targetImageView;
    VkImage m_targetImage;
    VulkanContext::TextureAccess m_targetLastAccess;

    // Used when m_targetTextureView does not have
    // VK_ACCESS_INPUT_ATTACHMENT_READ_BIT
    rcp<vkutil::Texture> m_offscreenColorTexture;

    rcp<vkutil::Texture> m_coverageTexture; // InterlockMode::rasterOrdering.
    rcp<vkutil::Texture> m_clipTexture;
    rcp<vkutil::Texture> m_scratchColorTexture;
    rcp<vkutil::Texture> m_coverageAtomicTexture; // InterlockMode::atomics.
    rcp<vkutil::Texture> m_depthStencilTexture;

    rcp<vkutil::TextureView> m_offscreenColorTextureView;
    rcp<vkutil::TextureView> m_coverageTextureView;
    rcp<vkutil::TextureView> m_clipTextureView;
    rcp<vkutil::TextureView> m_scratchColorTextureView;
    rcp<vkutil::TextureView> m_coverageAtomicTextureView;
    rcp<vkutil::TextureView> m_depthStencilTextureView;
};

class RenderContextVulkanImpl : public RenderContextImpl
{
public:
    static std::unique_ptr<RenderContext> MakeContext(
        VkInstance,
        VkPhysicalDevice,
        VkDevice,
        const VulkanFeatures&,
        PFN_vkGetInstanceProcAddr);
    ~RenderContextVulkanImpl();

    VulkanContext* vulkanContext() const { return m_vk.get(); }

    rcp<RenderTargetVulkan> makeRenderTarget(uint32_t width,
                                             uint32_t height,
                                             VkFormat framebufferFormat,
                                             VkImageUsageFlags targetUsageFlags)
    {
        return rcp(new RenderTargetVulkan(m_vk,
                                          width,
                                          height,
                                          framebufferFormat,
                                          targetUsageFlags));
    }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    rcp<Texture> makeImageTexture(uint32_t width,
                                  uint32_t height,
                                  uint32_t mipLevelCount,
                                  const uint8_t imageDataRGBAPremul[]) override;

    void hotloadShaders(rive::Span<const uint32_t> spirvData);

private:
    RenderContextVulkanImpl(rcp<VulkanContext>,
                            const VkPhysicalDeviceProperties&);

    // Called outside the constructor so we can use virtual methods.
    void initGPUObjects();

    void prepareToFlush(uint64_t nextFrameNumber,
                        uint64_t safeFrameNumber) override;

#define IMPLEMENT_PLS_BUFFER(Name, m_buffer)                                   \
    void resize##Name(size_t sizeInBytes) override                             \
    {                                                                          \
        assert(m_buffer == nullptr);                                           \
        m_buffer##Pool.setTargetSize(sizeInBytes);                             \
    }                                                                          \
    void* map##Name(size_t mapSizeInBytes) override                            \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        return m_buffer->contents();                                           \
    }                                                                          \
    void unmap##Name(size_t mapSizeInBytes) override                           \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        m_buffer->flushContents(mapSizeInBytes);                               \
    }

#define IMPLEMENT_PLS_STRUCTURED_BUFFER(Name, m_buffer)                        \
    void resize##Name(size_t sizeInBytes, gpu::StorageBufferStructure)         \
        override                                                               \
    {                                                                          \
        assert(m_buffer == nullptr);                                           \
        m_buffer##Pool.setTargetSize(sizeInBytes);                             \
    }                                                                          \
    void* map##Name(size_t mapSizeInBytes) override                            \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        return m_buffer->contents();                                           \
    }                                                                          \
    void unmap##Name(size_t mapSizeInBytes) override                           \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        m_buffer->flushContents(mapSizeInBytes);                               \
    }

    IMPLEMENT_PLS_BUFFER(FlushUniformBuffer, m_flushUniformBuffer)
    IMPLEMENT_PLS_BUFFER(ImageDrawUniformBuffer, m_imageDrawUniformBuffer)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PathBuffer, m_pathBuffer)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PaintBuffer, m_paintBuffer)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(PaintAuxBuffer, m_paintAuxBuffer)
    IMPLEMENT_PLS_STRUCTURED_BUFFER(ContourBuffer, m_contourBuffer)
    IMPLEMENT_PLS_BUFFER(GradSpanBuffer, m_gradSpanBuffer)
    IMPLEMENT_PLS_BUFFER(TessVertexSpanBuffer, m_tessSpanBuffer)
    IMPLEMENT_PLS_BUFFER(TriangleVertexBuffer, m_triangleBuffer)

#undef IMPLEMENT_PLS_BUFFER
#undef IMPLEMENT_PLS_STRUCTURED_BUFFER

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;
    void resizeAtlasTexture(uint32_t width, uint32_t height) override;
    void resizeCoverageBuffer(size_t sizeInBytes) override;

    // Wraps a VkDescriptorPool created specifically for a PLS flush, and tracks
    // its allocated descriptor sets.
    class DescriptorSetPool final : public vkutil::Resource
    {
    public:
        DescriptorSetPool(rcp<VulkanContext>);
        ~DescriptorSetPool();

        VkDescriptorSet allocateDescriptorSet(VkDescriptorSetLayout);
        void reset();

    private:
        VkDescriptorPool m_vkDescriptorPool;
    };

    void flush(const FlushDescriptor&) override;

    double secondsNow() const override
    {
        auto elapsed = std::chrono::steady_clock::now() - m_localEpoch;
        return std::chrono::duration<double>(elapsed).count();
    }

    const rcp<VulkanContext> m_vk;
    const uint32_t m_vendorID;
    const VkFormat m_atlasFormat;

    // Rive buffer pools. These don't need to be rcp<> because the destructor of
    // RenderContextVulkanImpl is already synchronized.
    vkutil::BufferPool m_flushUniformBufferPool;
    vkutil::BufferPool m_imageDrawUniformBufferPool;
    vkutil::BufferPool m_pathBufferPool;
    vkutil::BufferPool m_paintBufferPool;
    vkutil::BufferPool m_paintAuxBufferPool;
    vkutil::BufferPool m_contourBufferPool;
    vkutil::BufferPool m_gradSpanBufferPool;
    vkutil::BufferPool m_tessSpanBufferPool;
    vkutil::BufferPool m_triangleBufferPool;

    // Specific Rive buffers that have been acquired for the current frame.
    // When the frame ends, these get recycled back in their respective pools.
    rcp<vkutil::Buffer> m_flushUniformBuffer;
    rcp<vkutil::Buffer> m_imageDrawUniformBuffer;
    rcp<vkutil::Buffer> m_pathBuffer;
    rcp<vkutil::Buffer> m_paintBuffer;
    rcp<vkutil::Buffer> m_paintAuxBuffer;
    rcp<vkutil::Buffer> m_contourBuffer;
    rcp<vkutil::Buffer> m_gradSpanBuffer;
    rcp<vkutil::Buffer> m_tessSpanBuffer;
    rcp<vkutil::Buffer> m_triangleBuffer;

    std::chrono::steady_clock::time_point m_localEpoch =
        std::chrono::steady_clock::now();

    // Immutable samplers.
    VkSampler m_linearSampler;
    VkSampler m_mipmapSampler;

    // Bound when there is not an image paint.
    rcp<TextureVulkanImpl> m_nullImageTexture;

    // With the exception of PLS texture bindings, which differ by interlock
    // mode, all other shaders use the same shared descriptor set layouts.
    VkDescriptorSetLayout m_perFlushDescriptorSetLayout;
    VkDescriptorSetLayout m_perDrawDescriptorSetLayout;
    VkDescriptorSetLayout m_immutableSamplerDescriptorSetLayout;
    VkDescriptorSetLayout m_emptyDescriptorSetLayout; // For when a set isn't
                                                      // used by a shader.

    VkDescriptorPool m_staticDescriptorPool; // For descriptorSets that never
                                             // change between frames.
    VkDescriptorSet m_nullImageDescriptorSet;
    VkDescriptorSet m_immutableSamplerDescriptorSet; // Empty since samplers are
                                                     // immutable, but also
                                                     // required by Vulkan.

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

    // Renders feathers to the atlas.
    class AtlasPipeline;
    std::unique_ptr<AtlasPipeline> m_atlasPipeline;
    rcp<vkutil::Texture> m_atlasTexture;
    rcp<vkutil::TextureView> m_atlasTextureView;
    rcp<vkutil::Framebuffer> m_atlasFramebuffer;

    // Coverage buffer used by shaders in clockwiseAtomic mode.
    rcp<vkutil::Buffer> m_coverageBuffer;

    // A pipeline for each
    // [rasterOrdering, atomics] x [all DrawPipelineLayoutOptions permutations].
    class DrawPipelineLayout;
    constexpr static int kDrawPipelineLayoutOptionCount = 1;
    std::array<std::unique_ptr<DrawPipelineLayout>,
               gpu::kInterlockModeCount * (1 << kDrawPipelineLayoutOptionCount)>
        m_drawPipelineLayouts;

    class DrawShader;
    std::map<uint32_t, DrawShader> m_drawShaders;

    class DrawPipeline;
    std::map<uint32_t, DrawPipeline> m_drawPipelines;

    // Gaussian integral table for feathering.
    rcp<TextureVulkanImpl> m_featherTexture;

    rcp<vkutil::Buffer> m_pathPatchVertexBuffer;
    rcp<vkutil::Buffer> m_pathPatchIndexBuffer;
    rcp<vkutil::Buffer> m_imageRectVertexBuffer;
    rcp<vkutil::Buffer> m_imageRectIndexBuffer;

    // Pool of DescriptorSetPool instances for flushing.
    class DescriptorSetPoolPool : public GPUResourcePool
    {
    public:
        constexpr static size_t MAX_POOL_SIZE = 64;
        DescriptorSetPoolPool(rcp<GPUResourceManager> manager) :
            GPUResourcePool(std::move(manager), MAX_POOL_SIZE)
        {}

        rcp<DescriptorSetPool> acquire();
    };

    rcp<DescriptorSetPoolPool> m_descriptorSetPoolPool;
};
} // namespace rive::gpu
