/*
 * Copyright 2025 Rive
 */

#include "rive/renderer/d3d12/render_context_d3d12_impl.hpp"
#include "rive/renderer/d3d/d3d_constants.hpp"
// needed for root sig and heap constants
#include "shaders/d3d/root.sig"

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

#include <sstream>
#include <D3DCompiler.h>

#include <fstream>
// this is defined here instead of root_sig becaise the gpu does not care about
// the number of rtvs this is gradient, tess, atlas and color
static constexpr UINT NUM_RTV_HEAP_DESCRIPTORS = 4;
namespace rive::gpu
{

static constexpr D3D12_FILTER filter_for_sampler_options(ImageFilter option)
{
    switch (option)
    {
        case ImageFilter::trilinear:
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
        case ImageFilter::nearest:
            return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
    }

    RIVE_UNREACHABLE();
    return D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
};

static constexpr D3D12_TEXTURE_ADDRESS_MODE address_mode_for_sampler_option(
    ImageWrap option)
{
    switch (option)
    {
        case ImageWrap::clamp:
            return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case ImageWrap::repeat:
            return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case ImageWrap::mirror:
            return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
    }

    RIVE_UNREACHABLE();
    return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
}

void RenderContextD3D12Impl::blitSubRect(ID3D12GraphicsCommandList* cmdList,
                                         D3D12Texture* dst,
                                         D3D12Texture* src,
                                         const IAABB& rect)
{
    D3D12_BOX updateBox = {
        static_cast<UINT>(rect.left),
        static_cast<UINT>(rect.top),
        0,
        static_cast<UINT>(rect.right),
        static_cast<UINT>(rect.bottom),
        1,
    };

    D3D12_TEXTURE_COPY_LOCATION dstLoc;
    dstLoc.SubresourceIndex = 0;
    dstLoc.pResource = dst->resource();
    dstLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    D3D12_TEXTURE_COPY_LOCATION srcLoc;
    srcLoc.SubresourceIndex = 0;
    srcLoc.pResource = src->resource();
    srcLoc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;

    m_resourceManager->transition(cmdList,
                                  src,
                                  D3D12_RESOURCE_STATE_COPY_SOURCE);
    m_resourceManager->transition(cmdList, dst, D3D12_RESOURCE_STATE_COPY_DEST);
    cmdList->CopyTextureRegion(&dstLoc,
                               updateBox.left,
                               updateBox.top,
                               0,
                               &srcLoc,
                               &updateBox);

    // we don't untransition here because we don't
    // know what we want it to be, so instead we just always transition before
    // use
}

class TextureD3D12Impl : public Texture
{
public:
    TextureD3D12Impl(D3D12ResourceManager* manager,
                     UINT width,
                     UINT height,
                     UINT mipLevel,
                     const uint8_t imageDataRGBA[]) :
        Texture(width, height),
        m_gpuTexture(manager->make2DTexture(width,
                                            height,
                                            mipLevel,
                                            DXGI_FORMAT_R8G8B8A8_UNORM,
                                            D3D12_RESOURCE_FLAG_NONE,
                                            D3D12_RESOURCE_STATE_COPY_DEST))
    {
        D3D12_SUBRESOURCE_DATA srcData;
        srcData.pData = imageDataRGBA;
        srcData.RowPitch = width * 4;
        srcData.SlicePitch = srcData.RowPitch * height;

        UINT numRows;
        UINT64 rowSizeInBtes;
        UINT64 totalBytes;
        auto desc = m_gpuTexture->resource()->GetDesc();
        manager->device()->GetCopyableFootprints(&desc,
                                                 0,
                                                 1,
                                                 0,
                                                 &m_uploadFootprint,
                                                 &numRows,
                                                 &rowSizeInBtes,
                                                 &totalBytes);

        m_uploadBuffer = manager->makeUploadBuffer(
            static_cast<UINT>(totalBytes) +
            D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT);

        D3D12_MEMCPY_DEST DestData = {
            m_uploadBuffer->map(),
            m_uploadFootprint.Footprint.RowPitch,
            SIZE_T(m_uploadFootprint.Footprint.RowPitch * numRows)};

        MemcpySubresource(&DestData,
                          &srcData,
                          static_cast<SIZE_T>(rowSizeInBtes),
                          numRows,
                          m_uploadFootprint.Footprint.Depth);
    }

    D3D12Texture* synchronize(ID3D12GraphicsCommandList* copyList,
                              ID3D12GraphicsCommandList* cmdList,
                              D3D12ResourceManager* manager) const
    {
        if (m_uploadBuffer)
        {
            const CD3DX12_TEXTURE_COPY_LOCATION dst(m_gpuTexture->resource(),
                                                    0);
            const CD3DX12_TEXTURE_COPY_LOCATION src(m_uploadBuffer->resource(),
                                                    m_uploadFootprint);

            copyList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

            manager->transition(cmdList,
                                m_gpuTexture.get(),
                                D3D12_RESOURCE_STATE_GENERIC_READ);

            m_uploadBuffer = nullptr;
        }

        return m_gpuTexture.get();
    }

    D3D12Texture* resource() const { return m_gpuTexture.get(); }

private:
    mutable rcp<D3D12Buffer> m_uploadBuffer;
    const rcp<D3D12Texture> m_gpuTexture;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT m_uploadFootprint;
};

class RenderBufferD3D12Impl
    : public LITE_RTTI_OVERRIDE(RenderBuffer, RenderBufferD3D12Impl)
{
public:
    RenderBufferD3D12Impl(D3D12ResourceManager* manager,
                          RenderBufferType renderBufferType,
                          RenderBufferFlags renderBufferFlags,
                          size_t sizeInBytes) :
        lite_rtti_override(renderBufferType, renderBufferFlags, sizeInBytes),
        m_gpuBuffer(manager->makeBuffer(static_cast<UINT>(sizeInBytes),
                                        D3D12_RESOURCE_FLAG_NONE,
                                        D3D12_RESOURCE_STATE_COMMON))
    {
        m_uploadBuffer =
            manager->makeUploadBuffer(static_cast<UINT>(sizeInBytes));

        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            mappedOnceMempry = m_uploadBuffer->map();
        }
    }

    D3D12Buffer* sync(ID3D12GraphicsCommandList* cmdList)
    {
        if (needsUpload)
        {
            needsUpload = false;
            assert(m_uploadBuffer->sizeInBytes() == m_gpuBuffer->sizeInBytes());
            cmdList->CopyBufferRegion(m_gpuBuffer->resource(),
                                      0,
                                      m_uploadBuffer->resource(),
                                      0,
                                      m_uploadBuffer->sizeInBytes());

            const auto barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_gpuBuffer->resource(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_COMMON);

            cmdList->ResourceBarrier(1, &barrier);
            if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
            {
                m_uploadBuffer = nullptr;
            }
        }

        return m_gpuBuffer.get();
    }

protected:
    void* onMap() override
    {
        assert(m_uploadBuffer);
        needsUpload = true;
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            assert(mappedOnceMempry);
            return mappedOnceMempry;
        }
        else
        {
            return m_uploadBuffer->map();
        }
    }

    void onUnmap() override
    {
        assert(m_uploadBuffer);
        if (flags() & RenderBufferFlags::mappedOnceAtInitialization)
        {
            assert(mappedOnceMempry);
            m_uploadBuffer->unmap();
            mappedOnceMempry = nullptr;
        }
        else
        {
            m_uploadBuffer->unmap();
        }
    }

private:
    rcp<D3D12Buffer> m_uploadBuffer;
    const rcp<D3D12Buffer> m_gpuBuffer;
    void* mappedOnceMempry = nullptr;

    bool needsUpload = false;
};

RenderTargetD3D12::RenderTargetD3D12(RenderContextD3D12Impl* impl,
                                     int width,
                                     int height) :
    RenderTarget(width, height),
    m_manager(impl->manager()),
    m_gpuSupportsTypedUAVLoadStore(
        impl->d3dCapabilities().supportsTypedUAVLoadStore)
{}

void RenderTargetD3D12::setTargetTexture(rcp<D3D12Texture> tex)
{
    if (tex && tex->resource())
    {
        D3D12_RESOURCE_DESC desc = tex->desc();
#ifdef DEBUG
        assert(desc.Width == width());
        assert(desc.Height == height());
        assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
#endif
        m_targetTextureSupportsUAV =
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) &&
            m_gpuSupportsTypedUAVLoadStore;
        m_targetFormat = desc.Format;
    }
    else
    {
        m_targetTextureSupportsUAV = false;
    }

    m_targetTexture = std::move(tex);
    SNAME_D3D12_OBJECT(m_targetTexture, "color");
}

void RenderTargetD3D12::setTargetTexture(ComPtr<ID3D12Resource> tex)
{
    if (tex)
    {
        D3D12_RESOURCE_DESC desc = tex->GetDesc();
#ifdef DEBUG
        assert(desc.Width == width());
        assert(desc.Height == height());
        assert(desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
               desc.Format == DXGI_FORMAT_R8G8B8A8_TYPELESS ||
               desc.Format == DXGI_FORMAT_B8G8R8A8_TYPELESS);
#endif
        m_targetTextureSupportsUAV =
            (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) &&
            m_gpuSupportsTypedUAVLoadStore;
        m_targetFormat = desc.Format;
    }
    else
    {
        m_targetTextureSupportsUAV = false;
    }

    m_targetTexture =
        m_manager->makeExternalTexture(tex, D3D12_RESOURCE_STATE_PRESENT);
    SNAME_D3D12_OBJECT(m_targetTexture, "color");
}

D3D12Texture* RenderTargetD3D12::offscreenTexture()
{
    if (!m_offscreenTexture)
    {
        m_offscreenTexture =
            m_manager->make2DTexture(width(),
                                     height(),
                                     1,
                                     DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                     D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS);
        SNAME_D3D12_OBJECT(m_offscreenTexture, "offscreenColor");
    }
    return m_offscreenTexture.get();
}

void RenderTargetD3D12::markTargetUAV(D3D12DescriptorHeap* heap)
{
    assert(m_targetTexture);
    D3D12Texture* targetTexture = m_targetTexture.get();
    if (!m_targetTextureSupportsUAV)
    {
        targetTexture = offscreenTexture();
    }

    if (auto& uavTexture =
            m_targetTextureSupportsUAV ? m_targetTexture : m_offscreenTexture)
    {
        DXGI_FORMAT targetUavFormat;
        switch (m_targetFormat)
        {
            case DXGI_FORMAT_R8G8B8A8_UNORM:
            case DXGI_FORMAT_R8G8B8A8_TYPELESS:
                targetUavFormat = m_gpuSupportsTypedUAVLoadStore
                                      ? DXGI_FORMAT_R8G8B8A8_UNORM
                                      : DXGI_FORMAT_R32_UINT;
                break;
            case DXGI_FORMAT_B8G8R8A8_UNORM:
            case DXGI_FORMAT_B8G8R8A8_TYPELESS:
                targetUavFormat = m_gpuSupportsTypedUAVLoadStore
                                      ? DXGI_FORMAT_B8G8R8A8_UNORM
                                      : DXGI_FORMAT_R32_UINT;
                break;
            default:
                RIVE_UNREACHABLE();
        }

        heap->markUavToIndex(m_manager->device(),
                             targetTexture,
                             targetUavFormat,
                             ATOMIC_COLOR_HEAP_OFFSET);
    }
}

void RenderTargetD3D12::markClipUAV(D3D12DescriptorHeap* heap)
{
    if (m_clipTexture == nullptr)
    {
        m_clipTexture =
            m_manager->make2DTexture(width(),
                                     height(),
                                     1,
                                     DXGI_FORMAT_R32_UINT,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        SNAME_D3D12_OBJECT(m_clipTexture, "clip");
    }
    assert(m_clipTexture);
    heap->markUavToIndex(m_manager->device(),
                         m_clipTexture.get(),
                         DXGI_FORMAT_R32_UINT,
                         ATOMIC_CLIP_HEAP_OFFSET);
}

void RenderTargetD3D12::markScratchColorUAV(D3D12DescriptorHeap* heap)
{
    // checks and makes texture
    if (!m_scratchColorTexture)
    {
        m_scratchColorTexture =
            m_manager->make2DTexture(width(),
                                     height(),
                                     1,
                                     DXGI_FORMAT_R8G8B8A8_TYPELESS,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                     D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                                     D3D12_HEAP_FLAG_ALLOW_SHADER_ATOMICS);
        SNAME_D3D12_OBJECT(m_scratchColorTexture, "scratchColor");
    }

    assert(m_scratchColorTexture);
    heap->markUavToIndex(m_manager->device(),
                         m_scratchColorTexture.get(),
                         m_gpuSupportsTypedUAVLoadStore
                             ? DXGI_FORMAT_R8G8B8A8_UNORM
                             : DXGI_FORMAT_R32_UINT,
                         ATOMIC_SCRATCH_COLOR_HEAP_OFFSET);
}

void RenderTargetD3D12::markCoverageUAV(D3D12DescriptorHeap* heap)
{
    if (m_coverageTexture == nullptr)
    {
        m_coverageTexture =
            m_manager->make2DTexture(width(),
                                     height(),
                                     1,
                                     DXGI_FORMAT_R32_UINT,
                                     D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
                                     D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

        SNAME_D3D12_OBJECT(m_coverageTexture, "coverage");
    }
    heap->markUavToIndex(m_manager->device(),
                         m_coverageTexture.get(),
                         DXGI_FORMAT_R32_UINT,
                         ATOMIC_COVERAGE_HEAP_OFFSET);
}

std::unique_ptr<RenderContext> RenderContextD3D12Impl::MakeContext(
    ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList* copyCommandList,
    const D3DContextOptions& contextOptions)
{
    D3DCapabilities d3dCapabilities;
    D3D12_FEATURE_DATA_D3D12_OPTIONS d3d12Options;

    if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS,
            &d3d12Options,
            sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS))))
    {
        d3dCapabilities.supportsRasterizerOrderedViews =
            d3d12Options.ROVsSupported;
        if (d3d12Options.TypedUAVLoadAdditionalFormats)
        {
            // TypedUAVLoadAdditionalFormats is true. Now check if we can
            // both load and store all formats used by Rive (currently only
            // RGBA8 and BGRA8):
            // https://learn.microsoft.com/en-us/windows/win32/direct3d11/typed-unordered-access-view-loads.
            auto check_typed_uav_load = [device](DXGI_FORMAT format) {
                D3D12_FEATURE_DATA_FORMAT_SUPPORT d3d12Format{};
                d3d12Format.Format = format;
                if (SUCCEEDED(device->CheckFeatureSupport(
                        D3D12_FEATURE_FORMAT_SUPPORT,
                        &d3d12Format,
                        sizeof(d3d12Format))))
                {
                    constexpr UINT loadStoreFlags =
                        D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD |
                        D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE;
                    return (d3d12Format.Support2 & loadStoreFlags) ==
                           loadStoreFlags;
                }
                return false;
            };

            d3dCapabilities.supportsTypedUAVLoadStore =
                check_typed_uav_load(DXGI_FORMAT_R8G8B8A8_UNORM) &&
                check_typed_uav_load(DXGI_FORMAT_B8G8R8A8_UNORM);

            // Check if we can use HLSL minimum precision types (e.g. min16int)
            d3dCapabilities.supportsMin16Precision =
                d3d12Options.MinPrecisionSupport &
                D3D12_SHADER_MIN_PRECISION_SUPPORT_16_BIT;
        }
    }

    if (contextOptions.disableRasterizerOrderedViews)
    {
        d3dCapabilities.supportsRasterizerOrderedViews = false;
    }
    if (contextOptions.disableTypedUAVLoadStore)
    {
        d3dCapabilities.supportsTypedUAVLoadStore = false;
    }

    d3dCapabilities.isIntel = contextOptions.isIntel;

    auto renderContextImpl = std::unique_ptr<RenderContextD3D12Impl>(
        new RenderContextD3D12Impl(device,
                                   copyCommandList,
                                   d3dCapabilities,
                                   contextOptions.shaderCompilationMode));
    return std::make_unique<RenderContext>(std::move(renderContextImpl));
}

RenderContextD3D12Impl::RenderContextD3D12Impl(
    ComPtr<ID3D12Device> device,
    ID3D12GraphicsCommandList* copyCommandList,
    const D3DCapabilities& capabilities,
    ShaderCompilationMode shaderCompilationMode) :
    m_device(device),
    m_capabilities(capabilities),
    m_pipelineManager(device, capabilities, shaderCompilationMode),
    m_resourceManager(make_rcp<D3D12ResourceManager>(device)),
    m_flushUniformBufferPool(m_resourceManager, sizeof(FlushUniforms)),
    m_imageDrawUniformBufferPool(m_resourceManager, sizeof(ImageDrawUniforms)),
    m_pathBufferPool(m_resourceManager),
    m_paintBufferPool(m_resourceManager),
    m_paintAuxBufferPool(m_resourceManager),
    m_contourBufferPool(m_resourceManager),
    m_gradSpanBufferPool(m_resourceManager),
    m_tessSpanBufferPool(m_resourceManager),
    m_triangleBufferPool(m_resourceManager),
    // this is 64kb which is the smallest heap that can be sent to gpu
    m_srvUavCbvHeapPool(m_resourceManager,
                        MAX_DESCRIPTOR_HEAPS_PER_FLUSH,
                        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                        D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE),
    m_cpuSrvUavCbvHeapPool(m_resourceManager,
                           NUM_SRV_UAV_HEAP_DESCRIPTORS,
                           D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
                           D3D12_DESCRIPTOR_HEAP_FLAG_NONE),
    m_samplerHeapPool(m_resourceManager,
                      MAX_DESCRIPTOR_SAMPLER_HEAPS_PER_FLUSH,
                      D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
                      D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
{

    m_platformFeatures.clipSpaceBottomUp = true;
    m_platformFeatures.framebufferBottomUp = false;
    m_platformFeatures.supportsRasterOrdering =
        m_capabilities.supportsRasterizerOrderedViews;
    m_platformFeatures.supportsFragmentShaderAtomics = true;
    m_platformFeatures.maxTextureSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;

    m_rtvHeap = m_resourceManager->makeHeap(NUM_RTV_HEAP_DESCRIPTORS,
                                            D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                            D3D12_DESCRIPTOR_HEAP_FLAG_NONE);

    m_pipelineManager.compileGradientPipeline();
    m_pipelineManager.compileTesselationPipeline();
    m_pipelineManager.compileAtlasPipeline();

    m_linearSampler.Filter = D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    m_linearSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_linearSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_linearSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    m_linearSampler.MipLODBias = 0.0f;
    m_linearSampler.MaxAnisotropy = 1;
    m_linearSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    m_linearSampler.MinLOD = 0;
    m_linearSampler.MaxLOD = 0;

    for (int i = 0; i < ImageSampler::MAX_SAMPLER_PERMUTATIONS; ++i)
    {
        auto wrapX = ImageSampler::GetWrapXOptionFromKey(i);
        auto wrapY = ImageSampler::GetWrapYOptionFromKey(i);
        auto filter = ImageSampler::GetFilterOptionFromKey(i);

        m_imageSamplers[i].Filter = filter_for_sampler_options(filter);
        m_imageSamplers[i].AddressU = address_mode_for_sampler_option(wrapX);
        m_imageSamplers[i].AddressV = address_mode_for_sampler_option(wrapY);
        m_imageSamplers[i].AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        m_imageSamplers[i].MipLODBias = 0.0f;
        m_imageSamplers[i].MaxAnisotropy = 1;
        m_imageSamplers[i].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        m_imageSamplers[i].MinLOD = 0;
        // this should be D3D12_FLOAT32_MAX but currently we don't generate mips
        m_imageSamplers[i].MaxLOD = 0;
    }

    PatchVertex patchVertices[kPatchVertexBufferCount];
    uint16_t patchIndices[kPatchIndexBufferCount];

    GeneratePatchBufferData(patchVertices, patchIndices);

    // advance to ensure that safe frame does not equal current frame so that
    // our temp upload buffers do not get released automatically
    m_resourceManager->advanceFrameNumber(1, 0);

    m_pathPatchVertexBuffer =
        m_resourceManager->makeImmutableBuffer<>(copyCommandList,
                                                 patchVertices);
    NAME_D3D12_OBJECT(m_pathPatchVertexBuffer);
    m_pathPatchIndexBuffer =
        m_resourceManager->makeImmutableBuffer<>(copyCommandList, patchIndices);
    NAME_D3D12_OBJECT(m_pathPatchIndexBuffer);
    m_tessSpanIndexBuffer =
        m_resourceManager->makeImmutableBuffer<>(copyCommandList,
                                                 kTessSpanIndices);
    NAME_D3D12_OBJECT(m_tessSpanIndexBuffer);
    m_imageRectVertexBuffer =
        m_resourceManager->makeImmutableBuffer<>(copyCommandList,
                                                 kImageRectVertices);
    NAME_D3D12_OBJECT(m_imageRectVertexBuffer);
    m_imageRectIndexBuffer =
        m_resourceManager->makeImmutableBuffer<>(copyCommandList,
                                                 kImageRectIndices);
    NAME_D3D12_OBJECT(m_imageRectIndexBuffer);
    m_featherTexture =
        m_resourceManager->make1DTextureArray(GAUSSIAN_TABLE_SIZE,
                                              FEATHER_TEXTURE_1D_ARRAY_LENGTH,
                                              1,
                                              DXGI_FORMAT_R16_FLOAT,
                                              D3D12_RESOURCE_FLAG_NONE,
                                              D3D12_RESOURCE_STATE_COPY_DEST);
    NAME_D3D12_OBJECT(m_featherTexture);
    auto featherUploadBuffer = m_resourceManager->makeUploadBuffer(
        sizeof(gpu::g_inverseGaussianIntegralTableF16) * 2);

    D3D12_SUBRESOURCE_DATA updateData;
    updateData.pData = g_gaussianIntegralTableF16;
    updateData.RowPitch = sizeof(g_gaussianIntegralTableF16);
    updateData.SlicePitch = updateData.RowPitch;

    UpdateSubresources(copyCommandList,
                       m_featherTexture->resource(),
                       featherUploadBuffer->resource(),
                       0,
                       0,
                       1,
                       &updateData);

    D3D12_SUBRESOURCE_DATA updateData2;
    updateData2.pData = g_inverseGaussianIntegralTableF16;
    updateData2.RowPitch = sizeof(g_inverseGaussianIntegralTableF16);
    updateData2.SlicePitch = updateData.RowPitch;

    UpdateSubresources(copyCommandList,
                       m_featherTexture->resource(),
                       featherUploadBuffer->resource(),
                       sizeof(gpu::g_gaussianIntegralTableF16),
                       1,
                       1,
                       &updateData2);

    m_resourceManager->transition(copyCommandList,
                                  m_featherTexture.get(),
                                  D3D12_RESOURCE_STATE_COMMON);
}

rcp<RenderBuffer> RenderContextD3D12Impl::makeRenderBuffer(
    RenderBufferType type,
    RenderBufferFlags flags,
    size_t size)
{
    return make_rcp<RenderBufferD3D12Impl>(m_resourceManager.get(),
                                           type,
                                           flags,
                                           size);
}

rcp<Texture> RenderContextD3D12Impl::makeImageTexture(
    uint32_t width,
    uint32_t height,
    uint32_t mipLevelCount,
    const uint8_t imageDataRGBAPremul[])
{
    return make_rcp<TextureD3D12Impl>(m_resourceManager.get(),
                                      width,
                                      height,
                                      mipLevelCount,
                                      imageDataRGBAPremul);
}

void rive::gpu::RenderContextD3D12Impl::resizeGradientTexture(uint32_t width,
                                                              uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_gradientTexture = nullptr;
    }
    else
    {
        m_gradientTexture = m_resourceManager->make2DTexture(
            width,
            height,
            1,
            DXGI_FORMAT_R8G8B8A8_UNORM,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        // these can be done here since they arent recycled every frame
        m_rtvHeap->markRtvToIndex(m_device.Get(),
                                  m_gradientTexture.get(),
                                  GRAD_RTV_HEAP_OFFSET);
        SNAME_D3D12_OBJECT(m_gradientTexture, "gradient");
    }
}

void rive::gpu::RenderContextD3D12Impl::resizeTessellationTexture(
    uint32_t width,
    uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_tesselationTexture = nullptr;
    }
    else
    {

        m_tesselationTexture = m_resourceManager->make2DTexture(
            width,
            height,
            1,
            DXGI_FORMAT_R32G32B32A32_UINT,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
        // these can be done here since they arent recycled every frame
        m_rtvHeap->markRtvToIndex(m_device.Get(),
                                  m_tesselationTexture.get(),
                                  TESS_RTV_HEAP_OFFSET);
        SNAME_D3D12_OBJECT(m_tesselationTexture, "tesselation");
    }
}

void rive::gpu::RenderContextD3D12Impl::resizeAtlasTexture(uint32_t width,
                                                           uint32_t height)
{
    if (width == 0 || height == 0)
    {
        m_atlasTexture = nullptr;
    }
    else
    {

        D3D12_CLEAR_VALUE clear{DXGI_FORMAT_R32_FLOAT, {}};

        m_atlasTexture = m_resourceManager->make2DTexture(
            width,
            height,
            1,
            DXGI_FORMAT_R32_FLOAT,
            D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_HEAP_FLAG_NONE,
            &clear);
        // these can be done here since they arent recycled every frame
        m_rtvHeap->markRtvToIndex(m_device.Get(),
                                  m_atlasTexture.get(),
                                  ATLAS_RTV_HEAP_OFFSET);
        SNAME_D3D12_OBJECT(m_atlasTexture, "atlas");
    }
}

void RenderContextD3D12Impl::prepareToFlush(uint64_t nextFrameNumber,
                                            uint64_t safeFrameNumber)
{
    // These should all have gotten recycled at the end of the last frame.
    assert(m_flushUniformBuffer == nullptr);
    assert(m_imageDrawUniformBuffer == nullptr);
    assert(m_pathBuffer == nullptr);
    assert(m_paintBuffer == nullptr);
    assert(m_paintAuxBuffer == nullptr);
    assert(m_contourBuffer == nullptr);
    assert(m_gradSpanBuffer == nullptr);
    assert(m_tessSpanBuffer == nullptr);
    assert(m_triangleBuffer == nullptr);

    assert(m_srvUavCbvHeap == nullptr);
    assert(m_cpuSrvUavCbvHeap == nullptr);
    assert(m_samplerHeap == nullptr);

    // Advance the context frame and delete resources that are no longer
    // referenced by in-flight command buffers.
    m_resourceManager->advanceFrameNumber(nextFrameNumber, safeFrameNumber);

    // Acquire buffers for the flush.
    m_flushUniformBuffer = m_flushUniformBufferPool.acquire();
    m_imageDrawUniformBuffer = m_imageDrawUniformBufferPool.acquire();
    m_pathBuffer = m_pathBufferPool.acquire();
    m_paintBuffer = m_paintBufferPool.acquire();
    m_paintAuxBuffer = m_paintAuxBufferPool.acquire();
    m_contourBuffer = m_contourBufferPool.acquire();
    m_gradSpanBuffer = m_gradSpanBufferPool.acquire();
    m_tessSpanBuffer = m_tessSpanBufferPool.acquire();
    m_triangleBuffer = m_triangleBufferPool.acquire();

    m_srvUavCbvHeap = m_srvUavCbvHeapPool.acquire();
    m_cpuSrvUavCbvHeap = m_cpuSrvUavCbvHeapPool.acquire();
    m_samplerHeap = m_samplerHeapPool.acquire();

    VNAME_D3D12_OBJECT(m_flushUniformBuffer);
    VNAME_D3D12_OBJECT(m_imageDrawUniformBuffer);
    VNAME_D3D12_OBJECT(m_pathBuffer);
    VNAME_D3D12_OBJECT(m_paintBuffer);
    VNAME_D3D12_OBJECT(m_paintAuxBuffer);
    VNAME_D3D12_OBJECT(m_contourBuffer);
    VNAME_D3D12_OBJECT(m_gradSpanBuffer);
    VNAME_D3D12_OBJECT(m_tessSpanBuffer);
    VNAME_D3D12_OBJECT(m_triangleBuffer);

    m_isFirstFlushOfFrame = true;

    m_heapDescriptorOffset = 0;
    m_samplerHeapDescriptorOffset = IMAGE_SAMPLER_HEAP_OFFSET;
    m_lastDynamicSampler = ImageSampler::LinearClamp();
}

void RenderContextD3D12Impl::flush(const FlushDescriptor& desc)
{
    CommandLists* commandLists =
        static_cast<CommandLists*>(desc.externalCommandBuffer);
    assert(commandLists);

    auto copyCmdList = commandLists->copyComandList;
    auto cmdList = commandLists->directComandList;
    // it's ok but less efficient to use one command list for everything
    // and a copy command list may not be guaranteed on certain platforms
    if (!copyCmdList)
        copyCmdList = cmdList;
    assert(copyCmdList);
    assert(cmdList);

    // this assert doesn't take into account any potential images
    // there is no way to currently know how many we have though so we have to
    // just do this and assert the image index later
    if (m_heapDescriptorOffset + NUM_SRV_UAV_HEAP_DESCRIPTORS >=
        MAX_DESCRIPTOR_HEAPS_PER_FLUSH)
    {
        fprintf(stderr,
                "heap descriptor ran out of room ! heapOffset %i maxHeap %i\n",
                m_heapDescriptorOffset,
                MAX_DESCRIPTOR_HEAPS_PER_FLUSH);
        assert(false);
        return;
    }

    auto renderTarget = static_cast<RenderTargetD3D12*>(desc.renderTarget);

    auto width = renderTarget->width();
    auto height = renderTarget->height();

    auto targetTexture = renderTarget->targetTexture();

    // offset for where to start writing image descriptors into the heap
    // on every frame but the first, that is just the number of dynamic
    // descriptors the first frame though we need to move it the entire length
    // of descriptors over
    UINT64 imageDescriptorOffset = NUM_DYNAMIC_SRV_HEAP_DESCRIPTORS;
    if (m_isFirstFlushOfFrame)
    {
        m_isFirstFlushOfFrame = false;

        // the -1 is because this size includes the image descriptor itself
        // note we could just use IMAGE_HEAP_OFFSET_START but i thought this
        // was more readable for someone who hasn't seen this code before
        imageDescriptorOffset = (NUM_SRV_UAV_HEAP_DESCRIPTORS - 1);
        // sync everything first logical flush,
        // copies are slow, so do it all at once
        m_flushUniformBuffer->sync(copyCmdList, 0);
        m_imageDrawUniformBuffer->sync(copyCmdList, 0);
        if (desc.pathCount > 0)
        {
            m_pathBuffer->sync(copyCmdList, 0);
            m_paintBuffer->sync(copyCmdList, 0);
            m_paintAuxBuffer->sync(copyCmdList, 0);
        }
        if (desc.contourCount > 0)
        {
            m_contourBuffer->sync(copyCmdList, 0);
        }

        if (desc.tessVertexSpanCount)
        {
            m_tessSpanBuffer->sync(copyCmdList, 0);
        }

        if (desc.gradSpanCount)
        {
            m_gradSpanBuffer->sync(copyCmdList, 0);
        }

        if (desc.hasTriangleVertices)
        {
            m_triangleBuffer->sync(copyCmdList, 0);
        }

        // mark all UAVS, thse only need to happen the fist time since they
        // won't change per logical flush

        renderTarget->markClipUAV(m_cpuSrvUavCbvHeap.get());
        renderTarget->markCoverageUAV(m_cpuSrvUavCbvHeap.get());
        renderTarget->markTargetUAV(m_cpuSrvUavCbvHeap.get());
        renderTarget->markScratchColorUAV(m_cpuSrvUavCbvHeap.get());
        m_rtvHeap->markRtvToIndex(m_device.Get(),
                                  targetTexture,
                                  TARGET_RTV_HEAP_OFFSET);
        // mark all the texture srvs, these also only need to be done once per
        // flush since the texture aren't re created per logical flush
        if (m_gradientTexture)
        {
            m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                               m_gradientTexture.get(),
                                               GRAD_IMAGE_HEAP_OFFSET);
        }
        if (m_tesselationTexture)
        {
            m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                               m_tesselationTexture.get(),
                                               TESS_IMAGE_HEAP_OFFSET);
        }
        if (m_atlasTexture)
        {
            m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                               m_atlasTexture.get(),
                                               ATLAS_IMAGE_HEAP_OFFSET);
        }
        assert(m_featherTexture);
        m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                           m_featherTexture.get(),
                                           FEATHER_IMAGE_HEAP_OFFSET);

        // do this only once.
        m_pipelineManager.setRootSig(cmdList);

        ID3D12DescriptorHeap* ppHeaps[] = {m_srvUavCbvHeap->heap(),
                                           m_samplerHeap->heap()};
        // copy the static descriptors
        m_device->CopyDescriptorsSimple(
            NUM_STATIC_SRV_UAV_HEAP_DESCRIPTORS,
            m_srvUavCbvHeap->cpuHandleForUpload(
                STATIC_SRV_UAV_HEAP_DESCRIPTOPR_START),
            m_cpuSrvUavCbvHeap->cpuHandleForIndex(
                STATIC_SRV_UAV_HEAP_DESCRIPTOPR_START),
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        // this is extremely expensive as it causes a fence on some hardware
        // (nvidia) so we do this only once per flush
        // some api's may potentially expose a global heap to use, if so, we
        // should use that one instead. but that will have to be addressed once
        // we start integrating
        cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

        // setup samplers, these don't change,
        // image sampler will eventually but for now keep it here
        m_samplerHeap->markSamplerToIndex(m_device.Get(),
                                          m_linearSampler,
                                          TESS_SAMPLER_HEAP_OFFSET);
        m_samplerHeap->markSamplerToIndex(m_device.Get(),
                                          m_linearSampler,
                                          GRAD_SAMPLER_HEAP_OFFSET);
        m_samplerHeap->markSamplerToIndex(m_device.Get(),
                                          m_linearSampler,
                                          FEATHER_SAMPLER_HEAP_OFFSET);
        m_samplerHeap->markSamplerToIndex(m_device.Get(),
                                          m_linearSampler,
                                          ATLAS_SAMPLER_HEAP_OFFSET);

        // this SHOULD be m_mipSampler but we don't currently generate mips
        m_samplerHeap->markSamplerToIndex(m_device.Get(),
                                          m_linearSampler,
                                          IMAGE_SAMPLER_HEAP_OFFSET);

        cmdList->SetGraphicsRootDescriptorTable(
            STATIC_SRV_SIG_INDEX,
            m_srvUavCbvHeap->gpuHandleForIndex(
                STATIC_SRV_UAV_HEAP_DESCRIPTOPR_START));

        cmdList->SetGraphicsRootDescriptorTable(
            UAV_SIG_INDEX,
            m_srvUavCbvHeap->gpuHandleForIndex(UAV_START_HEAP_INDEX));

        cmdList->SetGraphicsRootDescriptorTable(
            SAMPLER_SIG_INDEX,
            m_samplerHeap->gpuHandleForIndex(0));

        cmdList->SetGraphicsRootDescriptorTable(
            DYNAMIC_SAMPLER_SIG_INDEX,
            m_samplerHeap->gpuHandleForIndex(IMAGE_SAMPLER_HEAP_OFFSET));
    }

    // note that flush could have been left a heap but it would be pointless
    // since it would have been a heap size of 1, so instead we bind to as a
    // root constant buffer view instead
    cmdList->SetGraphicsRootConstantBufferView(
        FLUSH_UNIFORM_BUFFFER_SIG_INDEX,
        m_flushUniformBuffer->resource()->getGPUVirtualAddress() +
            desc.flushUniformDataOffsetInBytes);

    if (desc.pathCount > 0)
    {
        m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                           m_pathBuffer->resource(),
                                           PATH_BUFFER_HEAP_OFFSET,
                                           desc.pathCount,
                                           sizeof(PathData),
                                           desc.firstPath);

        m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                           m_paintBuffer->resource(),
                                           PAINT_BUFFER_HEAP_OFFSET,
                                           desc.pathCount,
                                           sizeof(PaintData),
                                           desc.firstPaint);

        m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                           m_paintAuxBuffer->resource(),
                                           PAINT_AUX_BUFFER_HEAP_OFFSET,
                                           desc.pathCount,
                                           sizeof(PaintAuxData),
                                           desc.firstPaintAux);
    }

    if (desc.contourCount > 0)
    {
        m_cpuSrvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                           m_contourBuffer->resource(),
                                           CONTOUR_BUFFER_HEAP_OFFSET,
                                           desc.contourCount,
                                           sizeof(ContourData),
                                           desc.firstContour);
    }

    // copy to gpu heap
    m_device->CopyDescriptorsSimple(
        NUM_DYNAMIC_SRV_HEAP_DESCRIPTORS,
        m_srvUavCbvHeap->cpuHandleForUpload(
            m_heapDescriptorOffset + DYNAMIC_SRV_UAV_HEAP_DESCRIPTOPR_START),
        m_cpuSrvUavCbvHeap->cpuHandleForIndex(
            DYNAMIC_SRV_UAV_HEAP_DESCRIPTOPR_START),
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    cmdList->SetGraphicsRootDescriptorTable(
        DYNAMIC_SRV_SIG_INDEX,
        m_srvUavCbvHeap->gpuHandleForIndex(
            m_heapDescriptorOffset + DYNAMIC_SRV_UAV_HEAP_DESCRIPTOPR_START));

    if (desc.gradSpanCount)
    {

        CD3DX12_VIEWPORT viewport(0.0f,
                                  0.0f,
                                  static_cast<float>(kGradTextureWidth),
                                  static_cast<float>(desc.gradDataHeight));
        CD3DX12_RECT scissorRect(0,
                                 0,
                                 static_cast<LONG>(kGradTextureWidth),
                                 static_cast<LONG>(desc.gradDataHeight));

        m_resourceManager->transition(cmdList,
                                      m_gradientTexture.get(),
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHandle = m_rtvHeap->cpuHandleForIndex(GRAD_RTV_HEAP_OFFSET);

        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissorRect);

        auto view = m_gradSpanBuffer->resource()->vertexBufferView(
            0,
            sizeof(GradientSpan));

        cmdList->IASetVertexBuffers(0, 1, &view);

        m_pipelineManager.setGradientPipeline(cmdList);

        cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        cmdList->SetGraphicsRoot32BitConstant(
            VERTEX_DRAW_UNIFORM_SIG_INDEX,
            math::lossless_numeric_cast<UINT>(desc.firstGradSpan),
            0);

        cmdList->DrawInstanced(
            gpu::GRAD_SPAN_TRI_STRIP_VERTEX_COUNT,
            desc.gradSpanCount,
            0,
            math::lossless_numeric_cast<UINT>(desc.firstGradSpan));

        m_resourceManager->transition(cmdList,
                                      m_gradientTexture.get(),
                                      D3D12_RESOURCE_STATE_GENERIC_READ);

        cmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }

    if (desc.tessVertexSpanCount)
    {
        m_resourceManager->transition(cmdList,
                                      m_tesselationTexture.get(),
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHandle = m_rtvHeap->cpuHandleForIndex(TESS_RTV_HEAP_OFFSET);

        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        CD3DX12_VIEWPORT viewport(0.0f,
                                  0.0f,
                                  static_cast<float>(kTessTextureWidth),
                                  static_cast<float>(desc.tessDataHeight));
        CD3DX12_RECT scissorRect(0,
                                 0,
                                 static_cast<LONG>(kTessTextureWidth),
                                 static_cast<LONG>(desc.tessDataHeight));

        cmdList->RSSetViewports(1, &viewport);
        cmdList->RSSetScissorRects(1, &scissorRect);

        cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        auto VBView = m_tessSpanBuffer->resource()->vertexBufferView(
            sizeof(TessVertexSpan));

        auto IBView =
            m_tessSpanIndexBuffer->indexBufferView(0, sizeof(kTessSpanIndices));

        cmdList->IASetVertexBuffers(0, 1, &VBView);
        cmdList->IASetIndexBuffer(&IBView);

        m_pipelineManager.setTesselationPipeline(cmdList);

        cmdList->DrawIndexedInstanced(
            std::size(gpu::kTessSpanIndices),
            desc.tessVertexSpanCount,
            0,
            0,
            math::lossless_numeric_cast<UINT>(desc.firstTessVertexSpan));

        cmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);

        m_resourceManager->transition(cmdList,
                                      m_tesselationTexture.get(),
                                      D3D12_RESOURCE_STATE_GENERIC_READ);
    }

    D3D12_VERTEX_BUFFER_VIEW NULL_VIEW;
    NULL_VIEW.BufferLocation = 0;
    NULL_VIEW.SizeInBytes = 0;
    NULL_VIEW.StrideInBytes = 0;

    D3D12_VERTEX_BUFFER_VIEW vertexBuffers[3] = {
        m_pathPatchVertexBuffer->vertexBufferView(0,
                                                  kPatchVertexBufferCount,
                                                  sizeof(PatchVertex)),
        desc.hasTriangleVertices
            ? m_triangleBuffer->resource()->vertexBufferView(
                  0,
                  sizeof(TriangleVertex))
            : NULL_VIEW,
        m_imageRectVertexBuffer->vertexBufferView(0,
                                                  std::size(kImageRectVertices),
                                                  sizeof(ImageRectVertex))};

    static_assert(PATCH_VERTEX_DATA_SLOT == 0);
    static_assert(TRIANGLE_VERTEX_DATA_SLOT == 1);
    static_assert(IMAGE_RECT_VERTEX_DATA_SLOT == 2);
    cmdList->IASetVertexBuffers(0, 3, vertexBuffers);

    if ((desc.atlasFillBatchCount | desc.atlasStrokeBatchCount) != 0)
    {
        m_resourceManager->transition(cmdList,
                                      m_atlasTexture.get(),
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHandle = m_rtvHeap->cpuHandleForIndex(ATLAS_RTV_HEAP_OFFSET);
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        float clearZero[4]{};
        cmdList->ClearRenderTargetView(rtvHandle, clearZero, 0, nullptr);

        CD3DX12_VIEWPORT viewport(0.0f,
                                  0.0f,
                                  static_cast<float>(desc.atlasContentWidth),
                                  static_cast<float>(desc.atlasContentHeight));

        cmdList->RSSetViewports(1, &viewport);
        cmdList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        auto IBV = m_pathPatchIndexBuffer->indexBufferView();
        cmdList->IASetIndexBuffer(&IBV);
        if (desc.atlasFillBatchCount != 0)
        {
            m_pipelineManager.setAtlasFillPipeline(cmdList);

            for (size_t i = 0; i < desc.atlasFillBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& fillBatch = desc.atlasFillBatches[i];
                CD3DX12_RECT scissorRect(fillBatch.scissor.left,
                                         fillBatch.scissor.top,
                                         fillBatch.scissor.right,
                                         fillBatch.scissor.bottom);
                cmdList->RSSetScissorRects(1, &scissorRect);
                cmdList->SetGraphicsRoot32BitConstant(
                    VERTEX_DRAW_UNIFORM_SIG_INDEX,
                    fillBatch.basePatch,
                    0);
                cmdList->DrawIndexedInstanced(
                    kMidpointFanCenterAAPatchIndexCount,
                    fillBatch.patchCount,
                    kMidpointFanCenterAAPatchBaseIndex,
                    0,
                    fillBatch.basePatch);
            }
        }

        if (desc.atlasStrokeBatchCount != 0)
        {
            m_pipelineManager.setAtlasStrokePipeline(cmdList);

            for (size_t i = 0; i < desc.atlasStrokeBatchCount; ++i)
            {
                const gpu::AtlasDrawBatch& strokeBatch =
                    desc.atlasStrokeBatches[i];
                CD3DX12_RECT scissorRect(strokeBatch.scissor.left,
                                         strokeBatch.scissor.top,
                                         strokeBatch.scissor.right,
                                         strokeBatch.scissor.bottom);
                cmdList->RSSetScissorRects(1, &scissorRect);
                cmdList->SetGraphicsRoot32BitConstant(
                    VERTEX_DRAW_UNIFORM_SIG_INDEX,
                    strokeBatch.basePatch,
                    0);
                cmdList->DrawIndexedInstanced(
                    gpu::kMidpointFanPatchBorderIndexCount,
                    strokeBatch.patchCount,
                    gpu::kMidpointFanPatchBaseIndex,
                    0,
                    strokeBatch.basePatch);
            }
        }
        m_resourceManager->transition(cmdList,
                                      m_atlasTexture.get(),
                                      D3D12_RESOURCE_STATE_GENERIC_READ);

        cmdList->OMSetRenderTargets(0, nullptr, FALSE, nullptr);
    }

    CD3DX12_VIEWPORT viewport(0.0f,
                              0.0f,
                              static_cast<float>(width),
                              static_cast<float>(height));
    CD3DX12_RECT scissorRect(desc.renderTargetUpdateBounds.left,
                             desc.renderTargetUpdateBounds.top,
                             desc.renderTargetUpdateBounds.right,
                             desc.renderTargetUpdateBounds.bottom);
    cmdList->RSSetViewports(1, &viewport);
    cmdList->RSSetScissorRects(1, &scissorRect);

    // Setup and clear the PLS textures.

    if (desc.atomicFixedFunctionColorOutput)
    {
        m_resourceManager->transition(cmdList,
                                      targetTexture,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHandle = m_rtvHeap->cpuHandleForIndex(TARGET_RTV_HEAP_OFFSET);
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        if (desc.colorLoadAction == gpu::LoadAction::clear)
        {
            float clearColor4f[4];
            UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);
            cmdList->ClearRenderTargetView(rtvHandle, clearColor4f, 0, nullptr);
        }
    }
    else // !desc.atomicFixedFunctionColorOutput
    {
        if (renderTarget->targetTextureSupportsUAV())
        {
            m_resourceManager->transition(
                cmdList,
                targetTexture,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }

        if (desc.colorLoadAction == gpu::LoadAction::clear)
        {
            auto tex = renderTarget->targetTextureSupportsUAV()
                           ? renderTarget->targetTexture()->resource()
                           : renderTarget->offscreenTexture()->resource();

            if (m_capabilities.supportsTypedUAVLoadStore)
            {
                float clearColor4f[4];
                UnpackColorToRGBA32FPremul(desc.colorClearValue, clearColor4f);

                auto gpuHandle = m_srvUavCbvHeap->gpuHandleForIndex(
                    ATOMIC_COLOR_HEAP_OFFSET);
                auto cpuHandle = m_cpuSrvUavCbvHeap->cpuHandleForIndex(
                    ATOMIC_COLOR_HEAP_OFFSET);
                m_resourceManager->clearUAV(cmdList,
                                            tex,
                                            gpuHandle,
                                            cpuHandle,
                                            clearColor4f,
                                            desc.interlockMode ==
                                                InterlockMode::atomics);
            }
            else
            {
                UINT clearColorui[4] = {
                    gpu::SwizzleRiveColorToRGBAPremul(desc.colorClearValue)};

                auto gpuHandle = m_srvUavCbvHeap->gpuHandleForIndex(
                    ATOMIC_COLOR_HEAP_OFFSET);
                auto cpuHandle = m_cpuSrvUavCbvHeap->cpuHandleForIndex(
                    ATOMIC_COLOR_HEAP_OFFSET);

                m_resourceManager->clearUAV(cmdList,
                                            tex,
                                            gpuHandle,
                                            cpuHandle,
                                            clearColorui,
                                            desc.interlockMode ==
                                                InterlockMode::atomics);
            }
        }
        if (desc.colorLoadAction == gpu::LoadAction::preserveRenderTarget &&
            !renderTarget->targetTextureSupportsUAV())
        {
            auto offscreenTex = renderTarget->offscreenTexture();
            blitSubRect(cmdList,
                        offscreenTex,
                        renderTarget->targetTexture(),
                        desc.renderTargetUpdateBounds);

            m_resourceManager->transition(
                cmdList,
                offscreenTex,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        }
    }

    bool renderPassHasCoalescedResolveAndTransfer =
        desc.interlockMode == gpu::InterlockMode::atomics &&
        !desc.atomicFixedFunctionColorOutput &&
        !renderTarget->targetTextureSupportsUAV();

    if (desc.combinedShaderFeatures & gpu::ShaderFeatures::ENABLE_CLIPPING)
    {
        constexpr static UINT kZero[4]{};
        auto gpuHandle =
            m_srvUavCbvHeap->gpuHandleForIndex(ATOMIC_CLIP_HEAP_OFFSET);
        auto cpuHandle =
            m_cpuSrvUavCbvHeap->cpuHandleForIndex(ATOMIC_CLIP_HEAP_OFFSET);
        m_resourceManager->clearUAV(cmdList,
                                    renderTarget->clip()->resource(),
                                    gpuHandle,
                                    cpuHandle,
                                    kZero,
                                    desc.interlockMode ==
                                        InterlockMode::atomics);
    }
    // always clear coverage
    {
        UINT coverageClear[4]{desc.coverageClearValue};
        auto gpuHandle =
            m_srvUavCbvHeap->gpuHandleForIndex(ATOMIC_COVERAGE_HEAP_OFFSET);
        auto cpuHandle =
            m_cpuSrvUavCbvHeap->cpuHandleForIndex(ATOMIC_COVERAGE_HEAP_OFFSET);

        m_resourceManager->clearUAV(cmdList,
                                    renderTarget->coverage()->resource(),
                                    gpuHandle,
                                    cpuHandle,
                                    coverageClear,
                                    desc.interlockMode ==
                                        InterlockMode::atomics);
    }

    if (renderPassHasCoalescedResolveAndTransfer)
    {
        m_resourceManager->transition(cmdList,
                                      targetTexture,
                                      D3D12_RESOURCE_STATE_RENDER_TARGET);

        auto rtvHandle = m_rtvHeap->cpuHandleForIndex(TARGET_RTV_HEAP_OFFSET);
        cmdList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);
    }

    m_heapDescriptorOffset += imageDescriptorOffset;

    for (const DrawBatch& batch : *desc.drawList)
    {
        assert(batch.elementCount != 0);

        DrawType drawType = batch.drawType;
        auto shaderFeatures = desc.interlockMode == gpu::InterlockMode::atomics
                                  ? desc.combinedShaderFeatures
                                  : batch.shaderFeatures;
        auto shaderMiscFlags = batch.shaderMiscFlags;
        if (drawType == gpu::DrawType::atomicResolve &&
            renderPassHasCoalescedResolveAndTransfer)
        {
            shaderMiscFlags |=
                gpu::ShaderMiscFlags::coalescedResolveAndTransfer;
        }
        if (desc.atomicFixedFunctionColorOutput)
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::fixedFunctionColorOutput;
        }
        if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
            (batch.drawContents & gpu::DrawContents::clockwiseFill))
        {
            shaderMiscFlags |= gpu::ShaderMiscFlags::clockwiseFill;
        }

        auto pipeline = m_pipelineManager.getPipeline({
            .drawType = drawType,
            .shaderFeatures = shaderFeatures,
            .interlockMode = desc.interlockMode,
            .shaderMiscFlags = shaderMiscFlags,
#ifdef WITH_RIVE_TOOLS
            .synthesizeCompilationFailures = desc.synthesizeCompilationFailures,
#endif
        });
        cmdList->SetPipelineState(pipeline.m_d3dPipelineState.Get());

        // all atomic barriers are the same for dx12
        if (batch.barriers &
            (BarrierFlags::plsAtomicPreResolve | BarrierFlags::plsAtomic))
        {
            assert(desc.interlockMode == gpu::InterlockMode::atomics);
            auto target = renderTarget->targetTextureSupportsUAV()
                              ? renderTarget->targetTexture()
                              : renderTarget->offscreenTexture();

            D3D12_RESOURCE_BARRIER barriers[] = {
                CD3DX12_RESOURCE_BARRIER::UAV(
                    renderTarget->coverage()->resource()),
                CD3DX12_RESOURCE_BARRIER::UAV(target->resource()),
                CD3DX12_RESOURCE_BARRIER::UAV(
                    renderTarget->clip()->resource())};

            cmdList->ResourceBarrier(
                desc.combinedShaderFeatures &
                        gpu::ShaderFeatures::ENABLE_CLIPPING
                    ? 3
                    : 2,
                barriers);
        }

        if (auto imageTextureD3D12 =
                static_cast<const TextureD3D12Impl*>(batch.imageTexture))
        {
            auto imageTexture =
                imageTextureD3D12->synchronize(copyCmdList,
                                               cmdList,
                                               m_resourceManager.get());
            RNAME_D3D12_OBJECT(imageTexture, m_heapDescriptorOffset);
            if (m_heapDescriptorOffset >= MAX_DESCRIPTOR_HEAPS_PER_FLUSH)
            {
                fprintf(stderr,
                        "heap descriptor ran out of room ! heapOffset %i "
                        "maxHeap %i\n",
                        m_heapDescriptorOffset,
                        MAX_DESCRIPTOR_HEAPS_PER_FLUSH);
                assert(false);
                // break out of loop and let transitions happen
                break;
            }
            m_srvUavCbvHeap->markSrvToIndex(m_device.Get(),
                                            imageTexture,
                                            m_heapDescriptorOffset);
            cmdList->SetGraphicsRootDescriptorTable(
                IMAGE_SIG_INDEX,
                m_srvUavCbvHeap->gpuHandleForIndex(m_heapDescriptorOffset));
            ++m_heapDescriptorOffset;

            // we want to update this as little as possible, so only set it if
            // it's changed
            if (m_lastDynamicSampler != batch.imageSampler)
            {
                if (++m_samplerHeapDescriptorOffset >=
                    MAX_DESCRIPTOR_SAMPLER_HEAPS_PER_FLUSH)
                {
                    auto oldHeap = m_samplerHeap;
                    m_samplerHeap = m_samplerHeapPool.acquire();
                    m_samplerHeapDescriptorOffset = IMAGE_SAMPLER_HEAP_OFFSET;
                    // copy the imutable sampelrs to the new heap
                    m_device->CopyDescriptorsSimple(
                        IMAGE_SAMPLER_HEAP_OFFSET,
                        m_samplerHeap->cpuHandleForUpload(0),
                        oldHeap->cpuHandleForUpload(0),
                        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);

                    ID3D12DescriptorHeap* ppHeaps[] = {m_srvUavCbvHeap->heap(),
                                                       m_samplerHeap->heap()};

                    cmdList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);
                }

                m_samplerHeap->markSamplerToIndex(
                    m_device.Get(),
                    m_imageSamplers[batch.imageSampler.asKey()],
                    m_samplerHeapDescriptorOffset);

                cmdList->SetGraphicsRootDescriptorTable(
                    DYNAMIC_SAMPLER_SIG_INDEX,
                    m_samplerHeap->gpuHandleForIndex(
                        m_samplerHeapDescriptorOffset));

                m_lastDynamicSampler = batch.imageSampler;
            }
        }

        switch (drawType)
        {
            case DrawType::midpointFanPatches:
            case DrawType::midpointFanCenterAAPatches:
            case DrawType::outerCurvePatches:
            {
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto IBV = m_pathPatchIndexBuffer->indexBufferView();
                cmdList->IASetIndexBuffer(&IBV);
                cmdList->SetGraphicsRoot32BitConstant(
                    VERTEX_DRAW_UNIFORM_SIG_INDEX,
                    batch.baseElement,
                    0);
                cmdList->DrawIndexedInstanced(PatchIndexCount(drawType),
                                              batch.elementCount,
                                              PatchBaseIndex(drawType),
                                              0,
                                              batch.baseElement);
                break;
            }
            case DrawType::interiorTriangulation:
            case DrawType::atlasBlit:
            {
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                cmdList->DrawInstanced(batch.elementCount,
                                       1,
                                       batch.baseElement,
                                       0);
                break;
            }
            case DrawType::imageRect:
            {
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto IBV = m_imageRectIndexBuffer->indexBufferView();
                cmdList->IASetIndexBuffer(&IBV);

                cmdList->SetGraphicsRootConstantBufferView(
                    IMAGE_UNIFORM_BUFFFER_SIG_INDEX,
                    m_imageDrawUniformBuffer->resource()
                            ->getGPUVirtualAddress() +
                        batch.imageDrawDataOffset);

                cmdList->DrawIndexedInstanced(std::size(gpu::kImageRectIndices),
                                              1,
                                              0,
                                              0,
                                              0);
                break;
            }
            case DrawType::imageMesh:
            {
                LITE_RTTI_CAST_OR_BREAK(vertexBuffer,
                                        RenderBufferD3D12Impl*,
                                        batch.vertexBuffer);
                LITE_RTTI_CAST_OR_BREAK(uvBuffer,
                                        RenderBufferD3D12Impl*,
                                        batch.uvBuffer);
                LITE_RTTI_CAST_OR_BREAK(indexBuffer,
                                        RenderBufferD3D12Impl*,
                                        batch.indexBuffer);

                auto vBuffer = vertexBuffer->sync(copyCmdList);
                auto uBuffer = uvBuffer->sync(copyCmdList);
                auto iBuffer = indexBuffer->sync(copyCmdList);

                D3D12_VERTEX_BUFFER_VIEW imageMeshBuffers[] = {
                    vBuffer->vertexBufferView(0, sizeof(Vec2D)),
                    uBuffer->vertexBufferView(0, sizeof(Vec2D))};
                cmdList->IASetVertexBuffers(IMAGE_MESH_VERTEX_DATA_SLOT,
                                            2,
                                            imageMeshBuffers);
                static_assert(IMAGE_MESH_UV_DATA_SLOT ==
                              IMAGE_MESH_VERTEX_DATA_SLOT + 1);
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
                auto IBV = iBuffer->indexBufferView();
                cmdList->IASetIndexBuffer(&IBV);

                cmdList->SetGraphicsRootConstantBufferView(
                    IMAGE_UNIFORM_BUFFFER_SIG_INDEX,
                    m_imageDrawUniformBuffer->resource()
                            ->getGPUVirtualAddress() +
                        batch.imageDrawDataOffset);

                cmdList->DrawIndexedInstanced(batch.elementCount,
                                              1,
                                              batch.baseElement,
                                              0,
                                              0);
                break;
            }
            case DrawType::atomicResolve:
                assert(desc.interlockMode == gpu::InterlockMode::atomics);
                cmdList->IASetPrimitiveTopology(
                    D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                cmdList->DrawInstanced(4, 1, 0, 0);
                break;
            case DrawType::atomicInitialize:
            case DrawType::msaaStrokes:
            case DrawType::msaaMidpointFanBorrowedCoverage:
            case DrawType::msaaMidpointFans:
            case DrawType::msaaMidpointFanStencilReset:
            case DrawType::msaaMidpointFanPathsStencil:
            case DrawType::msaaMidpointFanPathsCover:
            case DrawType::msaaOuterCubics:
            case DrawType::msaaStencilClipReset:
                RIVE_UNREACHABLE();
        }
    }

    if (desc.interlockMode == gpu::InterlockMode::rasterOrdering &&
        !renderTarget->targetTextureSupportsUAV())
    {
        // We rendered to an offscreen UAV and did not resolve to the
        // renderTarget. Copy back to the main target.
        assert(!desc.atomicFixedFunctionColorOutput);
        assert(!renderPassHasCoalescedResolveAndTransfer);
        blitSubRect(cmdList,
                    renderTarget->targetTexture(),
                    renderTarget->offscreenTexture(),
                    desc.renderTargetUpdateBounds);

        m_resourceManager->transition(cmdList,
                                      targetTexture,
                                      D3D12_RESOURCE_STATE_COMMON);

        // we create the offscreen texture in UAV state and always assume it is
        // in it. so put it back
        m_resourceManager->transition(cmdList,
                                      renderTarget->offscreenTexture(),
                                      D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    }

    if (desc.atomicFixedFunctionColorOutput ||
        renderPassHasCoalescedResolveAndTransfer ||
        (renderTarget->targetTextureSupportsUAV() &&
         !desc.atomicFixedFunctionColorOutput))
    {
        m_resourceManager->transition(cmdList,
                                      targetTexture,
                                      D3D12_RESOURCE_STATE_COMMON);
    }
}

void RenderContextD3D12Impl::postFlush(const RenderContext::FlushResources&)
{
    // Recycle buffers.
    m_flushUniformBufferPool.recycle(std::move(m_flushUniformBuffer));
    m_imageDrawUniformBufferPool.recycle(std::move(m_imageDrawUniformBuffer));
    m_pathBufferPool.recycle(std::move(m_pathBuffer));
    m_paintBufferPool.recycle(std::move(m_paintBuffer));
    m_paintAuxBufferPool.recycle(std::move(m_paintAuxBuffer));
    m_contourBufferPool.recycle(std::move(m_contourBuffer));
    m_gradSpanBufferPool.recycle(std::move(m_gradSpanBuffer));
    m_tessSpanBufferPool.recycle(std::move(m_tessSpanBuffer));
    m_triangleBufferPool.recycle(std::move(m_triangleBuffer));

    m_srvUavCbvHeapPool.recycle(std::move(m_srvUavCbvHeap));
    m_cpuSrvUavCbvHeapPool.recycle(std::move(m_cpuSrvUavCbvHeap));
    m_samplerHeapPool.recycle(std::move(m_samplerHeap));
}
}; // namespace rive::gpu