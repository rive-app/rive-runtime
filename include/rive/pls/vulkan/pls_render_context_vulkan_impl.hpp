/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/pls/pls_render_context_impl.hpp"
#include "vkutil.hpp"
#include <chrono>
#include <map>
#include <vulkan/vulkan.h>
#include <deque>

namespace rive::pls
{
class PLSTextureVulkanImpl;

// Specific Vulkan features that PLS uses (if available). The client should ensure
// these get enabled on the Vulkan context when supported.
struct VulkanFeatures
{
    // VkPhysicalDeviceFeatures.
    bool independentBlend = false;
    bool fillModeNonSolid = false;
    bool fragmentStoresAndAtomics = false;

    // EXT_rasterization_order_attachment_access.
    bool rasterizationOrderColorAttachmentAccess = false;
};

class PLSRenderTargetVulkan : public PLSRenderTarget
{
public:
    void setTargetTextureView(rcp<vkutil::TextureView> view)
    {
        m_targetTextureView = std::move(view);
    }

private:
    friend class PLSRenderContextVulkanImpl;

    PLSRenderTargetVulkan(uint32_t width, uint32_t height, VkFormat framebufferFormat) :
        PLSRenderTarget(width, height), m_framebufferFormat(framebufferFormat)
    {}

    // Called during flush(). Ensures the required offscreen views are all initialized.
    void synchronize(vkutil::Allocator*, VkCommandBuffer, pls::InterlockMode);

    const VkFormat m_framebufferFormat;
    rcp<vkutil::TextureView> m_targetTextureView;

    rcp<vkutil::Texture> m_coverageTexture; // pls::InterlockMode::rasterOrdering.
    rcp<vkutil::Texture> m_clipTexture;
    rcp<vkutil::Texture> m_scratchColorTexture;
    rcp<vkutil::Texture> m_coverageAtomicTexture; // pls::InterlockMode::atomics.

    rcp<vkutil::TextureView> m_coverageTextureView;
    rcp<vkutil::TextureView> m_clipTextureView;
    rcp<vkutil::TextureView> m_scratchColorTextureView;
    rcp<vkutil::TextureView> m_coverageAtomicTextureView;
};

class PLSRenderContextVulkanImpl : public PLSRenderContextImpl
{
public:
    static std::unique_ptr<PLSRenderContext> MakeContext(rcp<vkutil::Allocator>, VulkanFeatures);

    ~PLSRenderContextVulkanImpl();

    vkutil::Allocator* allocator() const { return m_allocator.get(); }

    rcp<PLSRenderTargetVulkan> makeRenderTarget(uint32_t width,
                                                uint32_t height,
                                                VkFormat framebufferFormat)
    {
        return rcp(new PLSRenderTargetVulkan(width, height, framebufferFormat));
    }

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType, RenderBufferFlags, size_t) override;

    rcp<PLSTexture> decodeImageTexture(Span<const uint8_t> encodedBytes) override;

    // Called when a vkutil::RenderingResource has been fully released (refCnt
    // reaches 0). The resource won't actually be deleted until the current frame's
    // command buffer has finished executing.
    void onRenderingResourceReleased(const vkutil::RenderingResource* resource)
    {
        m_resourcePurgatory.emplace_back(resource, m_currentFrameIdx);
    }

private:
    PLSRenderContextVulkanImpl(rcp<vkutil::Allocator>, VulkanFeatures);

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
    void resize##Name(size_t sizeInBytes, pls::StorageBufferStructure) override                    \
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

    const rcp<vkutil::Allocator> m_allocator;
    const VkDevice m_device;
    const VulkanFeatures m_features;

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

    rcp<pls::CommandBufferCompletionFence> m_frameCompletionFences[pls::kBufferRingSize];
    uint64_t m_currentFrameIdx = 0;
    int m_bufferRingIdx = -1;

    // A vkutil::RenderingResource that has been fully released, but whose
    // underlying Vulkan object may still be referenced by an in-flight command
    // buffer.
    template <typename T> struct ZombieResource
    {
        ZombieResource(T* resource_, uint64_t lastFrameUsed) :
            resource(resource_), expirationFrameIdx(lastFrameUsed + pls::kBufferRingSize)
        {
            assert(resource_->debugging_refcnt() == 0);
        }
        std::unique_ptr<T> resource;
        // Frame index at which the underlying Vulkan resource is no longer is use
        // by an in-flight command buffer.
        const uint64_t expirationFrameIdx;
    };

    // Pool of DescriptorSetPools that have been fully released. These can be
    // recycled once their expirationFrameIdx is reached.
    std::deque<ZombieResource<DescriptorSetPool>> m_descriptorSetPoolPool;

    // Temporary storage for vkutil::RenderingResource instances that have been
    // fully released, but need to persist until in-flight command buffers have
    // finished referencing their underlying Vulkan objects.
    std::deque<ZombieResource<const vkutil::RenderingResource>> m_resourcePurgatory;
};
} // namespace rive::pls
