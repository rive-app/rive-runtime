/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/render_context.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "intersection_board.hpp"
#include "gradient.hpp"
#include "rive_render_paint.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/rive_render_image.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/profiler/profiler_macros.h"

#include "shaders/constants.glsl"

#include <string_view>

#ifdef RIVE_DECODERS
#include "rive/decoders/bitmap_decoder.hpp"
#endif

namespace rive::gpu
{
constexpr size_t kDefaultSimpleGradientCapacity = 512;
constexpr size_t kDefaultComplexGradientCapacity = 1024;
constexpr size_t kDefaultDrawCapacity = 2048;

// TODO: Move this variable to PlatformFeatures.
constexpr uint32_t kMaxTextureHeight = 2048;
constexpr size_t kMaxTessellationVertexCount =
    kMaxTextureHeight * kTessTextureWidth;
constexpr size_t kMaxTessellationPaddingVertexCount =
    gpu::kMidpointFanPatchSegmentSpan + // Padding at the beginning of the tess
                                        // texture
    (gpu::kOuterCurvePatchSegmentSpan -
     1) + // Max padding between patch types in the tess texture
    1;    // Padding at the end of the tessellation texture
constexpr size_t kMaxTessellationVertexCountBeforePadding =
    kMaxTessellationVertexCount - kMaxTessellationPaddingVertexCount;

// Metal requires vertex buffers to be 256-byte aligned.
constexpr size_t kMaxTessellationAlignmentVertices =
    gpu::kTessVertexBufferAlignmentInElements - 1;

// We can only reorder 32767 draws at a time since the one-based groupIndex
// returned by IntersectionBoard is a signed 16-bit integer.
constexpr size_t kMaxReorderedDrawPassCount =
    std::numeric_limits<int16_t>::max();

// How tall to make a resource texture in order to support the given number of
// items.
template <size_t WidthInItems>
constexpr static size_t resource_texture_height(size_t itemCount)
{
    return (itemCount + WidthInItems - 1) / WidthInItems;
}

constexpr static size_t gradient_data_height(size_t simpleRampCount,
                                             size_t complexRampCount)
{
    return resource_texture_height<gpu::kGradTextureWidthInSimpleRamps>(
               simpleRampCount) +
           complexRampCount;
}

inline GradientContentKey::GradientContentKey(rcp<const Gradient> gradient) :
    m_gradient(std::move(gradient))
{}

inline GradientContentKey::GradientContentKey(GradientContentKey&& other) :
    m_gradient(std::move(other.m_gradient))
{}

bool GradientContentKey::operator==(const GradientContentKey& other) const
{
    if (m_gradient.get() == other.m_gradient.get())
    {
        return true;
    }
    else
    {
        return m_gradient->count() == other.m_gradient->count() &&
               !memcmp(m_gradient->stops(),
                       other.m_gradient->stops(),
                       m_gradient->count() * sizeof(float)) &&
               !memcmp(m_gradient->colors(),
                       other.m_gradient->colors(),
                       m_gradient->count() * sizeof(ColorInt));
    }
}

size_t DeepHashGradient::operator()(const GradientContentKey& key) const
{
    const Gradient* grad = key.gradient();
    std::hash<std::string_view> hash;
    size_t x =
        hash(std::string_view(reinterpret_cast<const char*>(grad->stops()),
                              grad->count() * sizeof(float)));
    size_t y =
        hash(std::string_view(reinterpret_cast<const char*>(grad->colors()),
                              grad->count() * sizeof(ColorInt)));
    return x ^ y;
}

RenderContext::RenderContext(std::unique_ptr<RenderContextImpl> impl) :
    m_impl(std::move(impl)),
    // -1 from m_maxPathID so we reserve a path record for the clearColor paint
    // (for atomic mode). This also allows us to index the storage buffers
    // directly by pathID.
    m_maxPathID(MaxPathID(m_impl->platformFeatures().pathIDGranularity) - 1)
{
    setResourceSizes(ResourceAllocationCounts(), /*forceRealloc =*/true);
    releaseResources();
}

RenderContext::~RenderContext()
{
    // Always call flush() to avoid deadlock.
    assert(!m_didBeginFrame);
    // Delete the logical flushes before the block allocators let go of their
    // allocations.
    m_logicalFlushes.clear();
}

const gpu::PlatformFeatures& RenderContext::platformFeatures() const
{
    return m_impl->platformFeatures();
}

rcp<RenderBuffer> RenderContext::makeRenderBuffer(RenderBufferType type,
                                                  RenderBufferFlags flags,
                                                  size_t sizeInBytes)
{
    return m_impl->makeRenderBuffer(type, flags, sizeInBytes);
}

rcp<RenderImage> RenderContext::decodeImage(Span<const uint8_t> encodedBytes)
{
    RIVE_PROF_SCOPE()
    rcp<Texture> texture = m_impl->platformDecodeImageTexture(encodedBytes);
#ifdef RIVE_DECODERS
    if (texture == nullptr)
    {
        auto bitmap = Bitmap::decode(encodedBytes.data(), encodedBytes.size());
        if (bitmap)
        {
            // For now, RenderContextImpl::makeImageTexture() only accepts RGBA.
            if (bitmap->pixelFormat() != Bitmap::PixelFormat::RGBAPremul)
            {
                bitmap->pixelFormat(Bitmap::PixelFormat::RGBAPremul);
            }
            uint32_t width = bitmap->width();
            uint32_t height = bitmap->height();
            uint32_t mipLevelCount = math::msb(height | width);
            texture = m_impl->makeImageTexture(width,
                                               height,
                                               mipLevelCount,
                                               bitmap->bytes());
        }
    }
#endif
    return texture != nullptr ? make_rcp<RiveRenderImage>(std::move(texture))
                              : nullptr;
}

void RenderContext::releaseResources()
{
    assert(!m_didBeginFrame);
    resetContainers();
    setResourceSizes(ResourceAllocationCounts());
    m_maxRecentResourceRequirements = ResourceAllocationCounts();
    m_lastResourceTrimTimeInSeconds = m_impl->secondsNow();
}

void RenderContext::resetContainers()
{
    assert(!m_didBeginFrame);

    if (!m_logicalFlushes.empty())
    {
        // Should get reset to 1 after flush().
        assert(m_logicalFlushes.size() == 1);
        m_logicalFlushes.resize(1);
        m_logicalFlushes.front()->resetContainers();
    }

    m_indirectDrawList.clear();
    m_indirectDrawList.shrink_to_fit();

    m_intersectionBoard = nullptr;
}

RenderContext::LogicalFlush::LogicalFlush(RenderContext* parent) : m_ctx(parent)
{
    rewind();
}

void RenderContext::LogicalFlush::rewind()
{
    m_resourceCounts = Draw::ResourceCounters();
    m_drawPassCount = 0;
    m_simpleGradients.clear();
    m_pendingSimpleGradDraws.clear();
    m_complexGradients.clear();
    m_pendingComplexGradDraws.clear();
    m_pendingGradSpanCount = 0;
    m_clips.clear();
    m_draws.clear();
    m_combinedDrawBounds = {std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::min(),
                            std::numeric_limits<int32_t>::min()};

    m_pathPaddingCount = 0;
    m_paintPaddingCount = 0;
    m_paintAuxPaddingCount = 0;
    m_contourPaddingCount = 0;
    m_gradSpanPaddingCount = 0;
    m_midpointFanTessEndLocation = 0;
    m_outerCubicTessEndLocation = 0;
    m_outerCubicTessVertexIdx = 0;
    m_midpointFanTessVertexIdx = 0;

    m_flushDesc = FlushDescriptor();

    m_drawList.reset();
    m_combinedShaderFeatures = gpu::ShaderFeatures::NONE;

    m_currentPathID = 0;
    m_currentContourID = 0;

    if (m_atlasRectanizer != nullptr)
    {
        m_atlasRectanizer->reset();
    }
    m_atlasMaxX = 0;
    m_atlasMaxY = 0;
    m_pendingAtlasDraws.clear();

    m_coverageBufferLength = 0;

    m_pendingBarriers = BarrierFlags::none;

    m_currentZIndex = 0;

    RIVE_DEBUG_CODE(m_hasDoneLayout = false;)
}

void RenderContext::LogicalFlush::resetContainers()
{
    m_clips.clear();
    m_clips.shrink_to_fit();
    m_draws.clear();
    m_draws.shrink_to_fit();
    m_draws.reserve(kDefaultDrawCapacity);

    m_simpleGradients.rehash(0);
    m_simpleGradients.reserve(kDefaultSimpleGradientCapacity);

    m_pendingSimpleGradDraws.clear();
    m_pendingSimpleGradDraws.shrink_to_fit();
    m_pendingSimpleGradDraws.reserve(kDefaultSimpleGradientCapacity);

    m_complexGradients.rehash(0);
    m_complexGradients.reserve(kDefaultComplexGradientCapacity);

    m_pendingComplexGradDraws.clear();
    m_pendingComplexGradDraws.shrink_to_fit();
    m_pendingComplexGradDraws.reserve(kDefaultComplexGradientCapacity);

    m_pendingAtlasDraws.clear();
    m_pendingAtlasDraws.shrink_to_fit();
    // Don't reserve any space in m_pendingAtlasDraws since there are many
    // usecases where it isn't used at all.
}

void RenderContext::beginFrame(const FrameDescriptor& frameDescriptor)
{
    RIVE_PROF_SCOPE()
    assert(!m_didBeginFrame);
    assert(frameDescriptor.renderTargetWidth > 0);
    assert(frameDescriptor.renderTargetHeight > 0);
    m_frameDescriptor = frameDescriptor;
    if (!platformFeatures().supportsRasterOrdering &&
        !platformFeatures().supportsFragmentShaderAtomics)
    {
        // We don't have pixel local storage in any form. Use 4x MSAA if
        // msaaSampleCount wasn't already specified.
        m_frameDescriptor.msaaSampleCount =
            m_frameDescriptor.msaaSampleCount > 0
                ? m_frameDescriptor.msaaSampleCount
                : 4;
    }
    if (m_frameDescriptor.msaaSampleCount > 0)
    {
        m_frameInterlockMode = gpu::InterlockMode::msaa;
    }
    else if (platformFeatures().supportsRasterOrdering &&
             (!m_frameDescriptor.disableRasterOrdering ||
              !platformFeatures().supportsFragmentShaderAtomics))
    {
        m_frameInterlockMode = gpu::InterlockMode::rasterOrdering;
    }
    else if (frameDescriptor.clockwiseFillOverride &&
             platformFeatures().supportsClockwiseAtomicRendering)
    {
        assert(platformFeatures().supportsFragmentShaderAtomics);
        m_frameInterlockMode = gpu::InterlockMode::clockwiseAtomic;
    }
    else
    {
        assert(platformFeatures().supportsFragmentShaderAtomics);
        m_frameInterlockMode = gpu::InterlockMode::atomics;
    }
    m_frameShaderFeaturesMask =
        gpu::ShaderFeaturesMaskFor(m_frameInterlockMode);
    if (m_logicalFlushes.empty())
    {
        m_logicalFlushes.emplace_back(new LogicalFlush(this));
    }
    RIVE_DEBUG_CODE(m_didBeginFrame = true);
}

bool RenderContext::isOutsideCurrentFrame(const IAABB& pixelBounds)
{
    assert(m_didBeginFrame);
    int4 bounds = simd::load4i(&pixelBounds);
    auto renderTargetSize =
        simd::cast<int32_t>(uint2{m_frameDescriptor.renderTargetWidth,
                                  m_frameDescriptor.renderTargetHeight});
    return simd::any(bounds.xy >= renderTargetSize || bounds.zw <= 0 ||
                     bounds.xy >= bounds.zw);
}

bool RenderContext::frameSupportsClipRects() const
{
    assert(m_didBeginFrame);
    return m_frameInterlockMode != gpu::InterlockMode::msaa ||
           platformFeatures().supportsClipPlanes;
}

bool RenderContext::frameSupportsImagePaintForPaths() const
{
    assert(m_didBeginFrame);
    return m_frameInterlockMode != gpu::InterlockMode::atomics;
}

uint32_t RenderContext::generateClipID(const IAABB& contentBounds)
{
    assert(m_didBeginFrame);
    assert(!m_logicalFlushes.empty());
    return m_logicalFlushes.back()->generateClipID(contentBounds);
}

uint32_t RenderContext::LogicalFlush::generateClipID(const IAABB& contentBounds)
{
    if (m_clips.size() < m_ctx->m_maxPathID) // maxClipID == maxPathID.
    {
        m_clips.emplace_back(contentBounds);
        assert(m_ctx->m_clipContentID != m_clips.size());
        return math::lossless_numeric_cast<uint32_t>(m_clips.size());
    }
    return 0; // There are no available clip IDs. The caller should flush and
              // try again.
}

RenderContext::LogicalFlush::ClipInfo& RenderContext::LogicalFlush::
    getWritableClipInfo(uint32_t clipID)
{
    assert(clipID > 0);
    assert(clipID <= m_clips.size());
    return m_clips[clipID - 1];
}

void RenderContext::LogicalFlush::addClipReadBounds(uint32_t clipID,
                                                    const IAABB& bounds)
{
    assert(clipID > 0);
    assert(clipID <= m_clips.size());
    ClipInfo& clipInfo = getWritableClipInfo(clipID);
    clipInfo.readBounds = clipInfo.readBounds.join(bounds);
}

bool RenderContext::pushDraws(DrawUniquePtr draws[], size_t drawCount)
{
    assert(m_didBeginFrame);
    assert(!m_logicalFlushes.empty());
    return m_logicalFlushes.back()->pushDraws(draws, drawCount);
}

bool RenderContext::LogicalFlush::pushDraws(DrawUniquePtr draws[],
                                            size_t drawCount)
{
    RIVE_PROF_SCOPE()
    assert(!m_hasDoneLayout);

    auto countsVector = m_resourceCounts.toVec();
    for (size_t i = 0; i < drawCount; ++i)
    {
        assert(!draws[i]->pixelBounds().empty());
        assert(m_ctx->frameSupportsClipRects() ||
               draws[i]->clipRectInverseMatrix() == nullptr);
        countsVector += draws[i]->resourceCounts().toVec();
    }
    Draw::ResourceCounters countsWithNewBatch = countsVector;

    // Textures and buffers have hard size limits. If the new batch doesn't fit
    // within our constraints, the caller needs to flush and try again.
    if (countsWithNewBatch.pathCount > m_ctx->m_maxPathID ||
        countsWithNewBatch.contourCount > kMaxContourID ||
        countsWithNewBatch.midpointFanTessVertexCount +
                countsWithNewBatch.outerCubicTessVertexCount >
            kMaxTessellationVertexCountBeforePadding)
    {
        return false;
    }

    // Allocate subpasses.
    int passCountInBatch = 0;
    for (size_t i = 0; i < drawCount; ++i)
    {
        draws[i]->countSubpasses();
        assert(draws[i]->prepassCount() >= 0);
        assert(draws[i]->subpassCount() >= 0);
        assert(draws[i]->prepassCount() + draws[i]->subpassCount() >= 1);
        passCountInBatch += draws[i]->prepassCount() + draws[i]->subpassCount();
    }

    // We can only reorder 32k draws at a time in atomic and msaa modes since
    // the sort key addresses them with a signed 16-bit index. Make sure we
    // don't exceed that limit.
    if (m_ctx->frameInterlockMode() != gpu::InterlockMode::rasterOrdering &&
        m_drawPassCount + passCountInBatch > kMaxReorderedDrawPassCount)
    {
        return false;
    }

    // Allocate final resources.
    for (size_t i = 0; i < drawCount; ++i)
    {
        if (!draws[i]->allocateResources(this))
        {
            // The draw failed to allocate resources. Give up and let the caller
            // flush and try again.
            //
            // FIXME: This works today, but the surrounding code could be
            // modified to inadvertently leave a stale dangling reference to one
            // of these draws in m_pendingAtlasDraws. This needs to be
            // revisited.
            return false;
        }
    }

    for (size_t i = 0; i < drawCount; ++i)
    {
        m_draws.push_back(std::move(draws[i]));
        m_combinedDrawBounds =
            m_combinedDrawBounds.join(m_draws.back()->pixelBounds());
    }

    m_resourceCounts = countsWithNewBatch;
    m_drawPassCount += passCountInBatch;
    return true;
}

bool RenderContext::LogicalFlush::allocateGradient(
    const Gradient* gradient,
    gpu::ColorRampLocation* colorRampLocation)
{
    RIVE_PROF_SCOPE()
    assert(!m_hasDoneLayout);

    const float* stops = gradient->stops();
    size_t stopCount = gradient->count();
    assert(stopCount > 0); // RiveRenderFactory guarantees this.

    if (stopCount == 1 || (stopCount == 2 && stops[0] == 0 && stops[1] == 1))
    {
        // This is a simple gradient that can be implemented by a two-texel
        // color ramp.
        const ColorInt* colors = gradient->colors();
        TwoTexelRamp colorRamp = {colors[0],
                                  // Handle ramps with a single stop.
                                  colors[std::min<size_t>(1, stopCount - 1)]};
        uint64_t simpleKey;
        static_assert(sizeof(simpleKey) == sizeof(ColorInt) * 2);
        RIVE_INLINE_MEMCPY(&simpleKey, &colorRamp, sizeof(ColorInt) * 2);
        uint32_t rampTexelsIdx;
        auto iter = m_simpleGradients.find(simpleKey);
        if (iter != m_simpleGradients.end())
        {
            // This gradient is already in the texture.
            rampTexelsIdx = iter->second;
        }
        else
        {
            if (gradient_data_height(m_simpleGradients.size() + 1,
                                     m_complexGradients.size()) >
                kMaxTextureHeight)
            {
                // We ran out of rows in the gradient texture. Caller has to
                // flush and try again.
                return false;
            }
            rampTexelsIdx = math::lossless_numeric_cast<uint32_t>(
                m_simpleGradients.size() * 2);
            m_simpleGradients.insert({simpleKey, rampTexelsIdx});
            m_pendingSimpleGradDraws.push_back(colorRamp);
            // Simple gradients get uploaded to the GPU as a single GradientSpan
            // instance.
            ++m_pendingGradSpanCount;
        }
        colorRampLocation->row = rampTexelsIdx / kGradTextureWidth;
        colorRampLocation->col = rampTexelsIdx % kGradTextureWidth;
    }
    else
    {
        // This is a complex gradient. Render it to an entire row of the
        // gradient texture.
        GradientContentKey key(ref_rcp(gradient));
        auto iter = m_complexGradients.find(key);
        uint16_t row;
        if (iter != m_complexGradients.end())
        {
            row = iter->second; // This gradient is already in the texture.
        }
        else
        {
            if (gradient_data_height(m_simpleGradients.size(),
                                     m_complexGradients.size() + 1) >
                kMaxTextureHeight)
            {
                // We ran out of rows in the gradient texture. Caller has to
                // flush and try again.
                return false;
            }

            row = static_cast<uint32_t>(m_complexGradients.size());
            m_complexGradients.emplace(std::move(key), row);
            m_pendingComplexGradDraws.push_back(gradient);

            size_t spanCount = stopCount - 1;
            m_pendingGradSpanCount += spanCount;
        }
        // Store the row relative to the first complex gradient for now.
        // PaintData::set() will offset this value by the number of simple
        // gradient rows once its final value is known.
        colorRampLocation->row = row;
        colorRampLocation->col = ColorRampLocation::kComplexGradientMarker;
    }
    return true;
}

bool RenderContext::LogicalFlush::allocateAtlasDraw(
    PathDraw* pathDraw,
    uint16_t drawWidth,
    uint16_t drawHeight,
    uint16_t desiredPadding,
    uint16_t* x,
    uint16_t* y,
    TAABB<uint16_t>* paddedRegion)
{
    RIVE_PROF_SCOPE()

    if (m_atlasRectanizer == nullptr)
    {
        uint16_t atlasMaxSize = m_ctx->atlasMaxSize();
        // Use an atlas larger than atlasMaxSize if it's too small for the
        // request (meaning the render target is larger than atlasMaxSize).
        m_atlasRectanizer = std::make_unique<skgpu::RectanizerSkyline>(
            std::max(atlasMaxSize, drawWidth),
            std::max(atlasMaxSize, drawHeight));
    }

    const uint16_t atlasMaxWidth = m_atlasRectanizer->width();
    const uint16_t atlasMaxHeight = m_atlasRectanizer->height();
    uint16_t paddedWidth =
        std::min<uint16_t>(drawWidth + desiredPadding * 2, atlasMaxWidth);
    uint16_t paddedHeight =
        std::min<uint16_t>(drawHeight + desiredPadding * 2, atlasMaxHeight);
    int16_t ix, iy;
    if (!m_atlasRectanizer->addRect(paddedWidth, paddedHeight, &ix, &iy))
    {
        // Delete the rectanizer of it wasn't big enough for this path. It will
        // be reallocated to a large enough size on the next call.
        if (drawWidth > atlasMaxWidth || drawHeight > atlasMaxHeight)
        {
            m_atlasRectanizer = nullptr;
        }
        m_atlasRectanizer = nullptr;
        return false;
    }

    assert(ix >= 0);
    assert(iy >= 0);
    assert(ix + paddedWidth <= atlasMaxWidth);
    assert(iy + paddedHeight <= atlasMaxHeight);

    *x = ix + (paddedWidth - drawWidth) / 2;
    *y = iy + (paddedHeight - drawHeight) / 2;
    *paddedRegion = {ix, iy, ix + paddedWidth, iy + paddedHeight};
    assert((TAABB<uint16_t>{0, 0, atlasMaxWidth, atlasMaxHeight})
               .contains(*paddedRegion));

    m_atlasMaxX = std::max<uint32_t>(m_atlasMaxX, paddedRegion->right);
    m_atlasMaxY = std::max<uint32_t>(m_atlasMaxY, paddedRegion->bottom);
    assert(m_atlasMaxX <= atlasMaxWidth);
    assert(m_atlasMaxY <= atlasMaxHeight);

    m_pendingAtlasDraws.push_back(pathDraw);
    return true;
}

size_t RenderContext::LogicalFlush::allocateCoverageBufferRange(size_t length)
{
    RIVE_PROF_SCOPE()
    assert(m_ctx->frameInterlockMode() == gpu::InterlockMode::clockwiseAtomic);
    assert(length % (32 * 32) == 0u); // Allocations must support 32x32 tiles.
    uint32_t offset = m_coverageBufferLength;
    if (offset + length > m_ctx->platformFeatures().maxCoverageBufferLength)
    {
        return -1;
    }
    m_coverageBufferLength += length;
    return offset;
}

void RenderContext::logicalFlush()
{
    assert(m_didBeginFrame);

    // Reset clipping state after every logical flush because the clip buffer is
    // not preserved between render passes.
    m_clipContentID = 0;

    // Don't issue any GPU commands between logical flushes. Instead, build up a
    // list of flushes that we will submit all at once at the end of the frame.
    m_logicalFlushes.emplace_back(new LogicalFlush(this));
}

void RenderContext::flush(const FlushResources& flushResources)
{
    RIVE_PROF_SCOPE()
    assert(m_didBeginFrame);
    assert(flushResources.renderTarget->width() ==
           m_frameDescriptor.renderTargetWidth);
    assert(flushResources.renderTarget->height() ==
           m_frameDescriptor.renderTargetHeight);

    m_clipContentID = 0;

    // Layout this frame's resource buffers and textures.
    LogicalFlush::ResourceCounters totalFrameResourceCounts;
    LogicalFlush::LayoutCounters layoutCounts;
    for (size_t i = 0; i < m_logicalFlushes.size(); ++i)
    {
        m_logicalFlushes[i]->layoutResources(flushResources,
                                             i,
                                             &totalFrameResourceCounts,
                                             &layoutCounts);
    }

    // Determine the minimum required resource allocation sizes to service this
    // flush.
    ResourceAllocationCounts resourceRequirements;
    resourceRequirements.flushUniformBufferCount = m_logicalFlushes.size();
    resourceRequirements.imageDrawUniformBufferCount =
        totalFrameResourceCounts.imageDrawCount;
    resourceRequirements.pathBufferCount =
        totalFrameResourceCounts.pathCount + layoutCounts.pathPaddingCount;
    resourceRequirements.paintBufferCount =
        totalFrameResourceCounts.pathCount + layoutCounts.paintPaddingCount;
    resourceRequirements.paintAuxBufferCount =
        totalFrameResourceCounts.pathCount + layoutCounts.paintAuxPaddingCount;
    resourceRequirements.contourBufferCount =
        totalFrameResourceCounts.contourCount +
        layoutCounts.contourPaddingCount;
    resourceRequirements.gradSpanBufferCount =
        layoutCounts.gradSpanCount + layoutCounts.gradSpanPaddingCount;
    resourceRequirements.tessSpanBufferCount =
        totalFrameResourceCounts.maxTessellatedSegmentCount;
    resourceRequirements.triangleVertexBufferCount =
        totalFrameResourceCounts.maxTriangleVertexCount;
    resourceRequirements.gradTextureHeight = layoutCounts.maxGradTextureHeight;
    resourceRequirements.tessTextureHeight = layoutCounts.maxTessTextureHeight;
    resourceRequirements.atlasTextureWidth = layoutCounts.maxAtlasWidth;
    resourceRequirements.atlasTextureHeight = layoutCounts.maxAtlasHeight;
    resourceRequirements.coverageBufferLength =
        layoutCounts.maxCoverageBufferLength;

    // Ensure we're within hardware limits.
    assert(resourceRequirements.gradTextureHeight <= kMaxTextureHeight);
    assert(resourceRequirements.tessTextureHeight <= kMaxTextureHeight);
    assert(resourceRequirements.atlasTextureWidth <= atlasMaxSize() ||
           resourceRequirements.atlasTextureWidth <=
               frameDescriptor().renderTargetWidth);
    assert(resourceRequirements.atlasTextureHeight <= atlasMaxSize() ||
           resourceRequirements.atlasTextureHeight <=
               frameDescriptor().renderTargetHeight);
    assert(resourceRequirements.coverageBufferLength <=
           platformFeatures().maxCoverageBufferLength);

    // Track m_maxRecentResourceRequirements so we can trim GPU allocations when
    // steady-state usage goes down.
    m_maxRecentResourceRequirements =
        simd::max(resourceRequirements.toVec(),
                  m_maxRecentResourceRequirements.toVec());

    // Grow resources enough to handle this flush.
    // If "allocs" already fits in our current allocations, then don't change
    // them. If they don't fit, overallocate by 25% in order to create some
    // slack for growth.
    ResourceAllocationCounts allocs = simd::if_then_else(
        resourceRequirements.toVec() <= m_currentResourceAllocations.toVec(),
        m_currentResourceAllocations.toVec(),
        resourceRequirements.toVec() * size_t(5) / size_t(4));

    // In case the 25% growth pushed us above limits.
    allocs.gradTextureHeight =
        std::min<size_t>(allocs.gradTextureHeight, kMaxTextureHeight);
    allocs.tessTextureHeight =
        std::min<size_t>(allocs.tessTextureHeight, kMaxTextureHeight);
    allocs.atlasTextureWidth = std::min<size_t>(
        allocs.atlasTextureWidth,
        std::max(atlasMaxSize(), frameDescriptor().renderTargetWidth));
    allocs.atlasTextureHeight = std::min<size_t>(
        allocs.atlasTextureHeight,
        std::max(atlasMaxSize(), frameDescriptor().renderTargetHeight));
    allocs.coverageBufferLength =
        std::min(allocs.coverageBufferLength,
                 platformFeatures().maxCoverageBufferLength);

    // Additionally, every 5 seconds, trim resources down to the most recent
    // steady-state usage.
    double flushTime = m_impl->secondsNow();
    bool needsResourceTrim = flushTime - m_lastResourceTrimTimeInSeconds >= 5;
    if (needsResourceTrim)
    {
        // Trim GPU resource allocations to 125% of their maximum recent usage,
        // and only if the recent usage is 2/3 or less of the current
        // allocation.
        allocs = simd::if_then_else(m_maxRecentResourceRequirements.toVec() <=
                                        allocs.toVec() * size_t(2) / size_t(3),
                                    m_maxRecentResourceRequirements.toVec() *
                                        size_t(5) / size_t(4),
                                    allocs.toVec());

        // Ensure we stayed within limits.
        assert(allocs.gradTextureHeight <= kMaxTextureHeight);
        assert(allocs.tessTextureHeight <= kMaxTextureHeight);
        assert(allocs.atlasTextureWidth <= atlasMaxSize() ||
               allocs.atlasTextureWidth <= frameDescriptor().renderTargetWidth);
        assert(allocs.atlasTextureHeight <= atlasMaxSize() ||
               allocs.atlasTextureHeight <=
                   frameDescriptor().renderTargetHeight);
        assert(allocs.coverageBufferLength <=
               platformFeatures().maxCoverageBufferLength);

        // Zero out m_maxRecentResourceRequirements for the next interval.
        m_maxRecentResourceRequirements = ResourceAllocationCounts();
        m_lastResourceTrimTimeInSeconds = flushTime;
    }

    setResourceSizes(allocs);

    m_impl->prepareToFlush(flushResources.currentFrameNumber,
                           flushResources.safeFrameNumber);

    mapResourceBuffers(resourceRequirements);

    for (const auto& flush : m_logicalFlushes)
    {
        flush->writeResources();
    }

    assert(m_flushUniformData.elementsWritten() == m_logicalFlushes.size());
    assert(m_imageDrawUniformData.elementsWritten() ==
           totalFrameResourceCounts.imageDrawCount);
    assert(m_pathData.elementsWritten() ==
           totalFrameResourceCounts.pathCount + layoutCounts.pathPaddingCount);
    assert(m_paintData.elementsWritten() ==
           totalFrameResourceCounts.pathCount + layoutCounts.paintPaddingCount);
    assert(m_paintAuxData.elementsWritten() ==
           totalFrameResourceCounts.pathCount +
               layoutCounts.paintAuxPaddingCount);
    assert(m_contourData.elementsWritten() ==
           totalFrameResourceCounts.contourCount +
               layoutCounts.contourPaddingCount);
    assert(m_gradSpanData.elementsWritten() ==
           layoutCounts.gradSpanCount + layoutCounts.gradSpanPaddingCount);
    assert(m_tessSpanData.elementsWritten() <=
           totalFrameResourceCounts.maxTessellatedSegmentCount);
    assert(m_triangleVertexData.elementsWritten() <=
           totalFrameResourceCounts.maxTriangleVertexCount);

    unmapResourceBuffers(resourceRequirements);

    // Issue logical flushes to the backend.
    for (const auto& flush : m_logicalFlushes)
    {
        m_impl->flush(flush->desc());
    }

    m_impl->postFlush(flushResources);

    if (!m_logicalFlushes.empty())
    {
        m_logicalFlushes.resize(1);
        m_logicalFlushes.front()->rewind();
    }

    // Drop all memory that was allocated for this frame using
    // TrivialBlockAllocator.
    m_perFrameAllocator.reset();
    m_numChopsAllocator.reset();
    m_chopVerticesAllocator.reset();
    m_tangentPairsAllocator.reset();
    m_polarSegmentCountsAllocator.reset();
    m_parametricSegmentCountsAllocator.reset();

    m_frameDescriptor = FrameDescriptor();

    RIVE_DEBUG_CODE(m_didBeginFrame = false;)

    // Wait to reset CPU-side containers until after the flush has finished.
    if (needsResourceTrim)
    {
        resetContainers();
    }
}

void RenderContext::LogicalFlush::layoutResources(
    const FlushResources& flushResources,
    size_t logicalFlushIdx,
    ResourceCounters* runningFrameResourceCounts,
    LayoutCounters* runningFrameLayoutCounts)
{
    RIVE_PROF_SCOPE()
    assert(!m_hasDoneLayout);

    const FrameDescriptor& frameDescriptor = m_ctx->frameDescriptor();

    // Reserve a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    ++m_resourceCounts.pathCount;

    // Storage buffer offsets are required to be aligned on multiples of 256.
    m_pathPaddingCount =
        math::padding_to_align_up<gpu::kPathBufferAlignmentInElements>(
            m_resourceCounts.pathCount);
    m_paintPaddingCount =
        math::padding_to_align_up<gpu::kPaintBufferAlignmentInElements>(
            m_resourceCounts.pathCount);
    m_paintAuxPaddingCount =
        math::padding_to_align_up<gpu::kPaintAuxBufferAlignmentInElements>(
            m_resourceCounts.pathCount);
    m_contourPaddingCount =
        math::padding_to_align_up<gpu::kContourBufferAlignmentInElements>(
            m_resourceCounts.contourCount);

    // Metal requires vertex buffers to be 256-byte aligned.
    m_gradSpanPaddingCount =
        math::padding_to_align_up<gpu::kGradSpanBufferAlignmentInElements>(
            m_pendingGradSpanCount);

    size_t totalTessVertexCountWithPadding = 0;
    if ((m_resourceCounts.midpointFanTessVertexCount |
         m_resourceCounts.outerCubicTessVertexCount) != 0)
    {
        // midpointFan tessellation vertices reside at the beginning of the
        // tessellation texture, after 1 patch of padding vertices.
        constexpr uint32_t kPrePadding = gpu::kMidpointFanPatchSegmentSpan;
        m_midpointFanTessVertexIdx = kPrePadding;
        m_midpointFanTessEndLocation =
            m_midpointFanTessVertexIdx +
            math::lossless_numeric_cast<uint32_t>(
                m_resourceCounts.midpointFanTessVertexCount);

        // outerCubic tessellation vertices reside after the midpointFan
        // vertices, aligned on a multiple of the outerCubic patch size.
        uint32_t interiorPadding =
            math::padding_to_align_up<gpu::kOuterCurvePatchSegmentSpan>(
                m_midpointFanTessEndLocation);
        m_outerCubicTessVertexIdx =
            m_midpointFanTessEndLocation + interiorPadding;
        m_outerCubicTessEndLocation =
            m_outerCubicTessVertexIdx +
            math::lossless_numeric_cast<uint32_t>(
                m_resourceCounts.outerCubicTessVertexCount);

        // We need one more padding vertex after all the tessellation vertices.
        constexpr uint32_t kPostPadding = 1;
        totalTessVertexCountWithPadding =
            m_outerCubicTessEndLocation + kPostPadding;

        assert(kPrePadding + interiorPadding + kPostPadding <=
               kMaxTessellationPaddingVertexCount);
        assert(totalTessVertexCountWithPadding <= kMaxTessellationVertexCount);
    }

    uint32_t tessDataHeight = math::lossless_numeric_cast<uint32_t>(
        resource_texture_height<kTessTextureWidth>(
            totalTessVertexCountWithPadding));
    if (m_resourceCounts.maxTessellatedSegmentCount != 0)
    {
        // Conservatively account for line breaks and padding in the
        // tessellation span count. Line breaks potentially introduce a new
        // span. Count the maximum number of line breaks we might encounter,
        // which is at most TWO for every line in the tessellation texture (one
        // for a forward span, and one for its reflection.)
        size_t maxSpanBreakCount = tessDataHeight * 2;
        // The tessellation texture requires 3 separate spans of padding
        // vertices (see above and below).
        constexpr size_t kPaddingSpanCount = 3;
        m_resourceCounts.maxTessellatedSegmentCount +=
            maxSpanBreakCount + kPaddingSpanCount +
            kMaxTessellationAlignmentVertices;
    }

    // Complex gradients begin on the first row immediately after the simple
    // gradients.
    m_gradTextureLayout.complexOffsetY = math::lossless_numeric_cast<uint32_t>(
        resource_texture_height<gpu::kGradTextureWidthInSimpleRamps>(
            m_simpleGradients.size()));

    m_flushDesc.renderTarget = flushResources.renderTarget;
    m_flushDesc.interlockMode = m_ctx->frameInterlockMode();
    m_flushDesc.msaaSampleCount = frameDescriptor.msaaSampleCount;

    // In atomic mode, we may be able to skip the explicit clear of the color
    // buffer and fold it into the atomic "resolve" operation instead.
    bool doClearDuringAtomicResolve = false;

    if (logicalFlushIdx != 0)
    {
        // We always have to preserve the renderTarget between logical flushes.
        m_flushDesc.colorLoadAction = gpu::LoadAction::preserveRenderTarget;
    }
    else if (frameDescriptor.loadAction == gpu::LoadAction::clear)
    {
        // In atomic mode, we can clear during the resolve operation if the
        // clearColor is opaque (because we don't want or have a "source only"
        // blend mode).
        doClearDuringAtomicResolve =
            m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics &&
            colorAlpha(frameDescriptor.clearColor) == 255;
        m_flushDesc.colorLoadAction = doClearDuringAtomicResolve
                                          ? gpu::LoadAction::dontCare
                                          : gpu::LoadAction::clear;
    }
    else
    {
        m_flushDesc.colorLoadAction = frameDescriptor.loadAction;
    }
    m_flushDesc.colorClearValue = frameDescriptor.clearColor;

    if (doClearDuringAtomicResolve)
    {
        // In atomic mode we can accomplish a clear of the color buffer while
        // the shader resolves coverage, instead of actually clearing.
        // writeResources() will configure the fill for pathID=0 to be a solid
        // fill matching the clearColor, so if we just initialize coverage
        // buffer to solid coverage with pathID=0, the resolve step will write
        // out the correct clear color.
        assert(m_flushDesc.interlockMode == gpu::InterlockMode::atomics);
        m_flushDesc.coverageClearValue =
            static_cast<uint32_t>(FIXED_COVERAGE_ONE);
    }
    else if (m_flushDesc.interlockMode == gpu::InterlockMode::atomics)
    {
        // When we don't skip the initial clear in atomic mode, clear the
        // coverage buffer to pathID=0 and a transparent coverage value.
        // pathID=0 meets the requirement that pathID is always monotonically
        // increasing. Transparent coverage makes sure the clearColor doesn't
        // get written out while resolving.
        m_flushDesc.coverageClearValue =
            static_cast<uint32_t>(FIXED_COVERAGE_ZERO);
    }
    else
    {
        // In non-atomic mode, the coverage buffer just needs to be initialized
        // with "pathID=0" to avoid collisions with any pathIDs being rendered.
        m_flushDesc.coverageClearValue = 0;
    }

    if (doClearDuringAtomicResolve ||
        m_flushDesc.colorLoadAction == gpu::LoadAction::clear)
    {
        // If we're clearing then we always update the entire render target.
        m_flushDesc.renderTargetUpdateBounds =
            m_flushDesc.renderTarget->bounds();
    }
    else
    {
        // When we don't clear, we only update the draw bounds.
        m_flushDesc.renderTargetUpdateBounds =
            m_flushDesc.renderTarget->bounds().intersect(m_combinedDrawBounds);
    }
    if (m_flushDesc.renderTargetUpdateBounds.empty())
    {
        // If this is empty it means there are no draws and no clear.
        m_flushDesc.renderTargetUpdateBounds = {0, 0, 0, 0};
    }

    m_flushDesc.atlasContentWidth = m_atlasMaxX;
    m_flushDesc.atlasContentHeight = m_atlasMaxY;

    m_flushDesc.flushUniformDataOffsetInBytes =
        logicalFlushIdx * sizeof(gpu::FlushUniforms);
    m_flushDesc.pathCount =
        math::lossless_numeric_cast<uint32_t>(m_resourceCounts.pathCount);
    m_flushDesc.firstPath = runningFrameResourceCounts->pathCount +
                            runningFrameLayoutCounts->pathPaddingCount;
    m_flushDesc.firstPaint = runningFrameResourceCounts->pathCount +
                             runningFrameLayoutCounts->paintPaddingCount;
    m_flushDesc.firstPaintAux = runningFrameResourceCounts->pathCount +
                                runningFrameLayoutCounts->paintAuxPaddingCount;
    m_flushDesc.contourCount =
        math::lossless_numeric_cast<uint32_t>(m_resourceCounts.contourCount);
    m_flushDesc.firstContour = runningFrameResourceCounts->contourCount +
                               runningFrameLayoutCounts->contourPaddingCount;
    m_flushDesc.gradSpanCount =
        math::lossless_numeric_cast<uint32_t>(m_pendingGradSpanCount);
    m_flushDesc.firstGradSpan = runningFrameLayoutCounts->gradSpanCount +
                                runningFrameLayoutCounts->gradSpanPaddingCount;
    m_flushDesc.gradDataHeight = math::lossless_numeric_cast<uint32_t>(
        m_gradTextureLayout.complexOffsetY + m_complexGradients.size());
    m_flushDesc.tessDataHeight = tessDataHeight;
    m_flushDesc.clockwiseFillOverride = frameDescriptor.clockwiseFillOverride;
    m_flushDesc.wireframe = frameDescriptor.wireframe;
#ifdef WITH_RIVE_TOOLS
    m_flushDesc.synthesizeCompilationFailures =
        frameDescriptor.synthesizeCompilationFailures;
#endif

    m_flushDesc.externalCommandBuffer = flushResources.externalCommandBuffer;

    *runningFrameResourceCounts =
        runningFrameResourceCounts->toVec() + m_resourceCounts.toVec();
    runningFrameLayoutCounts->pathPaddingCount += m_pathPaddingCount;
    runningFrameLayoutCounts->paintPaddingCount += m_paintPaddingCount;
    runningFrameLayoutCounts->paintAuxPaddingCount += m_paintAuxPaddingCount;
    runningFrameLayoutCounts->contourPaddingCount += m_contourPaddingCount;
    runningFrameLayoutCounts->gradSpanCount += m_pendingGradSpanCount;
    runningFrameLayoutCounts->gradSpanPaddingCount += m_gradSpanPaddingCount;
    runningFrameLayoutCounts->maxGradTextureHeight =
        std::max(m_flushDesc.gradDataHeight,
                 runningFrameLayoutCounts->maxGradTextureHeight);
    runningFrameLayoutCounts->maxTessTextureHeight =
        std::max(m_flushDesc.tessDataHeight,
                 runningFrameLayoutCounts->maxTessTextureHeight);
    runningFrameLayoutCounts->maxAtlasWidth =
        std::max(m_atlasMaxX, runningFrameLayoutCounts->maxAtlasWidth);
    runningFrameLayoutCounts->maxAtlasHeight =
        std::max(m_atlasMaxY, runningFrameLayoutCounts->maxAtlasHeight);
    runningFrameLayoutCounts->maxCoverageBufferLength =
        std::max<size_t>(m_coverageBufferLength,
                         runningFrameLayoutCounts->maxCoverageBufferLength);

    assert(m_flushDesc.firstPath % gpu::kPathBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstPaint % gpu::kPaintBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstPaintAux %
               gpu::kPaintAuxBufferAlignmentInElements ==
           0);
    assert(m_flushDesc.firstContour % gpu::kContourBufferAlignmentInElements ==
           0);
    assert(m_flushDesc.firstGradSpan %
               gpu::kGradSpanBufferAlignmentInElements ==
           0);
    RIVE_DEBUG_CODE(m_hasDoneLayout = true;)
}

void RenderContext::LogicalFlush::writeResources()
{
    RIVE_PROF_SCOPE()
    const gpu::PlatformFeatures& platformFeatures = m_ctx->platformFeatures();
    assert(m_hasDoneLayout);
    assert(m_flushDesc.firstPath == m_ctx->m_pathData.elementsWritten());
    assert(m_flushDesc.firstPaint == m_ctx->m_paintData.elementsWritten());
    assert(m_flushDesc.firstPaintAux ==
           m_ctx->m_paintAuxData.elementsWritten());

    // Wait until here before we record these texture sizes; they aren't decided
    // until after all LogicalFlushes have run layoutResources().
    m_flushDesc.atlasTextureWidth = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_currentResourceAllocations.atlasTextureWidth);
    m_flushDesc.atlasTextureHeight = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_currentResourceAllocations.atlasTextureHeight);
    m_gradTextureLayout.inverseHeight =
        1.f / m_ctx->m_currentResourceAllocations.gradTextureHeight;

    // Exact tessSpan/triangleVertex counts aren't known until after their data
    // is written out.
    size_t firstTessVertexSpan = m_ctx->m_tessSpanData.elementsWritten();
    size_t initialTriangleVertexDataSize =
        m_ctx->m_triangleVertexData.bytesWritten();

    // Metal requires vertex buffers to be 256-byte aligned.
    size_t tessAlignmentPadding =
        math::padding_to_align_up<gpu::kTessVertexBufferAlignmentInElements>(
            firstTessVertexSpan);
    assert(tessAlignmentPadding <= kMaxTessellationAlignmentVertices);
    m_ctx->m_tessSpanData.push_back_n(nullptr, tessAlignmentPadding);
    m_flushDesc.firstTessVertexSpan =
        firstTessVertexSpan + tessAlignmentPadding;
    assert(m_flushDesc.firstTessVertexSpan ==
           m_ctx->m_tessSpanData.elementsWritten());

    // Write out the simple gradient data.
    constexpr static uint32_t ONE_TEXEL_FIXED = 65536 / gpu::kGradTextureWidth;
    assert(m_simpleGradients.size() == m_pendingSimpleGradDraws.size());
    if (!m_pendingSimpleGradDraws.empty())
    {
        for (size_t i = 0; i < m_pendingSimpleGradDraws.size(); ++i)
        {
            // Render each simple gradient as a single, empty GradientSpan with
            // 1px borders to the left and right.
            auto [color0, color1] = m_pendingSimpleGradDraws[i];
            uint32_t y = math::lossless_numeric_cast<uint32_t>(
                i / gpu::kGradTextureWidthInSimpleRamps);
            size_t centerX = (i % gpu::kGradTextureWidthInSimpleRamps) * 2 + 1;
            uint32_t centerXFixed = math::lossless_numeric_cast<uint32_t>(
                centerX * ONE_TEXEL_FIXED);
            m_ctx->m_gradSpanData.set_back(centerXFixed,
                                           centerXFixed,
                                           y,
                                           GRAD_SPAN_FLAG_LEFT_BORDER |
                                               GRAD_SPAN_FLAG_RIGHT_BORDER,
                                           color0,
                                           color1);
        }
    }

    // Write out the vertex data for rendering complex gradients.
    assert(m_complexGradients.size() == m_pendingComplexGradDraws.size());
    if (!m_pendingComplexGradDraws.empty())
    {
        // The viewport will start at simpleGradDataHeight when rendering color
        // ramps.
        for (uint32_t i = 0; i < m_pendingComplexGradDraws.size(); ++i)
        {
            // Push "GradientSpan" instances that will render each section of
            // this color ramp's gradient.
            const Gradient* gradient = m_pendingComplexGradDraws[i];
            const float* stops = gradient->stops();
            const ColorInt* colors = gradient->colors();
            size_t stopCount = gradient->count();
            uint32_t y = i + m_gradTextureLayout.complexOffsetY;

            // "stop * m + a" converts a stop position to a fixed-point x
            // coordinate in the gradient texture. (In an ideal world, stops
            // would all be aligned on pixel centers for the texture sampling to
            // be identical to the gradient, but here we just stretch it across
            // kGradTextureWidth pixels and hope everything looks ok.)
            float m = (kGradTextureWidth - 1.f) * ONE_TEXEL_FIXED;
            float a = .5f * ONE_TEXEL_FIXED;
            uint32_t lastXFixed = static_cast<uint32_t>(stops[0] * m + a);
            ColorInt lastColor = colors[0];
            assert(stopCount >= 2);
            for (size_t i = 1; i < stopCount; ++i)
            {
                uint32_t xFixed = static_cast<uint32_t>(stops[i] * m + a);
                // stops[] must be ordered.
                assert(lastXFixed <= xFixed && xFixed < 65536);
                uint32_t flags = GRAD_SPAN_FLAG_COMPLEX_BORDER;
                if (i == 1)
                    flags |= GRAD_SPAN_FLAG_LEFT_BORDER;
                if (i == stopCount - 1)
                    flags |= GRAD_SPAN_FLAG_RIGHT_BORDER;
                m_ctx->m_gradSpanData.set_back(lastXFixed,
                                               xFixed,
                                               y,
                                               flags,
                                               lastColor,
                                               colors[i]);
                lastColor = colors[i];
                lastXFixed = xFixed;
            }
        }
    }

    // Write a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    gpu::SimplePaintValue clearColorValue;
    clearColorValue.color = m_ctx->frameDescriptor().clearColor;
    m_ctx->m_pathData.skip_back();
    m_ctx->m_paintData.set_back(gpu::DrawContents::none,
                                PaintType::solidColor,
                                clearColorValue,
                                GradTextureLayout(),
                                /*clipID =*/0,
                                /*hasClipRect =*/false,
                                BlendMode::srcOver);
    m_ctx->m_paintAuxData.skip_back();

    // Render padding vertices in the tessellation texture.
    if (m_flushDesc.tessDataHeight > 0)
    {
        // Padding at the beginning of the tessellation texture.
        pushPaddingVertices(gpu::kMidpointFanPatchSegmentSpan, 0);
        // Padding between patch types in the tessellation texture.
        if (m_outerCubicTessVertexIdx > m_midpointFanTessEndLocation)
        {
            pushPaddingVertices(m_outerCubicTessVertexIdx -
                                    m_midpointFanTessEndLocation,
                                m_midpointFanTessEndLocation);
        }
        // The final vertex of the final patch of each contour crosses over into
        // the next contour. (This is how we wrap around back to the beginning.)
        // Therefore, the final contour of the flush needs an out-of-contour
        // vertex to cross into as well, so we emit a padding vertex here at the
        // end.
        pushPaddingVertices(1, m_outerCubicTessEndLocation);
    }

    // Write out all the data for our high level draws, and build up a low-level
    // draw list.
    if (m_ctx->frameInterlockMode() == gpu::InterlockMode::rasterOrdering)
    {
        for (const DrawUniquePtr& draw : m_draws)
        {
            // TODO: We don't currently support a front-to-back prepass in
            // rasterOrdering mode. If we decide to support this, we will either
            // need to walk the draws backwards here, or, more likely, start
            // sorting and re-ordering in rasterOrdering mode as well.
            assert(draw->prepassCount() == 0);
            assert(draw->subpassCount() > 0);
            for (int i = 0; i < draw->subpassCount(); ++i)
            {
                draw->pushToRenderContext(this, i);
            }
        }
    }
    else
    {
        assert(m_drawPassCount <= kMaxReorderedDrawPassCount);

        // Sort the draw list to optimize batching, since we can only batch
        // non-overlapping draws.
        std::vector<int64_t>& indirectDrawList = m_ctx->m_indirectDrawList;
        indirectDrawList.clear();
        indirectDrawList.reserve(m_drawPassCount);

        if (m_ctx->m_intersectionBoard == nullptr)
        {
            m_ctx->m_intersectionBoard = std::make_unique<IntersectionBoard>();
        }
        IntersectionBoard* intersectionBoard = m_ctx->m_intersectionBoard.get();
        intersectionBoard->resizeAndReset(m_flushDesc.renderTarget->width(),
                                          m_flushDesc.renderTarget->height());

        // Build a list of sort keys that determine the final draw order.
        constexpr static int kDrawGroupShift =
            48; // Where in the key does the draw group begin?
        constexpr static int64_t kDrawGroupMask = 0x7fffllu << kDrawGroupShift;
        constexpr static int kDrawTypeShift = 45;
        constexpr static int64_t kDrawTypeMask RIVE_MAYBE_UNUSED =
            7llu << kDrawTypeShift;
        constexpr static int kTextureHashShift = 31;
        constexpr static int64_t kTextureHashMask = 0x3fffllu
                                                    << kTextureHashShift;
        constexpr static int kBlendModeShift = 27;
        constexpr static int kBlendModeMask = 0xf << kBlendModeShift;
        constexpr static int kDrawContentsShift = 18;
        constexpr static int64_t kDrawContentsMask = 0x1ffllu
                                                     << kDrawContentsShift;
        constexpr static int kDrawIndexShift = 2;
        constexpr static int64_t kDrawIndexMask = 0x7fff << kDrawIndexShift;
        constexpr static int64_t kSubpassIndexMask = 0x3;

        for (size_t i = 0; i < m_draws.size(); ++i)
        {
            Draw* draw = m_draws[i].get();
            int4 drawBounds = simd::load4i(&m_draws[i]->pixelBounds());

            // Add one extra pixel of padding to the draw bounds to make
            // absolutely certain we get no overlapping pixels, which destroy
            // the atomic shader.
            constexpr int32_t kMax32i = std::numeric_limits<int32_t>::max();
            constexpr int32_t kMin32i = std::numeric_limits<int32_t>::min();
            drawBounds = simd::if_then_else(
                drawBounds != int4{kMin32i, kMin32i, kMax32i, kMax32i},
                drawBounds + int4{-1, -1, 1, 1},
                drawBounds);

            // Our top priority in re-ordering is to group non-overlapping draws
            // together, in order to maximize batching while preserving
            // correctness.
            int maxPasses =
                std::max(draw->prepassCount(), draw->subpassCount());
            int16_t drawGroupIdx =
                intersectionBoard->addRectangle(drawBounds, maxPasses);
            assert(drawGroupIdx > 0);
            int64_t key = static_cast<int64_t>(drawGroupIdx) << kDrawGroupShift;

            // Within sub-groups of non-overlapping draws, sort similar draw
            // types together.
            int64_t drawType = static_cast<int64_t>(draw->type());
            assert(drawType <= kDrawTypeMask >> kDrawTypeShift);
            key |= drawType << kDrawTypeShift;

            // Within sub-groups of matching draw type, sort by texture binding.
            int64_t textureHash =
                draw->imageTexture() != nullptr
                    ? draw->imageTexture()->textureResourceHash() &
                          (kTextureHashMask >> kTextureHashShift)
                    : 0;
            key |= textureHash << kTextureHashShift;

            // If using KHR_blend_equation_advanced, we need a batching barrier
            // between draws with different blend modes. If not using
            // KHR_blend_equation_advanced, sorting by blend mode may still give
            // us better branching on the GPU.
            int64_t blendMode =
                gpu::ConvertBlendModeToPLSBlendMode(draw->blendMode());
            assert(blendMode <= kBlendModeMask >> kBlendModeShift);
            key |= blendMode << kBlendModeShift;

            // msaa mode draws strokes, fills, and even/odd with different
            // stencil settings.
            int64_t drawContents = static_cast<int64_t>(draw->drawContents());
            assert(drawContents <= kDrawContentsMask >> kDrawContentsShift);
            key |= drawContents << kDrawContentsShift;

            // Draw and subpass indices go at the bottom of the key so we can
            // reference them again after sorting without affecting the order.
            assert(i <= kDrawIndexMask >> kDrawIndexShift);
            key |= i << kDrawIndexShift;

            assert((key & kDrawGroupMask) >> kDrawGroupShift == drawGroupIdx);
            assert((key & kDrawTypeMask) >> kDrawTypeShift == drawType);
            assert((key & kTextureHashMask) >> kTextureHashShift ==
                   textureHash);
            assert((key & kBlendModeMask) >> kBlendModeShift == blendMode);
            assert((key & kDrawContentsMask) >> kDrawContentsShift ==
                   drawContents);
            assert((key & kDrawIndexMask) >> kDrawIndexShift == i);

            // Add the first prepass and subpass, if any.
            if (draw->prepassCount() > 0)
            {
                // Negating the key is an easy way to sort the prepasses
                // front-to-back, and before the subpasses.
                indirectDrawList.push_back(-key);
            }
            if (draw->subpassCount() > 0)
            {
                indirectDrawList.push_back(key);
            }

            // Add any additional passes.
            for (int i = 1; i < maxPasses; ++i)
            {
                // Increment the drawGroupIdx and i both at once. (The
                // intersectionBoard already reserved "maxPasses" layers of
                // drawGroupIndices for us.)
                key += (1ll << kDrawGroupShift) + 1;
                assert((key & kDrawGroupMask) >> kDrawGroupShift ==
                       drawGroupIdx + i);
                assert((key & kSubpassIndexMask) == i);

                if (i < draw->prepassCount())
                {
                    // Negating the key is an easy way to sort the prepasses
                    // front-to-back, and before the subpasses.
                    indirectDrawList.push_back(-key);
                }
                if (i < draw->subpassCount())
                {
                    indirectDrawList.push_back(key);
                }
            }
        }
        assert(indirectDrawList.size() == m_drawPassCount);

        // Re-order the draws!!
        std::sort(indirectDrawList.begin(), indirectDrawList.end());

        // Atomic mode sometimes needs to initialize PLS with a draw when the
        // backend can't do it with typical clear/load APIs.
        if (m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics)
        {
            assert(m_pendingBarriers == BarrierFlags::none);
            if (platformFeatures.atomicPLSMustBeInitializedAsDraw)
            {
                m_drawList.emplace_back(m_ctx->perFrameAllocator(),
                                        DrawType::atomicInitialize,
                                        gpu::ShaderMiscFlags::none,
                                        1,
                                        0,
                                        BlendMode::srcOver,
                                        ImageSampler::LinearClamp(),
                                        BarrierFlags::none);
            }
            m_pendingBarriers |= BarrierFlags::plsAtomicPostInit;
        }

        // Find a mask that tells us when to insert barriers, and which barriers
        // are needed. When the keys of two adjacent draws differ within this
        // bitmask, we insert a barrier between them.
        int64_t needsBarrierMask = 0;
        BarrierFlags neededBarriers = BarrierFlags::none;
        switch (m_flushDesc.interlockMode)
        {
            case gpu::InterlockMode::rasterOrdering:
                // rasterOrdering mode doesn't reorder draws.
                RIVE_UNREACHABLE();

            case gpu::InterlockMode::atomics:
                // In atomic mode, we need barriers any time draws overlap.
                // Insert a barrier every time the drawGroupIdx changes.
                needsBarrierMask = kDrawGroupMask;
                neededBarriers = BarrierFlags::plsAtomic;
                break;

            case gpu::InterlockMode::clockwiseAtomic:
                // In clockwiseAtomic mode, we only need a barrier between the
                // borrowedCoverage prepasses and the main rendering. Prepasses
                // have a negative key, so just insert a barrier when the sign
                // changes.
                needsBarrierMask = 1ll << 63;
                neededBarriers = BarrierFlags::clockwiseBorrowedCoverage;
                break;

            case gpu::InterlockMode::msaa:
                // MSAA mode can't batch draws that overlap because they both
                // rely on the stencil buffer across subpasses. Stop batching
                // every time the drawGroupIdx changes.
                needsBarrierMask = kDrawGroupMask;
                // MSAA mode draws clips, strokes, fills, and even/odd with
                // different stencil settings, so these can't be batched.
                needsBarrierMask |= kDrawContentsMask;
                if (platformFeatures.supportsBlendAdvancedKHR)
                {
                    // If using KHR_blend_equation_advanced, we also need to
                    // stop batching between blend modes in order to change the
                    // blend equation.
                    needsBarrierMask |= kBlendModeMask;
                }
                // MSAA barriers only need to prevent batching of draws for now.
                // If we also need a dstBlend barrier, that will be decided
                // later.
                neededBarriers = BarrierFlags::drawBatchBreak;
                break;
        }

        // Write out the draw data from the sorted draw list, and build up a
        // condensed/batched list of low-level draws.
        int64_t priorSignedKey =
            !indirectDrawList.empty() ? indirectDrawList[0] : 0;
        for (const int64_t signedKey : indirectDrawList)
        {
            assert(signedKey >= priorSignedKey);
            if ((priorSignedKey & needsBarrierMask) !=
                (signedKey & needsBarrierMask))
            {
                m_pendingBarriers |= neededBarriers;
            }
            int64_t key = abs(signedKey);
            uint32_t drawIndex = (key & kDrawIndexMask) >> kDrawIndexShift;
            int subpassIndex = key & kSubpassIndexMask;
            if (signedKey < 0)
            {
                // Negative keys are a prepass. Update the subpassIndex to be
                // negative.
                subpassIndex = -1 - subpassIndex;
            }
            // FIXME: m_currentZIndex shouldn't be a stateful variable; it
            // should be passed to pushToRenderContext() instead.
            m_currentZIndex = math::lossless_numeric_cast<uint32_t>(
                abs(key >> static_cast<int64_t>(kDrawGroupShift)));
            m_draws[drawIndex]->pushToRenderContext(this, subpassIndex);
            priorSignedKey = signedKey;
        }

        // Atomic mode needs one more draw to resolve all the pixels.
        if (m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics)
        {
            // We can ignore any pending "plsAtomic" barriers; the
            // plsAtomicPreResolve can replace them.
            assert((m_pendingBarriers & ~(BarrierFlags::plsAtomic |
                                          BarrierFlags::plsAtomicPostInit)) ==
                   BarrierFlags::none);
            m_drawList
                .emplace_back(m_ctx->perFrameAllocator(),
                              DrawType::atomicResolve,
                              gpu::ShaderMiscFlags::none,
                              1,
                              0,
                              BlendMode::srcOver,
                              ImageSampler::LinearClamp(),
                              BarrierFlags::plsAtomicPreResolve)
                .shaderFeatures = m_combinedShaderFeatures;
        }
    }

    // Write out the draws to the feather atlas. Do this after the main draws
    // (even though the atlas ones execute first) so that our path info and Z
    // index are decided and available to pushAtlasTessellation().
    if (!m_pendingAtlasDraws.empty())
    {
        TAABB<uint16_t> fullAtlasViewport = {0,
                                             0,
                                             m_flushDesc.atlasContentWidth,
                                             m_flushDesc.atlasContentHeight};
        gpu::AtlasDrawBatch* currentBatch =
            m_ctx->m_perFrameAllocator.makePODArray<gpu::AtlasDrawBatch>(
                m_pendingAtlasDraws.size());
        // Iterate the atlas draws 4 times so we can sort by fill / stroke /
        // scissored / not, and batch together the draws that don't have
        // scissor.
        for (bool stroked : {false, true})
        {
            if (stroked)
            {
                m_flushDesc.atlasStrokeBatches = currentBatch;
            }
            else
            {
                m_flushDesc.atlasFillBatches = currentBatch;
            }
            for (bool scissored : {false, true})
            {
                gpu::AtlasDrawBatch* lastBatch = nullptr;
                for (PathDraw* draw : m_pendingAtlasDraws)
                {
                    if (draw->isStroke() != stroked ||
                        draw->atlasScissorEnabled() != scissored)
                    {
                        continue;
                    }
                    uint32_t tessVertexCount, tessBaseVertex;
                    draw->pushAtlasTessellation(this,
                                                &tessVertexCount,
                                                &tessBaseVertex);
                    if (tessVertexCount == 0)
                    {
                        continue;
                    }
                    uint32_t patchCount =
                        tessVertexCount / gpu::kMidpointFanPatchSegmentSpan;
                    uint32_t basePatch =
                        tessBaseVertex / gpu::kMidpointFanPatchSegmentSpan;
                    assert(patchCount * gpu::kMidpointFanPatchSegmentSpan ==
                           tessVertexCount);
                    assert(basePatch * gpu::kMidpointFanPatchSegmentSpan ==
                           tessBaseVertex);
                    if (lastBatch == nullptr || scissored)
                    {
                        lastBatch = currentBatch++;
                        *lastBatch = {
                            lastBatch->scissor = scissored
                                                     ? draw->atlasScissor()
                                                     : fullAtlasViewport,
                            lastBatch->patchCount = patchCount,
                            lastBatch->basePatch = basePatch,
                        };
                    }
                    else
                    {
                        assert(lastBatch->basePatch + lastBatch->patchCount ==
                               basePatch);
                        lastBatch->patchCount += patchCount;
                    }
                }
            }
            if (stroked)
            {
                m_flushDesc.atlasStrokeBatchCount =
                    currentBatch - m_flushDesc.atlasStrokeBatches;
            }
            else
            {
                m_flushDesc.atlasFillBatchCount =
                    currentBatch - m_flushDesc.atlasFillBatches;
            }
        }
        assert(m_flushDesc.atlasFillBatchCount +
                   m_flushDesc.atlasStrokeBatchCount ==
               currentBatch - m_flushDesc.atlasFillBatches);
        assert(m_flushDesc.atlasFillBatchCount +
                   m_flushDesc.atlasStrokeBatchCount <=
               m_pendingAtlasDraws.size());
    }

    // Pad our buffers to 256-byte alignment.
    m_ctx->m_pathData.push_back_n(nullptr, m_pathPaddingCount);
    m_ctx->m_paintData.push_back_n(nullptr, m_paintPaddingCount);
    m_ctx->m_paintAuxData.push_back_n(nullptr, m_paintAuxPaddingCount);
    m_ctx->m_contourData.push_back_n(nullptr, m_contourPaddingCount);
    m_ctx->m_gradSpanData.push_back_n(nullptr, m_gradSpanPaddingCount);

    assert(m_ctx->m_pathData.elementsWritten() ==
           m_flushDesc.firstPath + m_resourceCounts.pathCount +
               m_pathPaddingCount);
    assert(m_ctx->m_paintData.elementsWritten() ==
           m_flushDesc.firstPaint + m_resourceCounts.pathCount +
               m_paintPaddingCount);
    assert(m_ctx->m_paintAuxData.elementsWritten() ==
           m_flushDesc.firstPaintAux + m_resourceCounts.pathCount +
               m_paintAuxPaddingCount);
    assert(m_ctx->m_contourData.elementsWritten() ==
           m_flushDesc.firstContour + m_resourceCounts.contourCount +
               m_contourPaddingCount);
    assert(m_ctx->m_gradSpanData.elementsWritten() ==
           m_flushDesc.firstGradSpan + m_pendingGradSpanCount +
               m_gradSpanPaddingCount);
    assert(m_midpointFanTessVertexIdx == m_midpointFanTessEndLocation);
    assert(m_outerCubicTessVertexIdx == m_outerCubicTessEndLocation);

    // Some of the flushDescriptor's data isn't known until after
    // writeResources(). Update it now that it's known.
    m_flushDesc.combinedShaderFeatures = m_combinedShaderFeatures;
    m_flushDesc.atomicFixedFunctionColorOutput =
        m_ctx->frameInterlockMode() == InterlockMode::atomics &&
        !(m_combinedShaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND);

    if (m_coverageBufferLength > 0)
    {
        assert(m_flushDesc.interlockMode ==
               gpu::InterlockMode::clockwiseAtomic);
        // The coverage buffer prefix gets reset to zero when the buffer is
        // reallocated, so wait until here to get the prefix.
        m_flushDesc.coverageBufferPrefix = m_ctx->incrementCoverageBufferPrefix(
            &m_flushDesc.needsCoverageBufferClear);
    }

    m_flushDesc.tessVertexSpanCount = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_tessSpanData.elementsWritten() -
        m_flushDesc.firstTessVertexSpan);

    m_flushDesc.hasTriangleVertices =
        m_ctx->m_triangleVertexData.bytesWritten() !=
        initialTriangleVertexDataSize;

    m_flushDesc.drawList = &m_drawList;

    // Write out the uniforms for this flush now that the flushDescriptor is
    // complete.
    m_ctx->m_flushUniformData.emplace_back(m_flushDesc, platformFeatures);
}

void RenderContext::setResourceSizes(ResourceAllocationCounts allocs,
                                     bool forceRealloc)
{
    RIVE_PROF_SCOPE()
#if 0
    class Logger
    {
    public:
        void logSize(const char* name,
                     size_t oldSize,
                     size_t newSize,
                     size_t newSizeInBytes)
        {
            m_totalSizeInBytes += newSizeInBytes;
            if (oldSize == newSize)
            {
                return;
            }
            if (!m_hasChanged)
            {
                printf("RenderContext::setResourceSizes():\n");
                m_hasChanged = true;
            }
            printf("  resize %s: %zu -> %zu (%zu KiB)\n",
                   name,
                   oldSize,
                   newSize,
                   newSizeInBytes >> 10);
        }

        void logTextureSize(const char* widthName,
                            const char* heightName,
                            size_t oldWidth,
                            size_t oldHeight,
                            size_t newWidth,
                            size_t newHeight,
                            size_t bytesPerPixel)
        {
            m_totalSizeInBytes += newHeight * newWidth * bytesPerPixel;
            if (oldWidth == newWidth && oldHeight == newHeight)
            {
                return;
            }
            if (!m_hasChanged)
            {
                printf("RenderContext::setResourceSizes():\n");
                m_hasChanged = true;
            }
            printf("  resize %s x %s: %zu x %zu -> %zu x %zu (%zu KiB)\n",
                   widthName,
                   heightName,
                   oldWidth,
                   oldHeight,
                   newWidth,
                   newHeight,
                   (newHeight * newWidth * bytesPerPixel) >> 10);
        }

        ~Logger()
        {
            if (!m_hasChanged)
            {
                return;
            }
            printf("  TOTAL GPU resource usage: %zu KiB\n",
                   m_totalSizeInBytes >> 10);
        }

    private:
        size_t m_totalSizeInBytes = 0;
        bool m_hasChanged = false;
    } logger;
#define LOG_BUFFER_RING_SIZE(NAME, ITEM_SIZE_IN_BYTES)                         \
    logger.logSize(#NAME,                                                      \
                   m_currentResourceAllocations.NAME,                          \
                   allocs.NAME,                                                \
                   allocs.NAME* ITEM_SIZE_IN_BYTES* gpu::kBufferRingSize)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)                                \
    logger.logSize(#NAME,                                                      \
                   m_currentResourceAllocations.NAME,                          \
                   allocs.NAME,                                                \
                   allocs.NAME* BYTES_PER_ROW)
#define LOG_TEXTURE_SIZE(WIDTH_NAME, HEIGHT_NAME, BYTES_PER_PIXEL)             \
    logger.logTextureSize(#WIDTH_NAME,                                         \
                          #HEIGHT_NAME,                                        \
                          m_currentResourceAllocations.WIDTH_NAME,             \
                          m_currentResourceAllocations.HEIGHT_NAME,            \
                          allocs.WIDTH_NAME,                                   \
                          allocs.HEIGHT_NAME,                                  \
                          BYTES_PER_PIXEL)
#define LOG_BUFFER_SIZE(NAME, BYTES_PER_ELEMENT)                               \
    logger.logSize(#NAME,                                                      \
                   m_currentResourceAllocations.NAME,                          \
                   allocs.NAME,                                                \
                   allocs.NAME* BYTES_PER_ELEMENT)
#else
#define LOG_BUFFER_RING_SIZE(NAME, ITEM_SIZE_IN_BYTES)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)
#define LOG_TEXTURE_SIZE(WIDTH_NAME, HEIGHT_NAME, BYTES_PER_PIXEL)
#define LOG_BUFFER_SIZE(NAME, BYTES_PER_ELEMENT)
#endif

    LOG_BUFFER_RING_SIZE(flushUniformBufferCount, sizeof(gpu::FlushUniforms));
    if (allocs.flushUniformBufferCount !=
            m_currentResourceAllocations.flushUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeFlushUniformBuffer(allocs.flushUniformBufferCount *
                                         sizeof(gpu::FlushUniforms));
    }

    LOG_BUFFER_RING_SIZE(imageDrawUniformBufferCount,
                         sizeof(gpu::ImageDrawUniforms));
    if (allocs.imageDrawUniformBufferCount !=
            m_currentResourceAllocations.imageDrawUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeImageDrawUniformBuffer(
            allocs.imageDrawUniformBufferCount *
            sizeof(gpu::ImageDrawUniforms));
    }

    LOG_BUFFER_RING_SIZE(pathBufferCount, sizeof(gpu::PathData));
    if (allocs.pathBufferCount !=
            m_currentResourceAllocations.pathBufferCount ||
        forceRealloc)
    {
        m_impl->resizePathBuffer(allocs.pathBufferCount * sizeof(gpu::PathData),
                                 gpu::PathData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintBufferCount, sizeof(gpu::PaintData));
    if (allocs.paintBufferCount !=
            m_currentResourceAllocations.paintBufferCount ||
        forceRealloc)
    {
        m_impl->resizePaintBuffer(allocs.paintBufferCount *
                                      sizeof(gpu::PaintData),
                                  gpu::PaintData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintAuxBufferCount, sizeof(gpu::PaintAuxData));
    if (allocs.paintAuxBufferCount !=
            m_currentResourceAllocations.paintAuxBufferCount ||
        forceRealloc)
    {
        m_impl->resizePaintAuxBuffer(allocs.paintAuxBufferCount *
                                         sizeof(gpu::PaintAuxData),
                                     gpu::PaintAuxData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(contourBufferCount, sizeof(gpu::ContourData));
    if (allocs.contourBufferCount !=
            m_currentResourceAllocations.contourBufferCount ||
        forceRealloc)
    {
        m_impl->resizeContourBuffer(allocs.contourBufferCount *
                                        sizeof(gpu::ContourData),
                                    gpu::ContourData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(gradSpanBufferCount, sizeof(gpu::GradientSpan));
    if (allocs.gradSpanBufferCount !=
            m_currentResourceAllocations.gradSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeGradSpanBuffer(allocs.gradSpanBufferCount *
                                     sizeof(gpu::GradientSpan));
    }

    LOG_BUFFER_RING_SIZE(tessSpanBufferCount, sizeof(gpu::TessVertexSpan));
    if (allocs.tessSpanBufferCount !=
            m_currentResourceAllocations.tessSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTessVertexSpanBuffer(allocs.tessSpanBufferCount *
                                           sizeof(gpu::TessVertexSpan));
    }

    LOG_BUFFER_RING_SIZE(triangleVertexBufferCount,
                         sizeof(gpu::TriangleVertex));
    if (allocs.triangleVertexBufferCount !=
            m_currentResourceAllocations.triangleVertexBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTriangleVertexBuffer(allocs.triangleVertexBufferCount *
                                           sizeof(gpu::TriangleVertex));
    }

    assert(allocs.gradTextureHeight <= kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(gradTextureHeight, gpu::kGradTextureWidth * 4);
    if (allocs.gradTextureHeight !=
            m_currentResourceAllocations.gradTextureHeight ||
        forceRealloc)
    {
        m_impl->resizeGradientTexture(
            gpu::kGradTextureWidth,
            math::lossless_numeric_cast<uint32_t>(allocs.gradTextureHeight));
    }

    assert(allocs.tessTextureHeight <= kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(tessTextureHeight, gpu::kTessTextureWidth * 4 * 4);
    if (allocs.tessTextureHeight !=
            m_currentResourceAllocations.tessTextureHeight ||
        forceRealloc)
    {
        m_impl->resizeTessellationTexture(
            gpu::kTessTextureWidth,
            math::lossless_numeric_cast<uint32_t>(allocs.tessTextureHeight));
    }

    assert(allocs.atlasTextureWidth <= atlasMaxSize() ||
           allocs.atlasTextureWidth <= frameDescriptor().renderTargetWidth);
    assert(allocs.atlasTextureHeight <= atlasMaxSize() ||
           allocs.atlasTextureHeight <= frameDescriptor().renderTargetHeight);
    LOG_TEXTURE_SIZE(atlasTextureWidth, atlasTextureHeight, sizeof(uint16_t));
    if (allocs.atlasTextureWidth !=
            m_currentResourceAllocations.atlasTextureWidth ||
        allocs.atlasTextureHeight !=
            m_currentResourceAllocations.atlasTextureHeight ||
        forceRealloc)
    {
        m_impl->resizeAtlasTexture(
            math::lossless_numeric_cast<uint32_t>(allocs.atlasTextureWidth),
            math::lossless_numeric_cast<uint32_t>(allocs.atlasTextureHeight));
    }

    assert(allocs.coverageBufferLength <=
           platformFeatures().maxCoverageBufferLength);
    LOG_BUFFER_SIZE(coverageBufferLength, sizeof(uint32_t));
    if (allocs.coverageBufferLength !=
            m_currentResourceAllocations.coverageBufferLength ||
        forceRealloc)
    {
        m_impl->resizeCoverageBuffer(allocs.coverageBufferLength *
                                     sizeof(uint32_t));
        // Start the coverageBufferPrefix over at zero. This ensure the new
        // buffer gets cleared because the only criteria for clearing it is when
        // the prefix wraps around to 0.
        m_coverageBufferPrefix = 0;
    }

    m_currentResourceAllocations = allocs;
}

void RenderContext::mapResourceBuffers(
    const ResourceAllocationCounts& mapCounts)
{
    RIVE_PROF_SCOPE()
    if (mapCounts.flushUniformBufferCount > 0)
    {
        m_flushUniformData.mapElements(
            m_impl.get(),
            &RenderContextImpl::mapFlushUniformBuffer,
            mapCounts.flushUniformBufferCount);
    }
    assert(m_flushUniformData.hasRoomFor(mapCounts.flushUniformBufferCount));

    if (mapCounts.imageDrawUniformBufferCount > 0)
    {
        m_imageDrawUniformData.mapElements(
            m_impl.get(),
            &RenderContextImpl::mapImageDrawUniformBuffer,
            mapCounts.imageDrawUniformBufferCount);
    }
    assert(m_imageDrawUniformData.hasRoomFor(
        mapCounts.imageDrawUniformBufferCount > 0));

    if (mapCounts.pathBufferCount > 0)
    {
        m_pathData.mapElements(m_impl.get(),
                               &RenderContextImpl::mapPathBuffer,
                               mapCounts.pathBufferCount);
    }
    assert(m_pathData.hasRoomFor(mapCounts.pathBufferCount));

    if (mapCounts.paintBufferCount > 0)
    {
        m_paintData.mapElements(m_impl.get(),
                                &RenderContextImpl::mapPaintBuffer,
                                mapCounts.paintBufferCount);
    }
    assert(m_paintData.hasRoomFor(mapCounts.paintBufferCount));

    if (mapCounts.paintAuxBufferCount > 0)
    {
        m_paintAuxData.mapElements(m_impl.get(),
                                   &RenderContextImpl::mapPaintAuxBuffer,
                                   mapCounts.paintAuxBufferCount);
    }
    assert(m_paintAuxData.hasRoomFor(mapCounts.paintAuxBufferCount));

    if (mapCounts.contourBufferCount > 0)
    {
        m_contourData.mapElements(m_impl.get(),
                                  &RenderContextImpl::mapContourBuffer,
                                  mapCounts.contourBufferCount);
    }
    assert(m_contourData.hasRoomFor(mapCounts.contourBufferCount));

    if (mapCounts.gradSpanBufferCount > 0)
    {
        m_gradSpanData.mapElements(m_impl.get(),
                                   &RenderContextImpl::mapGradSpanBuffer,
                                   mapCounts.gradSpanBufferCount);
    }
    assert(m_gradSpanData.hasRoomFor(mapCounts.gradSpanBufferCount));

    if (mapCounts.tessSpanBufferCount > 0)
    {
        m_tessSpanData.mapElements(m_impl.get(),
                                   &RenderContextImpl::mapTessVertexSpanBuffer,
                                   mapCounts.tessSpanBufferCount);
    }
    assert(m_tessSpanData.hasRoomFor(mapCounts.tessSpanBufferCount));

    if (mapCounts.triangleVertexBufferCount > 0)
    {
        m_triangleVertexData.mapElements(
            m_impl.get(),
            &RenderContextImpl::mapTriangleVertexBuffer,
            mapCounts.triangleVertexBufferCount);
    }
    assert(
        m_triangleVertexData.hasRoomFor(mapCounts.triangleVertexBufferCount));
}

void RenderContext::unmapResourceBuffers(
    const ResourceAllocationCounts& mapCounts)
{
    RIVE_PROF_SCOPE()
    if (m_flushUniformData)
    {
        m_flushUniformData.unmapElements(
            m_impl.get(),
            &RenderContextImpl::unmapFlushUniformBuffer,
            mapCounts.flushUniformBufferCount);
    }
    if (m_imageDrawUniformData)
    {
        m_imageDrawUniformData.unmapElements(
            m_impl.get(),
            &RenderContextImpl::unmapImageDrawUniformBuffer,
            mapCounts.imageDrawUniformBufferCount);
    }
    if (m_pathData)
    {
        m_pathData.unmapElements(m_impl.get(),
                                 &RenderContextImpl::unmapPathBuffer,
                                 mapCounts.pathBufferCount);
    }
    if (m_paintData)
    {
        m_paintData.unmapElements(m_impl.get(),
                                  &RenderContextImpl::unmapPaintBuffer,
                                  mapCounts.paintBufferCount);
    }
    if (m_paintAuxData)
    {
        m_paintAuxData.unmapElements(m_impl.get(),
                                     &RenderContextImpl::unmapPaintAuxBuffer,
                                     mapCounts.paintAuxBufferCount);
    }
    if (m_contourData)
    {
        m_contourData.unmapElements(m_impl.get(),
                                    &RenderContextImpl::unmapContourBuffer,
                                    mapCounts.contourBufferCount);
    }
    if (m_gradSpanData)
    {
        m_gradSpanData.unmapElements(m_impl.get(),
                                     &RenderContextImpl::unmapGradSpanBuffer,
                                     mapCounts.gradSpanBufferCount);
    }
    if (m_tessSpanData)
    {
        m_tessSpanData.unmapElements(
            m_impl.get(),
            &RenderContextImpl::unmapTessVertexSpanBuffer,
            mapCounts.tessSpanBufferCount);
    }
    if (m_triangleVertexData)
    {
        m_triangleVertexData.unmapElements(
            m_impl.get(),
            &RenderContextImpl::unmapTriangleVertexBuffer,
            mapCounts.triangleVertexBufferCount);
    }
}

uint32_t RenderContext::incrementCoverageBufferPrefix(
    bool* needsCoverageBufferClear)
{
    RIVE_PROF_SCOPE()
    assert(m_didBeginFrame);
    assert(frameInterlockMode() == gpu::InterlockMode::clockwiseAtomic);
    do
    {
        if (m_coverageBufferPrefix == 0)
        {
            // When the prefix wraps around to 0, we need to clear the coverage
            // buffer because our shaders require coverageBufferPrefix to be
            // monotonically increasing.
            *needsCoverageBufferClear = true;
        }
        m_coverageBufferPrefix += 1 << CLOCKWISE_COVERAGE_BIT_COUNT;
    } while (m_coverageBufferPrefix == 0);

    return m_coverageBufferPrefix;
}

uint32_t RenderContext::LogicalFlush::allocateMidpointFanTessVertices(
    uint32_t count)
{
    uint32_t location = m_midpointFanTessVertexIdx;
    m_midpointFanTessVertexIdx += count;
    assert(m_midpointFanTessVertexIdx <= m_midpointFanTessEndLocation);
    return location;
}

uint32_t RenderContext::LogicalFlush::allocateOuterCubicTessVertices(
    uint32_t count)
{
    uint32_t location = m_outerCubicTessVertexIdx;
    m_outerCubicTessVertexIdx += count;
    assert(m_outerCubicTessVertexIdx <= m_outerCubicTessEndLocation);
    return location;
}

uint32_t RenderContext::LogicalFlush::pushPath(const PathDraw* draw)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    ++m_currentPathID;
    assert(0 < m_currentPathID && m_currentPathID <= m_ctx->m_maxPathID);

    m_ctx->m_pathData.set_back(draw->matrix(),
                               draw->strokeRadius(),
                               draw->featherRadius(),
                               m_currentZIndex,
                               draw->atlasTransform(),
                               draw->coverageBufferRange());
    m_ctx->m_paintData.set_back(draw->drawContents(),
                                draw->paintType(),
                                draw->simplePaintValue(),
                                m_gradTextureLayout,
                                draw->clipID(),
                                draw->hasClipRect(),
                                draw->blendMode());
    m_ctx->m_paintAuxData.set_back(draw->matrix(),
                                   draw->paintType(),
                                   draw->simplePaintValue(),
                                   draw->gradient(),
                                   draw->imageTexture(),
                                   draw->clipRectInverseMatrix(),
                                   m_flushDesc.renderTarget,
                                   m_ctx->platformFeatures());

    assert(m_flushDesc.firstPath + m_currentPathID + 1 ==
           m_ctx->m_pathData.elementsWritten());
    assert(m_flushDesc.firstPaint + m_currentPathID + 1 ==
           m_ctx->m_paintData.elementsWritten());
    assert(m_flushDesc.firstPaintAux + m_currentPathID + 1 ==
           m_ctx->m_paintAuxData.elementsWritten());

    return m_currentPathID;
}

RenderContext::TessellationWriter::TessellationWriter(
    LogicalFlush* flush,
    uint32_t pathID,
    gpu::ContourDirections contourDirections,
    uint32_t forwardTessVertexCount,
    uint32_t forwardTessLocation,
    uint32_t mirroredTessVertexCount,
    uint32_t mirroredTessLocation) :
    m_flush(flush),
    m_tessSpanData(m_flush->m_ctx->m_tessSpanData),
    m_pathID(pathID),
    m_contourDirections(contourDirections),
    m_pathTessLocation(forwardTessLocation),
    m_pathMirroredTessLocation(mirroredTessLocation)
{
    RIVE_PROF_SCOPE()
    RIVE_DEBUG_CODE(m_expectedPathTessEndLocation =
                        m_pathTessLocation + forwardTessVertexCount;)
    RIVE_DEBUG_CODE(m_expectedPathMirroredTessEndLocation =
                        m_pathMirroredTessLocation - mirroredTessVertexCount;)
    assert(m_flush->m_hasDoneLayout);
    assert(m_flush->m_ctx->m_pathData.elementsWritten() > 0);
    assert(forwardTessVertexCount == 0 || mirroredTessVertexCount == 0 ||
           forwardTessVertexCount == mirroredTessVertexCount);
    assert(!gpu::ContourDirectionsAreDoubleSided(m_contourDirections) ||
           forwardTessVertexCount == mirroredTessVertexCount);
    assert(m_pathTessLocation >= 0);
    assert(m_pathMirroredTessLocation <= kMaxTessellationVertexCount);
    assert(m_expectedPathTessEndLocation <= kMaxTessellationVertexCount);
    assert(m_expectedPathMirroredTessEndLocation >= 0);
}

RenderContext::TessellationWriter::~TessellationWriter()
{
    assert(m_pathTessLocation == m_expectedPathTessEndLocation);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessEndLocation);
}

uint32_t RenderContext::LogicalFlush::pushContour(uint32_t pathID,
                                                  Vec2D midpoint,
                                                  bool isStroke,
                                                  bool closed,
                                                  uint32_t vertexIndex0)
{
    RIVE_PROF_SCOPE()
    assert(pathID != 0);
    assert(isStroke || closed);

    if (isStroke)
    {
        midpoint.x = closed ? 1 : 0;
    }
    m_ctx->m_contourData.emplace_back(midpoint, pathID, vertexIndex0);

    ++m_currentContourID;
    assert(0 < m_currentContourID && m_currentContourID <= gpu::kMaxContourID);
    assert(m_flushDesc.firstContour + m_currentContourID ==
           m_ctx->m_contourData.elementsWritten());
    return m_currentContourID;
}

uint32_t RenderContext::TessellationWriter::pushContour(
    Vec2D midpoint,
    bool isStroke,
    bool closed,
    uint32_t paddingVertexCount)
{
    RIVE_PROF_SCOPE()
    // The first curve of the contour will be pre-padded with
    // 'paddingVertexCount' tessellation vertices, colocated at T=0. The caller
    // must use this argument align the end of the contour on a boundary of the
    // patch size. (See math::padding_to_align_up().)
    m_nextCubicPaddingVertexCount = paddingVertexCount;

    return m_flush->pushContour(m_pathID,
                                midpoint,
                                isStroke,
                                closed,
                                nextVertexIndex());
}

void RenderContext::TessellationWriter::pushCubic(
    const Vec2D pts[4],
    gpu::ContourDirections contourDirections,
    Vec2D joinTangent,
    uint32_t parametricSegmentCount,
    uint32_t polarSegmentCount,
    uint32_t joinSegmentCount,
    uint32_t contourIDWithFlags)
{
    RIVE_PROF_SCOPE()
    assert(0 <= parametricSegmentCount &&
           parametricSegmentCount <= kMaxParametricSegments);
    assert(0 <= polarSegmentCount && polarSegmentCount <= kMaxPolarSegments);
    assert(joinSegmentCount > 0);
    assert((contourIDWithFlags & 0xffff) ==
           (m_flush->m_currentContourID & 0xffff));
    assert((contourIDWithFlags & 0xffff) != 0); // contourID can't be zero.

    // Polar and parametric segments share the same beginning and ending
    // vertices, so the merged *vertex* count is equal to the sum of polar and
    // parametric *segment* counts.
    uint32_t curveMergedVertexCount =
        parametricSegmentCount + polarSegmentCount;
    // -1 because the curve and join share an ending/beginning vertex.
    uint32_t totalVertexCount = m_nextCubicPaddingVertexCount +
                                curveMergedVertexCount + joinSegmentCount - 1;

    // Only the first curve of a contour gets padding vertices.
    m_nextCubicPaddingVertexCount = 0;

    switch (contourDirections)
    {
        case gpu::ContourDirections::forward:
            pushTessellationSpans(pts,
                                  joinTangent,
                                  totalVertexCount,
                                  parametricSegmentCount,
                                  polarSegmentCount,
                                  joinSegmentCount,
                                  contourIDWithFlags);
            break;
        case gpu::ContourDirections::reverse:
            pushMirroredTessellationSpans(pts,
                                          joinTangent,
                                          totalVertexCount,
                                          parametricSegmentCount,
                                          polarSegmentCount,
                                          joinSegmentCount,
                                          contourIDWithFlags);
            break;
        case gpu::ContourDirections::reverseThenForward:
        case gpu::ContourDirections::forwardThenReverse:
            // m_pathTessLocation and m_pathMirroredTessLocation are already
            // configured, so at ths point we don't need to handle
            // reverseThenForward or forwardThenReverse differently.
            pushDoubleSidedTessellationSpans(pts,
                                             joinTangent,
                                             totalVertexCount,
                                             parametricSegmentCount,
                                             polarSegmentCount,
                                             joinSegmentCount,
                                             contourIDWithFlags);
            break;
    }
}

RIVE_ALWAYS_INLINE void RenderContext::TessellationWriter::
    pushTessellationSpans(const Vec2D pts[4],
                          Vec2D joinTangent,
                          uint32_t totalVertexCount,
                          uint32_t parametricSegmentCount,
                          uint32_t polarSegmentCount,
                          uint32_t joinSegmentCount,
                          uint32_t contourIDWithFlags)
{
    RIVE_PROF_SCOPE()
    assert(totalVertexCount > 0);

    uint32_t y = m_pathTessLocation / kTessTextureWidth;
    int32_t x0 = m_pathTessLocation % kTessTextureWidth;
    int32_t x1 = x0 + totalVertexCount;
    for (;;)
    {
        m_tessSpanData.set_back(pts,
                                joinTangent,
                                static_cast<float>(y),
                                x0,
                                x1,
                                parametricSegmentCount,
                                polarSegmentCount,
                                joinSegmentCount,
                                contourIDWithFlags);
        if (x1 > static_cast<int32_t>(kTessTextureWidth))
        {
            // The span was too long to fit on the current line. Wrap and draw
            // it again, this time behind the left edge of the texture so we
            // capture what got clipped off last time.
            ++y;
            x0 -= kTessTextureWidth;
            x1 -= kTessTextureWidth;
            continue;
        }
        break;
    }
    assert(y ==
           (m_pathTessLocation + totalVertexCount - 1) / kTessTextureWidth);

    m_pathTessLocation += totalVertexCount;
    assert(m_pathTessLocation <= m_expectedPathTessEndLocation);
}

RIVE_ALWAYS_INLINE void RenderContext::TessellationWriter::
    pushMirroredTessellationSpans(const Vec2D pts[4],
                                  Vec2D joinTangent,
                                  uint32_t totalVertexCount,
                                  uint32_t parametricSegmentCount,
                                  uint32_t polarSegmentCount,
                                  uint32_t joinSegmentCount,
                                  uint32_t contourIDWithFlags)
{
    assert(totalVertexCount > 0);

    uint32_t reflectionY = (m_pathMirroredTessLocation - 1) / kTessTextureWidth;
    int32_t reflectionX0 =
        (m_pathMirroredTessLocation - 1) % kTessTextureWidth + 1;
    int32_t reflectionX1 = reflectionX0 - totalVertexCount;

    for (;;)
    {
        m_tessSpanData.set_back(pts,
                                joinTangent,
                                static_cast<float>(reflectionY),
                                reflectionX0,
                                reflectionX1,
                                parametricSegmentCount,
                                polarSegmentCount,
                                joinSegmentCount,
                                contourIDWithFlags);
        if (reflectionX1 < 0)
        {
            --reflectionY;
            reflectionX0 += kTessTextureWidth;
            reflectionX1 += kTessTextureWidth;
            continue;
        }
        break;
    }

    m_pathMirroredTessLocation -= totalVertexCount;
    assert(m_pathMirroredTessLocation >= m_expectedPathMirroredTessEndLocation);
}

RIVE_ALWAYS_INLINE void RenderContext::TessellationWriter::
    pushDoubleSidedTessellationSpans(const Vec2D pts[4],
                                     Vec2D joinTangent,
                                     uint32_t totalVertexCount,
                                     uint32_t parametricSegmentCount,
                                     uint32_t polarSegmentCount,
                                     uint32_t joinSegmentCount,
                                     uint32_t contourIDWithFlags)
{
    assert(totalVertexCount > 0);

    int32_t y = m_pathTessLocation / kTessTextureWidth;
    int32_t x0 = m_pathTessLocation % kTessTextureWidth;
    int32_t x1 = x0 + totalVertexCount;

    uint32_t reflectionY = (m_pathMirroredTessLocation - 1) / kTessTextureWidth;
    int32_t reflectionX0 =
        (m_pathMirroredTessLocation - 1) % kTessTextureWidth + 1;
    int32_t reflectionX1 = reflectionX0 - totalVertexCount;

    for (;;)
    {
        m_tessSpanData.set_back(pts,
                                joinTangent,
                                static_cast<float>(y),
                                x0,
                                x1,
                                static_cast<float>(reflectionY),
                                reflectionX0,
                                reflectionX1,
                                parametricSegmentCount,
                                polarSegmentCount,
                                joinSegmentCount,
                                contourIDWithFlags);
        if (x1 > static_cast<int32_t>(kTessTextureWidth) || reflectionX1 < 0)
        {
            // Either the span or its reflection was too long to fit on the
            // current line. Wrap and draw both of them again, this time beyond
            // the opposite edge of the texture so we capture what got clipped
            // off last time.
            ++y;
            x0 -= kTessTextureWidth;
            x1 -= kTessTextureWidth;

            --reflectionY;
            reflectionX0 += kTessTextureWidth;
            reflectionX1 += kTessTextureWidth;
            continue;
        }
        break;
    }

    m_pathTessLocation += totalVertexCount;
    assert(m_pathTessLocation <= m_expectedPathTessEndLocation);

    m_pathMirroredTessLocation -= totalVertexCount;
    assert(m_pathMirroredTessLocation >= m_expectedPathMirroredTessEndLocation);
}

void RenderContext::LogicalFlush::pushPaddingVertices(uint32_t count,
                                                      uint32_t tessLocation)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);
    assert(count > 0);

    constexpr static Vec2D kEmptyCubic[4]{};
    TessellationWriter(this,
                       /*pathID=*/0,
                       gpu::ContourDirections::forward,
                       count,
                       tessLocation)
        .pushTessellationSpans(kEmptyCubic,
                               {0, 0},
                               count,
                               0,
                               0,
                               1,
                               INVALID_CONTOUR_ID_WITH_FLAGS);
}

void RenderContext::LogicalFlush::pushMidpointFanDraw(
    const PathDraw* draw,
    gpu::DrawType drawType,
    uint32_t tessVertexCount,
    uint32_t tessLocation,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    uint32_t baseInstance = math::lossless_numeric_cast<uint32_t>(
        tessLocation / kMidpointFanPatchSegmentSpan);
    // flush() is responsible for alignment.
    assert(baseInstance * kMidpointFanPatchSegmentSpan == tessLocation);

    uint32_t instanceCount = tessVertexCount / kMidpointFanPatchSegmentSpan;
    // flush() is responsible for alignment.
    assert(instanceCount * kMidpointFanPatchSegmentSpan == tessVertexCount);

    pushPathDraw(draw, drawType, shaderMiscFlags, instanceCount, baseInstance);
}

void RenderContext::LogicalFlush::pushOuterCubicsDraw(
    const PathDraw* draw,
    gpu::DrawType drawType,
    uint32_t tessVertexCount,
    uint32_t tessLocation,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    uint32_t baseInstance = math::lossless_numeric_cast<uint32_t>(
        tessLocation / kOuterCurvePatchSegmentSpan);
    // flush() is responsible for alignment.
    assert(baseInstance * kOuterCurvePatchSegmentSpan == tessLocation);

    uint32_t instanceCount = tessVertexCount / kOuterCurvePatchSegmentSpan;
    // flush() is responsible for alignment.
    assert(instanceCount * kOuterCurvePatchSegmentSpan == tessVertexCount);

    pushPathDraw(draw, drawType, shaderMiscFlags, instanceCount, baseInstance);
}

size_t RenderContext::LogicalFlush::pushInteriorTriangulationDraw(
    const PathDraw* draw,
    uint32_t pathID,
    gpu::WindingFaces windingFaces,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);
    assert(pathID != 0);

    uint32_t baseVertex = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_triangleVertexData.elementsWritten());
    size_t actualVertexCount =
        draw->triangulator()->polysToTriangles(pathID,
                                               windingFaces,
                                               &m_ctx->m_triangleVertexData);
    assert(baseVertex + actualVertexCount ==
           m_ctx->m_triangleVertexData.elementsWritten());
    if (actualVertexCount > 0)
    {
        pushPathDraw(draw,
                     DrawType::interiorTriangulation,
                     shaderMiscFlags,
                     math::lossless_numeric_cast<uint32_t>(actualVertexCount),
                     baseVertex);
    }
    return actualVertexCount;
}

void RenderContext::LogicalFlush::pushAtlasBlit(PathDraw* draw, uint32_t pathID)
{
    RIVE_PROF_SCOPE()
    auto baseVertex = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_triangleVertexData.elementsWritten());
    auto [l, t, r, b] = AABB(draw->pixelBounds());
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, b}, 1, pathID);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, t}, 1, pathID);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, b}, 1, pathID);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, b}, 1, pathID);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, t}, 1, pathID);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, t}, 1, pathID);
    pushPathDraw(draw,
                 DrawType::atlasBlit,
                 gpu::ShaderMiscFlags::none,
                 6,
                 baseVertex);
}

void RenderContext::LogicalFlush::pushImageRectDraw(ImageRectDraw* draw)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    // If we support image paints for paths, the client should use pushPath()
    // with an image paint instead of calling this method.
    assert(!m_ctx->frameSupportsImagePaintForPaths());

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(draw->matrix(),
                                               draw->opacity(),
                                               draw->clipRectInverseMatrix(),
                                               draw->clipID(),
                                               draw->blendMode(),
                                               m_currentZIndex);

    DrawBatch& batch = pushDraw(draw,
                                DrawType::imageRect,
                                gpu::ShaderMiscFlags::none,
                                PaintType::image,
                                1,
                                0);
    batch.imageDrawDataOffset =
        math::lossless_numeric_cast<uint32_t>(imageDrawDataOffset);
}

void RenderContext::LogicalFlush::pushImageMeshDraw(ImageMeshDraw* draw)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(draw->matrix(),
                                               draw->opacity(),
                                               draw->clipRectInverseMatrix(),
                                               draw->clipID(),
                                               draw->blendMode(),
                                               m_currentZIndex);

    DrawBatch& batch = pushDraw(draw,
                                DrawType::imageMesh,
                                gpu::ShaderMiscFlags::none,
                                PaintType::image,
                                draw->indexCount(),
                                0);
    batch.vertexBuffer = draw->vertexBuffer();
    batch.uvBuffer = draw->uvBuffer();
    batch.indexBuffer = draw->indexBuffer();
    batch.imageDrawDataOffset =
        math::lossless_numeric_cast<uint32_t>(imageDrawDataOffset);
}

void RenderContext::LogicalFlush::pushStencilClipResetDraw(
    StencilClipReset* draw)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    uint32_t baseVertex = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_triangleVertexData.elementsWritten());
    auto [l, t, r, b] = AABB(getClipInfo(draw->previousClipID()).contentBounds);
    uint32_t z = m_currentZIndex;
    assert(AABB(l, t, r, b).round() == draw->pixelBounds());
    assert(draw->resourceCounts().maxTriangleVertexCount == 6);
    assert(m_ctx->m_triangleVertexData.hasRoomFor(6));
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, b}, 0, z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, t}, 0, z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, b}, 0, z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, b}, 0, z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{l, t}, 0, z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{r, t}, 0, z);
    pushDraw(draw,
             DrawType::msaaStencilClipReset,
             gpu::ShaderMiscFlags::none,
             PaintType::clipUpdate,
             6,
             baseVertex);
}

gpu::DrawBatch& RenderContext::LogicalFlush::pushPathDraw(
    const PathDraw* draw,
    DrawType drawType,
    gpu::ShaderMiscFlags shaderMiscFlags,
    uint32_t vertexCount,
    uint32_t baseVertex)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);

    DrawBatch& batch = pushDraw(draw,
                                drawType,
                                shaderMiscFlags,
                                draw->paintType(),
                                vertexCount,
                                baseVertex);

    auto pathShaderFeatures = gpu::ShaderFeatures::NONE;
    if (draw->featherRadius() != 0 &&
        drawType != gpu::DrawType::interiorTriangulation &&
        drawType != gpu::DrawType::atlasBlit)
    {
        pathShaderFeatures |= ShaderFeatures::ENABLE_FEATHER;
    }
    if (draw->drawContents() & gpu::DrawContents::evenOddFill)
    {
        assert(!(shaderMiscFlags & gpu::ShaderMiscFlags::clockwiseFill));
        pathShaderFeatures |= ShaderFeatures::ENABLE_EVEN_ODD;
    }
    constexpr static gpu::DrawContents NESTED_CLIP_FLAGS =
        gpu::DrawContents::clipUpdate | gpu::DrawContents::activeClip;
    if ((draw->drawContents() & NESTED_CLIP_FLAGS) == NESTED_CLIP_FLAGS)
    {
        pathShaderFeatures |= ShaderFeatures::ENABLE_NESTED_CLIPPING;
    }
    batch.shaderFeatures |=
        pathShaderFeatures & m_ctx->m_frameShaderFeaturesMask;
    m_combinedShaderFeatures |= batch.shaderFeatures;
    assert(
        (batch.shaderFeatures &
         gpu::ShaderFeaturesMaskFor(drawType, m_ctx->frameInterlockMode())) ==
        batch.shaderFeatures);
    return batch;
}

RIVE_ALWAYS_INLINE static bool can_combine_draw_contents(
    gpu::InterlockMode interlockMode,
    gpu::DrawContents batchContents,
    const Draw* draw)
{
    // Feathered fills should never attempt to combine with fills, strokes, or
    // feathered strokes because they use a different DrawType.
    assert((batchContents & gpu::DrawContents::featheredFill).bits() ==
           (draw->drawContents() & gpu::DrawContents::featheredFill).bits());

    constexpr static auto ANY_FILL = gpu::DrawContents::clockwiseFill |
                                     gpu::DrawContents::evenOddFill |
                                     gpu::DrawContents::nonZeroFill;
    // Raster ordering uses a different shader for clockwise fills, so we
    // can't combine both legacy and clockwise fills into the same draw.
    if (interlockMode == gpu::InterlockMode::rasterOrdering &&
        // Anything can be combined if either the existing batch or the new draw
        // don't have fills yet.
        (batchContents & ANY_FILL) && (draw->drawContents() & ANY_FILL))
    {
        assert(!(draw->drawContents() & gpu::DrawContents::stroke));
        return (batchContents & gpu::DrawContents::clockwiseFill).bits() ==
               (draw->drawContents() & gpu::DrawContents::clockwiseFill).bits();
    }
    return true;
}

RIVE_ALWAYS_INLINE static bool can_combine_draw_images(
    const Texture* currentDrawTexture,
    const Texture* nextDrawTexture,
    const ImageSampler currentImageSamplerKey,
    const ImageSampler nextImageSamplerKey)
{
    if (currentDrawTexture == nullptr || nextDrawTexture == nullptr)
    {
        // We can always combine two draws if one or both do not use an image
        // paint.
        return true;
    }
    // Since the image paint's texture must be bound to a specific slot, we
    // can't combine draws that use different textures.
    return (currentDrawTexture == nextDrawTexture) &&
           (currentImageSamplerKey == nextImageSamplerKey);
}

gpu::DrawBatch& RenderContext::LogicalFlush::pushDraw(
    const Draw* draw,
    DrawType drawType,
    gpu::ShaderMiscFlags shaderMiscFlags,
    gpu::PaintType paintType,
    uint32_t elementCount,
    uint32_t baseElement)
{
    RIVE_PROF_SCOPE()
    assert(m_hasDoneLayout);
    assert(elementCount > 0);

    bool canMergeWithPreviousBatch;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::midpointFanCenterAAPatches:
        case DrawType::outerCurvePatches:
        case DrawType::interiorTriangulation:
        case DrawType::atlasBlit:
        case DrawType::msaaStrokes:
        case DrawType::msaaMidpointFanBorrowedCoverage:
        case DrawType::msaaMidpointFans:
        case DrawType::msaaMidpointFanStencilReset:
        case DrawType::msaaMidpointFanPathsStencil:
        case DrawType::msaaMidpointFanPathsCover:
        case DrawType::msaaOuterCubics:
        case DrawType::msaaStencilClipReset:
            if (!m_drawList.empty() && m_pendingBarriers == BarrierFlags::none)
            {
                const DrawBatch& currentBatch = m_drawList.tail();
                canMergeWithPreviousBatch =
                    currentBatch.drawType == drawType &&
                    currentBatch.shaderMiscFlags == shaderMiscFlags &&
                    can_combine_draw_contents(m_ctx->frameInterlockMode(),
                                              currentBatch.drawContents,
                                              draw) &&
                    can_combine_draw_images(currentBatch.imageTexture,
                                            draw->imageTexture(),
                                            currentBatch.imageSampler,
                                            draw->imageSampler());
                if (canMergeWithPreviousBatch &&
                    currentBatch.baseElement + currentBatch.elementCount !=
                        baseElement)
                {
                    // In MSAA mode, multiple subpasses reference the same
                    // tessellation data. Although rare, this breaks the
                    // guarantee we have in other modes that mergeable batches
                    // will always have contiguous patches.
                    assert(m_ctx->frameInterlockMode() ==
                           gpu::InterlockMode::msaa);
                    canMergeWithPreviousBatch = false;
                }
                break;
            }
            [[fallthrough]];

        // Image draws can't be combined for now because they each have their
        // own unique uniforms.
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::atomicInitialize:
        case DrawType::atomicResolve:
            canMergeWithPreviousBatch = false;
            break;
    }

    DrawBatch* batch;
    if (canMergeWithPreviousBatch)
    {
        batch = &m_drawList.tail();
        assert(m_pendingBarriers == BarrierFlags::none);
        assert(batch->drawType == drawType);
        assert(batch->shaderMiscFlags == shaderMiscFlags);
        assert(batch->baseElement + batch->elementCount == baseElement);
        batch->elementCount += elementCount;
    }
    else
    {
        batch = &m_drawList.emplace_back(
            m_ctx->perFrameAllocator(),
            drawType,
            shaderMiscFlags,
            elementCount,
            baseElement,
            draw->blendMode(),
            draw->imageSampler(),
            std::exchange(m_pendingBarriers, BarrierFlags::none));
    }

    // If the batch was merged into a previous one, this ensures it was a valid
    // merge.
    assert(batch->drawType == drawType);
    assert(can_combine_draw_images(batch->imageTexture,
                                   draw->imageTexture(),
                                   batch->imageSampler,
                                   draw->imageSampler()));
    assert(m_pendingBarriers == BarrierFlags::none);

    auto shaderFeatures = ShaderFeatures::NONE;
    if (draw->clipID() != 0)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIPPING;
    }
    if (draw->hasClipRect() && paintType != PaintType::clipUpdate)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIP_RECT;
    }
    if (paintType != PaintType::clipUpdate &&
        !(shaderMiscFlags & gpu::ShaderMiscFlags::borrowedCoveragePrepass))
    {
        switch (draw->blendMode())
        {
            case BlendMode::hue:
            case BlendMode::saturation:
            case BlendMode::color:
            case BlendMode::luminosity:
                shaderFeatures |= ShaderFeatures::ENABLE_HSL_BLEND_MODES;
                [[fallthrough]];
            case BlendMode::screen:
            case BlendMode::overlay:
            case BlendMode::darken:
            case BlendMode::lighten:
            case BlendMode::colorDodge:
            case BlendMode::colorBurn:
            case BlendMode::hardLight:
            case BlendMode::softLight:
            case BlendMode::difference:
            case BlendMode::exclusion:
            case BlendMode::multiply:
                shaderFeatures |= ShaderFeatures::ENABLE_ADVANCED_BLEND;
                break;
            case BlendMode::srcOver:
                break;
        }
    }
    batch->shaderFeatures |= shaderFeatures & m_ctx->m_frameShaderFeaturesMask;
    m_combinedShaderFeatures |= batch->shaderFeatures;
    assert(
        (batch->shaderFeatures &
         gpu::ShaderFeaturesMaskFor(drawType, m_ctx->frameInterlockMode())) ==
        batch->shaderFeatures);

    batch->drawContents |= draw->drawContents();

    if (paintType == PaintType::image)
    {
        assert(draw->imageTexture() != nullptr);
        if (batch->imageTexture == nullptr)
        {
            batch->imageTexture = draw->imageTexture();
        }
        assert(batch->imageTexture == draw->imageTexture());
    }

    if (m_ctx->frameInterlockMode() == gpu::InterlockMode::msaa)
    {
        // msaa can't mix drawContents in a batch.
        assert(batch->drawContents == draw->drawContents());
        // msaa does't mix src-over draws with advanced blend draws.
        assert((batch->shaderFeatures &
                gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND) ==
               (draw->blendMode() != BlendMode::srcOver));
        // If using KHR_blend_equation_advanced, we can't mix blend modes in a
        // batch.
        assert(!m_ctx->platformFeatures().supportsBlendAdvancedKHR ||
               batch->firstBlendMode == draw->blendMode());
        if (draw->blendMode() != BlendMode::srcOver &&
            !m_ctx->platformFeatures().supportsBlendAdvancedCoherentKHR)
        {
            // An implementation-dependent barrier is required between
            // overlapping draws. Add a "dstBlend" barrier and build up a list
            // of "dstReads" for the batch. The dstRead list will be required in
            // the event that the implementation has to handle dstReads by
            // copying out a texture.
            //
            // (But if the draw already has a "nextDstRead" neighbor, do
            // nothing. It means an earlier subpass will already issue the
            // barrier and sync this region of the framebuffer. Since nothing
            // that overlaps will be ordered between that first subpass and us,
            // that barrier for the first subpass is all we need.)
            if (draw->nextDstRead() == nullptr)
            {
                batch->barriers |= BarrierFlags::dstBlend;
                batch->dstReadList = draw->addToDstReadList(batch->dstReadList);
            }
        }
    }

    return *batch;
}
} // namespace rive::gpu
