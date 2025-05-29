/*
 * Copyright 2025 Rive
 */

#pragma once

#include "rive/shapes/paint/image_sampler.hpp"
#include "rive/renderer/d3d12/d3d12_utils.hpp"
#include "rive/renderer/d3d12/d3d12_pipeline_manager.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/texture.hpp"

#include <chrono>

namespace rive::gpu
{
class RenderContextD3D12Impl;

class RenderTargetD3D12 : public RenderTarget
{
public:
    RenderTargetD3D12(RenderContextD3D12Impl*, int width, int height);
    ~RenderTargetD3D12() override {}

    void setTargetTexture(rcp<D3D12Texture> tex);
    void setTargetTexture(ComPtr<ID3D12Resource> tex);
    void releaseTexturesImmediately()
    {
        if (m_targetTexture)
            m_targetTexture->forceReleaseResource();
        if (m_offscreenTexture)
            m_offscreenTexture->forceReleaseResource();
        if (m_coverageTexture)
            m_coverageTexture->forceReleaseResource();
        if (m_scratchColorTexture)
            m_scratchColorTexture->forceReleaseResource();
        if (m_clipTexture)
            m_clipTexture->forceReleaseResource();

        m_targetTexture = nullptr;
        m_offscreenTexture = nullptr;
        m_coverageTexture = nullptr;
        m_scratchColorTexture = nullptr;
        m_clipTexture = nullptr;
    }

    D3D12Texture* targetTexture() const { return m_targetTexture.get(); }
    D3D12Texture* clip() const { return m_clipTexture.get(); }
    D3D12Texture* coverage() const { return m_coverageTexture.get(); }

    D3D12Texture* offscreenTexture();

    bool targetTextureSupportsUAV() const { return m_targetTextureSupportsUAV; }

    // Alternate rendering target when targetTextureSupportsUAV() is false.
    void markTargetUAV(D3D12DescriptorHeap*);
    void markClipUAV(D3D12DescriptorHeap*);
    void markScratchColorUAV(D3D12DescriptorHeap*);
    void markCoverageUAV(D3D12DescriptorHeap*);

private:
    rcp<D3D12ResourceManager> m_manager;
    const bool m_gpuSupportsTypedUAVLoadStore;

    rcp<D3D12Texture> m_targetTexture;
    bool m_targetTextureSupportsUAV = false;
    DXGI_FORMAT m_targetFormat = DXGI_FORMAT_UNKNOWN;

    rcp<D3D12Texture> m_offscreenTexture;
    rcp<D3D12Texture> m_coverageTexture;
    rcp<D3D12Texture> m_scratchColorTexture;
    rcp<D3D12Texture> m_clipTexture;
};

// D3D 12 backend implementation of RenderContextImpl.
class RenderContextD3D12Impl : public RenderContextImpl
{
public:
    struct CommandLists
    {
        // used for resource syncing and copies, expected to be created with
        // D3D12_COMMAND_LIST_TYPE_COPY type if this is null, drawCommandList
        // will be used for all , operations. However, this one can be provided
        // for optimization
        ID3D12GraphicsCommandList* copyComandList;
        // used for draw commands, expected to be created with
        // D3D12_COMMAND_LIST_TYPE_DIRECT type if copyComandList is provided,
        // the ID3D12CommandQueue that executes this list must wait on the
        // ID3D12CommandQueue that executes the copyComandList with
        // ID3D12CommandQueue::Wait
        ID3D12GraphicsCommandList* directComandList;
    };

    // the command lit is used to construct static buffers.
    static std::unique_ptr<RenderContext> MakeContext(
        ComPtr<ID3D12Device>,
        ID3D12GraphicsCommandList*,
        const D3DContextOptions&);

    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;

    virtual rcp<Texture> makeImageTexture(
        uint32_t width,
        uint32_t height,
        uint32_t mipLevelCount,
        const uint8_t imageDataRGBAPremul[]) override;

#define IMPLEMENT_RIVE_BUFFER(Name, m_buffer)                                  \
    void resize##Name(size_t sizeInBytes) override                             \
    {                                                                          \
        assert(m_buffer == nullptr);                                           \
        m_buffer##Pool.setTargetSize(sizeInBytes);                             \
    }                                                                          \
    void* map##Name(size_t mapSizeInBytes) override                            \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        return m_buffer->map(mapSizeInBytes);                                  \
    }                                                                          \
    void unmap##Name(size_t mapSizeInBytes) override                           \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        m_buffer->unmap(mapSizeInBytes);                                       \
    }

#define IMPLEMENT_RIVE_STRUCTURED_BUFFER(Name, m_buffer)                       \
    void resize##Name(size_t sizeInBytes, gpu::StorageBufferStructure)         \
        override                                                               \
    {                                                                          \
        assert(m_buffer == nullptr);                                           \
        m_buffer##Pool.setTargetSize(sizeInBytes);                             \
    }                                                                          \
    void* map##Name(size_t mapSizeInBytes) override                            \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        return m_buffer->map(mapSizeInBytes);                                  \
    }                                                                          \
    void unmap##Name(size_t mapSizeInBytes) override                           \
    {                                                                          \
        assert(m_buffer != nullptr);                                           \
        m_buffer->unmap(mapSizeInBytes);                                       \
    }

    IMPLEMENT_RIVE_BUFFER(FlushUniformBuffer, m_flushUniformBuffer)
    IMPLEMENT_RIVE_BUFFER(ImageDrawUniformBuffer, m_imageDrawUniformBuffer)
    IMPLEMENT_RIVE_STRUCTURED_BUFFER(PathBuffer, m_pathBuffer)
    IMPLEMENT_RIVE_STRUCTURED_BUFFER(PaintBuffer, m_paintBuffer)
    IMPLEMENT_RIVE_STRUCTURED_BUFFER(PaintAuxBuffer, m_paintAuxBuffer)
    IMPLEMENT_RIVE_STRUCTURED_BUFFER(ContourBuffer, m_contourBuffer)
    IMPLEMENT_RIVE_BUFFER(GradSpanBuffer, m_gradSpanBuffer)
    IMPLEMENT_RIVE_BUFFER(TessVertexSpanBuffer, m_tessSpanBuffer)
    IMPLEMENT_RIVE_BUFFER(TriangleVertexBuffer, m_triangleBuffer)

#undef IMPLEMENT_RIVE_BUFFER
#undef IMPLEMENT_RIVE_STRUCTURED_BUFFER

    void resizeGradientTexture(uint32_t width, uint32_t height) override;
    void resizeTessellationTexture(uint32_t width, uint32_t height) override;
    void resizeAtlasTexture(uint32_t width, uint32_t height) override;

    void flush(const FlushDescriptor&) override;

    rcp<RenderTargetD3D12> makeRenderTarget(uint32_t width, uint32_t height)
    {
        return make_rcp<RenderTargetD3D12>(this, width, height);
    }

    ComPtr<ID3D12Device> device() const { return m_device; }

    rcp<D3D12ResourceManager> manager() const { return m_resourceManager; }

    const D3DCapabilities& d3dCapabilities() const { return m_capabilities; }

    double secondsNow() const override
    {
        auto elapsed = std::chrono::steady_clock::now() - m_localEpoch;
        return std::chrono::duration<double>(elapsed).count();
    }

    void prepareToFlush(uint64_t nextFrameNumber,
                        uint64_t safeFrameNumber) override;

    virtual void postFlush(const RenderContext::FlushResources&) override;

private:
    RenderContextD3D12Impl(ComPtr<ID3D12Device>,
                           ID3D12GraphicsCommandList*,
                           const D3DCapabilities&);

    void blitSubRect(ID3D12GraphicsCommandList* cmdList,
                     D3D12Texture* dst,
                     D3D12Texture* src,
                     const IAABB& rect);

    ComPtr<ID3D12Device> m_device;
    const D3DCapabilities m_capabilities;

    rcp<D3D12Texture> m_gradientTexture;
    rcp<D3D12Texture> m_tesselationTexture;
    rcp<D3D12Texture> m_atlasTexture;
    rcp<D3D12TextureArray> m_featherTexture;

    D3D12PipelineManager m_pipelineManager;
    rcp<D3D12ResourceManager> m_resourceManager;

    // Rive buffer pools. These don't need to be rcp<> because the destructor of
    // RenderContextVulkanImpl is already synchronized.
    D3D12VolatileBufferPool m_flushUniformBufferPool;
    D3D12VolatileBufferPool m_imageDrawUniformBufferPool;
    D3D12VolatileBufferPool m_pathBufferPool;
    D3D12VolatileBufferPool m_paintBufferPool;
    D3D12VolatileBufferPool m_paintAuxBufferPool;
    D3D12VolatileBufferPool m_contourBufferPool;
    D3D12VolatileBufferPool m_gradSpanBufferPool;
    D3D12VolatileBufferPool m_tessSpanBufferPool;
    D3D12VolatileBufferPool m_triangleBufferPool;

    // this have to be re created every frame, rtv heaps do not
    D3D12DescriptorHeapPool m_srvUavCbvHeapPool;
    D3D12DescriptorHeapPool m_cpuSrvUavCbvHeapPool;
    D3D12DescriptorHeapPool m_samplerHeapPool;
    // Specific Rive buffers that have been acquired for the current frame.
    // When the frame ends, these get recycled back in their respective pools.
    rcp<D3D12VolatileBuffer> m_flushUniformBuffer;
    rcp<D3D12VolatileBuffer> m_imageDrawUniformBuffer;
    rcp<D3D12VolatileBuffer> m_pathBuffer;
    rcp<D3D12VolatileBuffer> m_paintBuffer;
    rcp<D3D12VolatileBuffer> m_paintAuxBuffer;
    rcp<D3D12VolatileBuffer> m_contourBuffer;
    rcp<D3D12VolatileBuffer> m_gradSpanBuffer;
    rcp<D3D12VolatileBuffer> m_tessSpanBuffer;
    rcp<D3D12VolatileBuffer> m_triangleBuffer;

    rcp<D3D12DescriptorHeap> m_srvUavCbvHeap;
    // mirrors m_srvUavCbvHeap, needed for clearing resourcess via ClearUAV
    // calls
    rcp<D3D12DescriptorHeap> m_cpuSrvUavCbvHeap;
    rcp<D3D12DescriptorHeap> m_samplerHeap;
    // this doesn't need to be pooled because rtv heaps are not sent to the gpu,
    // they are only used cpu side
    rcp<D3D12DescriptorHeap> m_rtvHeap;

    // this is needed for multiple logical flushes and is also used for images
    UINT m_heapDescriptorOffset;
    // this is needed for multiple logical flushes and is also used for image
    // samplers
    UINT m_samplerHeapDescriptorOffset;
    ImageSampler m_lastDynamicSampler = ImageSampler::LinearClamp();
    // this is a max number of bound descriptors at once.
    // TODO: replace this with a refactor that allows being told up front how
    // many reosurces will need to be bound per total flush (that means all
    // logical flushes combined) this number is the limit for my computer, i do
    // not know if it is different for different hardware
    // our stress testing only needed about 10012, so we will use 10x that here
    // for now
    static constexpr int MAX_DESCRIPTOR_HEAPS_PER_FLUSH = 100000;
    // same as above, but for samplers
    static constexpr int MAX_DESCRIPTOR_SAMPLER_HEAPS_PER_FLUSH = 2000;

    // these buffers are created once and potentially used every frame
    rcp<D3D12Buffer> m_pathPatchVertexBuffer;
    rcp<D3D12Buffer> m_pathPatchIndexBuffer;
    rcp<D3D12Buffer> m_tessSpanIndexBuffer;
    rcp<D3D12Buffer> m_imageRectVertexBuffer;
    rcp<D3D12Buffer> m_imageRectIndexBuffer;

    D3D12_SAMPLER_DESC m_linearSampler;
    D3D12_SAMPLER_DESC
    m_imageSamplers[ImageSampler::MAX_SAMPLER_PERMUTATIONS];

    std::chrono::steady_clock::time_point m_localEpoch =
        std::chrono::steady_clock::now();

    bool m_isFirstFlushOfFrame = true;
};
} // namespace rive::gpu
