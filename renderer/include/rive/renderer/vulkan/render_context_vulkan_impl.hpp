/*
 * Copyright 2023 Rive
 */

#pragma once

#ifdef RIVE_VULKAN

#include <chrono>
#include <vulkan/vulkan.h>
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/vulkan/vulkan_context.hpp"

namespace rive::gpu
{
class DrawPipelineLayoutVulkan;
class RenderTargetVulkan;
class RenderTargetVulkanImpl;
class PipelineManagerVulkan;
enum class RenderPassOptionsVulkan;

class RenderContextVulkanImpl : public RenderContextImpl
{
public:
    struct ContextOptions
    {
        bool forceAtomicMode = false;
        ShaderCompilationMode shaderCompilationMode =
            ShaderCompilationMode::standard;
    };

    static std::unique_ptr<RenderContext> MakeContext(VkInstance,
                                                      VkPhysicalDevice,
                                                      VkDevice,
                                                      const VulkanFeatures&,
                                                      PFN_vkGetInstanceProcAddr,
                                                      const ContextOptions&);

    static std::unique_ptr<RenderContext> MakeContext(
        VkInstance instance,
        VkPhysicalDevice physicalDevice,
        VkDevice device,
        const VulkanFeatures& vulkanFeatures,
        PFN_vkGetInstanceProcAddr fp_vkGetInstanceProcAddr)
    {
        return MakeContext(instance,
                           physicalDevice,
                           device,
                           vulkanFeatures,
                           fp_vkGetInstanceProcAddr,
                           ContextOptions{});
    }

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
    RenderContextVulkanImpl(rcp<VulkanContext>, const ContextOptions&);

    // Called outside the constructor so we can use virtual methods.
    void initGPUObjects(ShaderCompilationMode);

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
    void resizeTransientPLSBacking(uint32_t width,
                                   uint32_t height,
                                   uint32_t planeCount) override;
    void resizeAtomicCoverageBacking(uint32_t width, uint32_t height) override;
    void resizeCoverageBuffer(size_t sizeInBytes) override;

    // Lazy accessors for PLS backing resources. These are lazy because our
    // Vulkan backend needs different allocations based on interlock mode and
    // other factors.
    vkutil::Image* plsTransientImageArray();
    vkutil::ImageView* plsTransientCoverageView();
    vkutil::ImageView* plsTransientClipView();
    vkutil::Texture2D* plsTransientScratchColorTexture();

    // The offscreen color texture is not transient and supports PLS. It is used
    // in place of the renderTarget (via copying in and out) when the
    // renderTarget doesn't support PLS.
    vkutil::Texture2D* accessPLSOffscreenColorTexture(
        VkCommandBuffer,
        const vkutil::ImageAccess&,
        vkutil::ImageAccessAction =
            vkutil::ImageAccessAction::preserveContents);
    vkutil::Texture2D* clearPLSOffscreenColorTexture(
        VkCommandBuffer,
        ColorInt,
        const vkutil::ImageAccess& dstAccessAfterClear);
    vkutil::Texture2D* copyRenderTargetToPLSOffscreenColorTexture(
        VkCommandBuffer,
        RenderTargetVulkan*,
        const IAABB& copyBounds,
        const vkutil::ImageAccess& dstAccessAfterCopy);

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

    const DrawPipelineLayoutVulkan& beginDrawRenderPass(
        const FlushDescriptor& desc,
        RenderPassOptionsVulkan,
        const IAABB& drawBounds,
        VkImageView colorImageView,
        VkImageView msaaColorSeedImageView,
        VkImageView msaaResolveImageView);

    void flush(const FlushDescriptor&) override;

    void postFlush(const RenderContext::FlushResources&) override;

    double secondsNow() const override
    {
        auto elapsed = std::chrono::steady_clock::now() - m_localEpoch;
        return std::chrono::duration<double>(elapsed).count();
    }

    const rcp<VulkanContext> m_vk;

    struct DriverWorkarounds
    {
        // Some early Android tilers are known to crash when a render pass is
        // too complex. On these devices, we limit the maximum number of
        // instances that can be issued in a single render pass.
        uint32_t maxInstancesPerRenderPass = UINT32_MAX;
    };

    const DriverWorkarounds m_workarounds;

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

    // Bound when there is not an image paint.
    rcp<vkutil::Texture2D> m_nullImageTexture;

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
    rcp<vkutil::Texture2D> m_tesselationSyncIssueWorkaroundTexture;
    rcp<vkutil::Framebuffer> m_tessTextureFramebuffer;

    // Renders feathers to the atlas.
    class AtlasPipeline;
    std::unique_ptr<AtlasPipeline> m_atlasPipeline;
    rcp<vkutil::Texture2D> m_atlasTexture;
    rcp<vkutil::Framebuffer> m_atlasFramebuffer;

    // Pixel local storage backing resources.
    VkImageUsageFlags m_plsTransientUsageFlags;
    VkExtent3D m_plsExtent = {0, 0, 1};
    uint32_t m_plsTransientPlaneCount = 0;
    rcp<vkutil::Image> m_plsTransientImageArray;
    rcp<vkutil::ImageView> m_plsTransientCoverageView;
    rcp<vkutil::ImageView> m_plsTransientClipView;
    rcp<vkutil::Texture2D> m_plsTransientScratchColorTexture;
    rcp<vkutil::Texture2D> m_plsOffscreenColorTexture;
    rcp<vkutil::Texture2D> m_plsAtomicCoverageTexture;

    // Coverage buffer used by shaders in clockwiseAtomic mode.
    rcp<vkutil::Buffer> m_coverageBuffer;

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

    std::unique_ptr<PipelineManagerVulkan> m_pipelineManager;
};
} // namespace rive::gpu

#endif
