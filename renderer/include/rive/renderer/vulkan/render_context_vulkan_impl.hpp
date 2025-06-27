/*
 * Copyright 2023 Rive
 */

#pragma once

#ifdef RIVE_VULKAN

#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"
#include <chrono>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include "rive/shapes/paint/image_sampler.hpp"

namespace rive::gpu
{
class RenderTargetVulkanImpl;
class DrawShaderVulkan;

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

    rcp<RenderTargetVulkanImpl> makeRenderTarget(
        uint32_t width,
        uint32_t height,
        VkFormat framebufferFormat,
        VkImageUsageFlags targetUsageFlags);

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

    void postFlush(const RenderContext::FlushResources&) override;

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

    // Samplers.
    VkSampler m_linearSampler;
    VkSampler m_imageSamplers[ImageSampler::MAX_SAMPLER_PERMUTATIONS];

    // Bound when there is not an image paint.
    rcp<vkutil::Texture2D> m_nullImageTexture;

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
    rcp<vkutil::Texture2D> m_gradTexture;
    rcp<vkutil::Framebuffer> m_gradTextureFramebuffer;

    // Renders tessellated vertices to the tessellation texture.
    class TessellatePipeline;
    std::unique_ptr<TessellatePipeline> m_tessellatePipeline;
    rcp<vkutil::Buffer> m_tessSpanIndexBuffer;
    rcp<vkutil::Texture2D> m_tessTexture;
    rcp<vkutil::Framebuffer> m_tessTextureFramebuffer;

    // Renders feathers to the atlas.
    class AtlasPipeline;
    std::unique_ptr<AtlasPipeline> m_atlasPipeline;
    rcp<vkutil::Texture2D> m_atlasTexture;
    rcp<vkutil::Framebuffer> m_atlasFramebuffer;

    // Coverage buffer used by shaders in clockwiseAtomic mode.
    rcp<vkutil::Buffer> m_coverageBuffer;

    // Rive-specific options for configuring a flush's VkPipelineLayout.
    enum class DrawPipelineLayoutOptions
    {
        none = 0,

        // No need to attach the COLOR texture as an input attachment. There are
        // no advanced blend modes so we can use built-in hardware blending.
        fixedFunctionColorOutput = 1 << 0,

        // Use an offscreen texture to render color, but also attach the real
        // target texture at the COALESCED_ATOMIC_RESOLVE index, and render to
        // it directly in the atomic resolve step.
        coalescedResolveAndTransfer = 1 << 1,
    };
    constexpr static int DRAW_PIPELINE_LAYOUT_OPTION_COUNT = 2;

    // A VkPipelineLayout for each
    // interlockMode x [all DrawPipelineLayoutOptions permutations].
    constexpr static uint32_t DrawPipelineLayoutIdx(
        gpu::InterlockMode interlockMode,
        DrawPipelineLayoutOptions options)
    {
        return (static_cast<int>(interlockMode)
                << DRAW_PIPELINE_LAYOUT_OPTION_COUNT) |
               static_cast<int>(options);
    }
    constexpr static int DRAW_PIPELINE_LAYOUT_COUNT =
        gpu::kInterlockModeCount * (1 << DRAW_PIPELINE_LAYOUT_OPTION_COUNT);
    constexpr static int DRAW_PIPELINE_LAYOUT_BIT_COUNT =
        DRAW_PIPELINE_LAYOUT_OPTION_COUNT + 2;
    static_assert((1 << DRAW_PIPELINE_LAYOUT_BIT_COUNT) >=
                  DRAW_PIPELINE_LAYOUT_COUNT);
    static_assert((1 << (DRAW_PIPELINE_LAYOUT_BIT_COUNT - 1)) <
                  DRAW_PIPELINE_LAYOUT_COUNT);
    RIVE_DECL_ENUM_BITSET_FRIENDS(DrawPipelineLayoutOptions);

    // VkPipelineLayout wrapper for Rive flushes.
    class DrawPipelineLayout;
    std::array<std::unique_ptr<DrawPipelineLayout>, DRAW_PIPELINE_LAYOUT_COUNT>
        m_drawPipelineLayouts;

    // VkRenderPass wrapper for Rive flushes.
    class RenderPass;
    std::unordered_map<uint32_t, std::unique_ptr<RenderPass>> m_renderPasses;

    // VkPipeline wrapper for Rive draw calls.
    class DrawPipeline;
    std::unordered_map<uint32_t, std::unique_ptr<DrawShaderVulkan>>
        m_drawShaders;
    std::unordered_map<uint64_t, std::unique_ptr<DrawPipeline>> m_drawPipelines;

    // Gaussian integral table for feathering.
    rcp<vkutil::Texture2D> m_featherTexture;

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
RIVE_MAKE_ENUM_BITSET(RenderContextVulkanImpl::DrawPipelineLayoutOptions);
} // namespace rive::gpu

#endif
