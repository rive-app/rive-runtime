/*
 * Copyright 2022 Rive
 */

#include "rive/renderer/render_context.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "intersection_board.hpp"
#include "rive_render_paint.hpp"
#include "rive/renderer/draw.hpp"
#include "rive/renderer/image.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "shaders/constants.glsl"

#include <string_view>

namespace rive::gpu
{
constexpr size_t kDefaultSimpleGradientCapacity = 512;
constexpr size_t kDefaultComplexGradientCapacity = 1024;
constexpr size_t kDefaultDrawCapacity = 2048;

constexpr uint32_t kMaxTextureHeight = 2048; // TODO: Move this variable to PlatformFeatures.
constexpr size_t kMaxTessellationVertexCount = kMaxTextureHeight * kTessTextureWidth;
constexpr size_t kMaxTessellationPaddingVertexCount =
    gpu::kMidpointFanPatchSegmentSpan +      // Padding at the beginning of the tess texture
    (gpu::kOuterCurvePatchSegmentSpan - 1) + // Max padding between patch types in the tess texture
    1;                                       // Padding at the end of the tessellation texture
constexpr size_t kMaxTessellationVertexCountBeforePadding =
    kMaxTessellationVertexCount - kMaxTessellationPaddingVertexCount;

// Metal requires vertex buffers to be 256-byte aligned.
constexpr size_t kMaxTessellationAlignmentVertices = gpu::kTessVertexBufferAlignmentInElements - 1;

// We can only reorder 32767 draws at a time since the one-based groupIndex returned by
// IntersectionBoard is a signed 16-bit integer.
constexpr size_t kMaxReorderedDrawCount = std::numeric_limits<int16_t>::max();

// How tall to make a resource texture in order to support the given number of items.
template <size_t WidthInItems> constexpr static size_t resource_texture_height(size_t itemCount)
{
    return (itemCount + WidthInItems - 1) / WidthInItems;
}

constexpr static size_t gradient_data_height(size_t simpleRampCount, size_t complexRampCount)
{
    return resource_texture_height<gpu::kGradTextureWidthInSimpleRamps>(simpleRampCount) +
           complexRampCount;
}

inline GradientContentKey::GradientContentKey(rcp<const PLSGradient> gradient) :
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
    const PLSGradient* grad = key.gradient();
    std::hash<std::string_view> hash;
    size_t x = hash(std::string_view(reinterpret_cast<const char*>(grad->stops()),
                                     grad->count() * sizeof(float)));
    size_t y = hash(std::string_view(reinterpret_cast<const char*>(grad->colors()),
                                     grad->count() * sizeof(ColorInt)));
    return x ^ y;
}

PLSRenderContext::PLSRenderContext(std::unique_ptr<PLSRenderContextImpl> impl) :
    m_impl(std::move(impl)),
    // -1 from m_maxPathID so we reserve a path record for the clearColor paint (for atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    m_maxPathID(MaxPathID(m_impl->platformFeatures().pathIDGranularity) - 1)
{
    setResourceSizes(ResourceAllocationCounts(), /*forceRealloc =*/true);
    releaseResources();
}

PLSRenderContext::~PLSRenderContext()
{
    // Always call flush() to avoid deadlock.
    assert(!m_didBeginFrame);
    // Delete the logical flushes before the block allocators let go of their allocations.
    m_logicalFlushes.clear();
}

const gpu::PlatformFeatures& PLSRenderContext::platformFeatures() const
{
    return m_impl->platformFeatures();
}

rcp<RenderBuffer> PLSRenderContext::makeRenderBuffer(RenderBufferType type,
                                                     RenderBufferFlags flags,
                                                     size_t sizeInBytes)
{
    return m_impl->makeRenderBuffer(type, flags, sizeInBytes);
}

rcp<RenderImage> PLSRenderContext::decodeImage(Span<const uint8_t> encodedBytes)
{
    rcp<PLSTexture> texture = m_impl->decodeImageTexture(encodedBytes);
    return texture != nullptr ? make_rcp<PLSImage>(std::move(texture)) : nullptr;
}

void PLSRenderContext::releaseResources()
{
    assert(!m_didBeginFrame);
    resetContainers();
    setResourceSizes(ResourceAllocationCounts());
    m_maxRecentResourceRequirements = ResourceAllocationCounts();
    m_lastResourceTrimTimeInSeconds = m_impl->secondsNow();
}

void PLSRenderContext::resetContainers()
{
    assert(!m_didBeginFrame);

    if (!m_logicalFlushes.empty())
    {
        assert(m_logicalFlushes.size() == 1); // Should get reset to 1 after flush().
        m_logicalFlushes.resize(1);
        m_logicalFlushes.front()->resetContainers();
    }

    m_indirectDrawList.clear();
    m_indirectDrawList.shrink_to_fit();

    m_intersectionBoard = nullptr;
}

PLSRenderContext::LogicalFlush::LogicalFlush(PLSRenderContext* parent) : m_ctx(parent) { rewind(); }

void PLSRenderContext::LogicalFlush::rewind()
{
    m_resourceCounts = PLSDraw::ResourceCounters();
    m_simpleGradients.clear();
    m_pendingSimpleGradientWrites.clear();
    m_complexGradients.clear();
    m_pendingComplexColorRampDraws.clear();
    m_clips.clear();
    m_plsDraws.clear();
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

    m_currentPathIsStroked = false;
    m_currentPathContourDirections = gpu::ContourDirections::none;
    m_currentPathID = 0;
    m_currentContourID = 0;
    m_currentContourPaddingVertexCount = 0;
    m_pathTessLocation = 0;
    m_pathMirroredTessLocation = 0;
    RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(m_expectedPathMirroredTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(m_pathCurveCount = 0;)

    m_currentZIndex = 0;

    RIVE_DEBUG_CODE(m_hasDoneLayout = false;)
}

void PLSRenderContext::LogicalFlush::resetContainers()
{
    m_clips.clear();
    m_clips.shrink_to_fit();
    m_plsDraws.clear();
    m_plsDraws.shrink_to_fit();
    m_plsDraws.reserve(kDefaultDrawCapacity);

    m_simpleGradients.rehash(0);
    m_simpleGradients.reserve(kDefaultSimpleGradientCapacity);

    m_pendingSimpleGradientWrites.clear();
    m_pendingSimpleGradientWrites.shrink_to_fit();
    m_pendingSimpleGradientWrites.reserve(kDefaultSimpleGradientCapacity);

    m_complexGradients.rehash(0);
    m_complexGradients.reserve(kDefaultComplexGradientCapacity);

    m_pendingComplexColorRampDraws.clear();
    m_pendingComplexColorRampDraws.shrink_to_fit();
    m_pendingComplexColorRampDraws.reserve(kDefaultComplexGradientCapacity);
}

void PLSRenderContext::beginFrame(const FrameDescriptor& frameDescriptor)
{
    assert(!m_didBeginFrame);
    assert(frameDescriptor.renderTargetWidth > 0);
    assert(frameDescriptor.renderTargetHeight > 0);
    m_frameDescriptor = frameDescriptor;
    if (!platformFeatures().supportsPixelLocalStorage)
    {
        // Use 4x MSAA if we don't have pixel local storage and MSAA wasn't specified.
        m_frameDescriptor.msaaSampleCount =
            m_frameDescriptor.msaaSampleCount > 0 ? m_frameDescriptor.msaaSampleCount : 4;
    }
    if (m_frameDescriptor.msaaSampleCount > 0)
    {
        m_frameInterlockMode = gpu::InterlockMode::depthStencil;
    }
    else if (m_frameDescriptor.disableRasterOrdering || !platformFeatures().supportsRasterOrdering)
    {
        m_frameInterlockMode = gpu::InterlockMode::atomics;
    }
    else
    {
        m_frameInterlockMode = gpu::InterlockMode::rasterOrdering;
    }
    m_frameShaderFeaturesMask = gpu::ShaderFeaturesMaskFor(m_frameInterlockMode);
    if (m_logicalFlushes.empty())
    {
        m_logicalFlushes.emplace_back(new LogicalFlush(this));
    }
    RIVE_DEBUG_CODE(m_didBeginFrame = true);
}

bool PLSRenderContext::isOutsideCurrentFrame(const IAABB& pixelBounds)
{
    assert(m_didBeginFrame);
    int4 bounds = simd::load4i(&pixelBounds);
    auto renderTargetSize = simd::cast<int32_t>(
        uint2{m_frameDescriptor.renderTargetWidth, m_frameDescriptor.renderTargetHeight});
    return simd::any(bounds.xy >= renderTargetSize || bounds.zw <= 0 || bounds.xy >= bounds.zw);
}

bool PLSRenderContext::frameSupportsClipRects() const
{
    assert(m_didBeginFrame);
    return m_frameInterlockMode != gpu::InterlockMode::depthStencil ||
           platformFeatures().supportsClipPlanes;
}

bool PLSRenderContext::frameSupportsImagePaintForPaths() const
{
    assert(m_didBeginFrame);
    return m_frameInterlockMode != gpu::InterlockMode::atomics ||
           platformFeatures().supportsBindlessTextures;
}

uint32_t PLSRenderContext::generateClipID(const IAABB& contentBounds)
{
    assert(m_didBeginFrame);
    assert(!m_logicalFlushes.empty());
    return m_logicalFlushes.back()->generateClipID(contentBounds);
}

uint32_t PLSRenderContext::LogicalFlush::generateClipID(const IAABB& contentBounds)
{
    if (m_clips.size() < m_ctx->m_maxPathID) // maxClipID == maxPathID.
    {
        m_clips.emplace_back(contentBounds);
        assert(m_ctx->m_clipContentID != m_clips.size());
        return math::lossless_numeric_cast<uint32_t>(m_clips.size());
    }
    return 0; // There are no available clip IDs. The caller should flush and try again.
}

PLSRenderContext::LogicalFlush::ClipInfo& PLSRenderContext::LogicalFlush::getWritableClipInfo(
    uint32_t clipID)
{
    assert(clipID > 0);
    assert(clipID <= m_clips.size());
    return m_clips[clipID - 1];
}

void PLSRenderContext::LogicalFlush::addClipReadBounds(uint32_t clipID, const IAABB& bounds)
{
    assert(clipID > 0);
    assert(clipID <= m_clips.size());
    ClipInfo& clipInfo = getWritableClipInfo(clipID);
    clipInfo.readBounds = clipInfo.readBounds.join(bounds);
}

bool PLSRenderContext::pushDrawBatch(PLSDrawUniquePtr draws[], size_t drawCount)
{
    assert(m_didBeginFrame);
    assert(!m_logicalFlushes.empty());
    return m_logicalFlushes.back()->pushDrawBatch(draws, drawCount);
}

bool PLSRenderContext::LogicalFlush::pushDrawBatch(PLSDrawUniquePtr draws[], size_t drawCount)
{
    assert(!m_hasDoneLayout);

    if (m_flushDesc.interlockMode == gpu::InterlockMode::atomics &&
        m_drawList.count() + drawCount > kMaxReorderedDrawCount)
    {
        // We can only reorder 64k draws at a time since the sort key addresses them with a 16-bit
        // index.
        return false;
    }

    auto countsVector = m_resourceCounts.toVec();
    for (size_t i = 0; i < drawCount; ++i)
    {
        assert(!draws[i]->pixelBounds().empty());
        assert(m_ctx->frameSupportsClipRects() || draws[i]->clipRectInverseMatrix() == nullptr);
        countsVector += draws[i]->resourceCounts().toVec();
    }
    PLSDraw::ResourceCounters countsWithNewBatch = countsVector;

    // Textures have hard size limits. If new batch doesn't fit in one of the textures, the caller
    // needs to flush and try again.
    if (countsWithNewBatch.pathCount > m_ctx->m_maxPathID ||
        countsWithNewBatch.contourCount > kMaxContourID ||
        countsWithNewBatch.midpointFanTessVertexCount +
                countsWithNewBatch.outerCubicTessVertexCount >
            kMaxTessellationVertexCountBeforePadding)
    {
        return false;
    }

    // Allocate spans in the gradient texture.
    for (size_t i = 0; i < drawCount; ++i)
    {
        if (!draws[i]->allocateGradientIfNeeded(this, &countsWithNewBatch))
        {
            // The gradient doesn't fit. Give up and let the caller flush and try again.
            return false;
        }
    }

    for (size_t i = 0; i < drawCount; ++i)
    {
        m_plsDraws.push_back(std::move(draws[i]));
        m_combinedDrawBounds = m_combinedDrawBounds.join(m_plsDraws.back()->pixelBounds());
    }

    m_resourceCounts = countsWithNewBatch;
    return true;
}

bool PLSRenderContext::LogicalFlush::allocateGradient(const PLSGradient* gradient,
                                                      PLSDraw::ResourceCounters* counters,
                                                      gpu::ColorRampLocation* colorRampLocation)
{
    assert(!m_hasDoneLayout);

    const float* stops = gradient->stops();
    size_t stopCount = gradient->count();

    if (stopCount == 2 && stops[0] == 0 && stops[1] == 1)
    {
        // This is a simple gradient that can be implemented by a two-texel color ramp.
        uint64_t simpleKey;
        static_assert(sizeof(simpleKey) == sizeof(ColorInt) * 2);
        RIVE_INLINE_MEMCPY(&simpleKey, gradient->colors(), sizeof(ColorInt) * 2);
        uint32_t rampTexelsIdx;
        auto iter = m_simpleGradients.find(simpleKey);
        if (iter != m_simpleGradients.end())
        {
            rampTexelsIdx = iter->second; // This gradient is already in the texture.
        }
        else
        {
            if (gradient_data_height(m_simpleGradients.size() + 1, m_complexGradients.size()) >
                kMaxTextureHeight)
            {
                // We ran out of rows in the gradient texture. Caller has to flush and try again.
                return false;
            }
            rampTexelsIdx = math::lossless_numeric_cast<uint32_t>(m_simpleGradients.size() * 2);
            m_simpleGradients.insert({simpleKey, rampTexelsIdx});
            m_pendingSimpleGradientWrites.emplace_back().set(gradient->colors());
        }
        colorRampLocation->row = rampTexelsIdx / kGradTextureWidth;
        colorRampLocation->col = rampTexelsIdx % kGradTextureWidth;
    }
    else
    {
        // This is a complex gradient. Render it to an entire row of the gradient texture.
        GradientContentKey key(ref_rcp(gradient));
        auto iter = m_complexGradients.find(key);
        uint16_t row;
        if (iter != m_complexGradients.end())
        {
            row = iter->second; // This gradient is already in the texture.
        }
        else
        {
            if (gradient_data_height(m_simpleGradients.size(), m_complexGradients.size() + 1) >
                kMaxTextureHeight)
            {
                // We ran out of rows in the gradient texture. Caller has to flush and try again.
                return false;
            }

            size_t spanCount = stopCount + 1;
            counters->complexGradientSpanCount += spanCount;

            row = static_cast<uint32_t>(m_complexGradients.size());
            m_complexGradients.emplace(std::move(key), row);
            m_pendingComplexColorRampDraws.push_back(gradient);
        }
        colorRampLocation->row = row;
        colorRampLocation->col = ColorRampLocation::kComplexGradientMarker;
    }
    return true;
}

void PLSRenderContext::logicalFlush()
{
    assert(m_didBeginFrame);

    // Reset clipping state after every logical flush because the clip buffer is not preserved
    // between render passes.
    m_clipContentID = 0;

    // Don't issue any GPU commands between logical flushes. Instead, build up a list of flushes
    // that we will submit all at once at the end of the frame.
    m_logicalFlushes.emplace_back(new LogicalFlush(this));
}

void PLSRenderContext::flush(const FlushResources& flushResources)
{
    assert(m_didBeginFrame);
    assert(flushResources.renderTarget->width() == m_frameDescriptor.renderTargetWidth);
    assert(flushResources.renderTarget->height() == m_frameDescriptor.renderTargetHeight);

    m_clipContentID = 0;

    // Layout this frame's resource buffers and textures.
    LogicalFlush::ResourceCounters totalFrameResourceCounts;
    LogicalFlush::LayoutCounters layoutCounts;
    for (size_t i = 0; i < m_logicalFlushes.size(); ++i)
    {
        m_logicalFlushes[i]->layoutResources(flushResources,
                                             i,
                                             i == m_logicalFlushes.size() - 1,
                                             &totalFrameResourceCounts,
                                             &layoutCounts);
    }
    assert(layoutCounts.maxGradTextureHeight <= kMaxTextureHeight);
    assert(layoutCounts.maxTessTextureHeight <= kMaxTextureHeight);

    // Determine the minimum required resource allocation sizes to service this flush.
    ResourceAllocationCounts allocs;
    allocs.flushUniformBufferCount = m_logicalFlushes.size();
    allocs.imageDrawUniformBufferCount = totalFrameResourceCounts.imageDrawCount;
    allocs.pathBufferCount = totalFrameResourceCounts.pathCount + layoutCounts.pathPaddingCount;
    allocs.paintBufferCount = totalFrameResourceCounts.pathCount + layoutCounts.paintPaddingCount;
    allocs.paintAuxBufferCount =
        totalFrameResourceCounts.pathCount + layoutCounts.paintAuxPaddingCount;
    allocs.contourBufferCount =
        totalFrameResourceCounts.contourCount + layoutCounts.contourPaddingCount;
    // The gradient texture needs to be updated in entire rows at a time. Extend its
    // texture-transfer buffer's length in order to be able to serve a worst-case scenario.
    allocs.simpleGradientBufferCount =
        layoutCounts.simpleGradCount + gpu::kGradTextureWidthInSimpleRamps - 1;
    allocs.complexGradSpanBufferCount =
        totalFrameResourceCounts.complexGradientSpanCount + layoutCounts.gradSpanPaddingCount;
    allocs.tessSpanBufferCount = totalFrameResourceCounts.maxTessellatedSegmentCount;
    allocs.triangleVertexBufferCount = totalFrameResourceCounts.maxTriangleVertexCount;
    allocs.gradTextureHeight = layoutCounts.maxGradTextureHeight;
    allocs.tessTextureHeight = layoutCounts.maxTessTextureHeight;

    // Track m_maxRecentResourceRequirements so we can trim GPU allocations when steady-state usage
    // goes down.
    m_maxRecentResourceRequirements =
        simd::max(allocs.toVec(), m_maxRecentResourceRequirements.toVec());

    // Grow resources enough to handle this flush.
    // If "allocs" already fits in our current allocations, then don't change them.
    // If they don't fit, overallocate by 25% in order to create some slack for growth.
    allocs = simd::if_then_else(allocs.toVec() <= m_currentResourceAllocations.toVec(),
                                m_currentResourceAllocations.toVec(),
                                allocs.toVec() * size_t(5) / size_t(4));

    // Additionally, every 5 seconds, trim resources down to the most recent steady-state usage.
    double flushTime = m_impl->secondsNow();
    bool needsResourceTrim = flushTime - m_lastResourceTrimTimeInSeconds >= 5;
    if (needsResourceTrim)
    {
        // Trim GPU resource allocations to 125% of their maximum recent usage, and only if the
        // recent usage is 2/3 or less of the current allocation.
        allocs = simd::if_then_else(m_maxRecentResourceRequirements.toVec() <=
                                        allocs.toVec() * size_t(2) / size_t(3),
                                    m_maxRecentResourceRequirements.toVec() * size_t(5) / size_t(4),
                                    allocs.toVec());

        // Zero out m_maxRecentResourceRequirements for the next interval.
        m_maxRecentResourceRequirements = ResourceAllocationCounts();
        m_lastResourceTrimTimeInSeconds = flushTime;
    }

    setResourceSizes(allocs);

    // Write out the GPU buffers for this frame.
    mapResourceBuffers(allocs);

    for (const auto& flush : m_logicalFlushes)
    {
        flush->writeResources();
    }

    assert(m_flushUniformData.elementsWritten() == m_logicalFlushes.size());
    assert(m_imageDrawUniformData.elementsWritten() == totalFrameResourceCounts.imageDrawCount);
    assert(m_pathData.elementsWritten() ==
           totalFrameResourceCounts.pathCount + layoutCounts.pathPaddingCount);
    assert(m_paintData.elementsWritten() ==
           totalFrameResourceCounts.pathCount + layoutCounts.paintPaddingCount);
    assert(m_paintAuxData.elementsWritten() ==
           totalFrameResourceCounts.pathCount + layoutCounts.paintAuxPaddingCount);
    assert(m_contourData.elementsWritten() ==
           totalFrameResourceCounts.contourCount + layoutCounts.contourPaddingCount);
    assert(m_simpleColorRampsData.elementsWritten() == layoutCounts.simpleGradCount);
    assert(m_gradSpanData.elementsWritten() ==
           totalFrameResourceCounts.complexGradientSpanCount + layoutCounts.gradSpanPaddingCount);
    assert(m_tessSpanData.elementsWritten() <= totalFrameResourceCounts.maxTessellatedSegmentCount);
    assert(m_triangleVertexData.elementsWritten() <=
           totalFrameResourceCounts.maxTriangleVertexCount);

    unmapResourceBuffers();

    // Issue logical flushes to the backend.
    for (const auto& flush : m_logicalFlushes)
    {
        m_impl->flush(flush->desc());
    }

    if (!m_logicalFlushes.empty())
    {
        m_logicalFlushes.resize(1);
        m_logicalFlushes.front()->rewind();
    }

    // Drop all memory that was allocated for this frame using TrivialBlockAllocator.
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

void PLSRenderContext::LogicalFlush::layoutResources(const FlushResources& flushResources,
                                                     size_t logicalFlushIdx,
                                                     bool isFinalFlushOfFrame,
                                                     ResourceCounters* runningFrameResourceCounts,
                                                     LayoutCounters* runningFrameLayoutCounts)
{
    assert(!m_hasDoneLayout);

    const FrameDescriptor& frameDescriptor = m_ctx->frameDescriptor();

    // Reserve a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    ++m_resourceCounts.pathCount;

    // Storage buffer offsets are required to be aligned on multiples of 256.
    m_pathPaddingCount =
        gpu::PaddingToAlignUp<gpu::kPathBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_paintPaddingCount =
        gpu::PaddingToAlignUp<gpu::kPaintBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_paintAuxPaddingCount =
        gpu::PaddingToAlignUp<gpu::kPaintAuxBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_contourPaddingCount = gpu::PaddingToAlignUp<gpu::kContourBufferAlignmentInElements>(
        m_resourceCounts.contourCount);

    // Metal requires vertex buffers to be 256-byte aligned.
    m_gradSpanPaddingCount = gpu::PaddingToAlignUp<gpu::kGradSpanBufferAlignmentInElements>(
        m_resourceCounts.complexGradientSpanCount);

    size_t totalTessVertexCountWithPadding = 0;
    if ((m_resourceCounts.midpointFanTessVertexCount |
         m_resourceCounts.outerCubicTessVertexCount) != 0)
    {
        // midpointFan tessellation vertices reside at the beginning of the tessellation texture,
        // after 1 patch of padding vertices.
        constexpr uint32_t kPrePadding = gpu::kMidpointFanPatchSegmentSpan;
        m_midpointFanTessVertexIdx = kPrePadding;
        m_midpointFanTessEndLocation =
            m_midpointFanTessVertexIdx +
            math::lossless_numeric_cast<uint32_t>(m_resourceCounts.midpointFanTessVertexCount);

        // outerCubic tessellation vertices reside after the midpointFan vertices, aligned on a
        // multiple of the outerCubic patch size.
        uint32_t interiorPadding =
            PaddingToAlignUp<gpu::kOuterCurvePatchSegmentSpan>(m_midpointFanTessEndLocation);
        m_outerCubicTessVertexIdx = m_midpointFanTessEndLocation + interiorPadding;
        m_outerCubicTessEndLocation =
            m_outerCubicTessVertexIdx +
            math::lossless_numeric_cast<uint32_t>(m_resourceCounts.outerCubicTessVertexCount);

        // We need one more padding vertex after all the tessellation vertices.
        constexpr uint32_t kPostPadding = 1;
        totalTessVertexCountWithPadding = m_outerCubicTessEndLocation + kPostPadding;

        assert(kPrePadding + interiorPadding + kPostPadding <= kMaxTessellationPaddingVertexCount);
        assert(totalTessVertexCountWithPadding <= kMaxTessellationVertexCount);
    }

    uint32_t tessDataHeight = math::lossless_numeric_cast<uint32_t>(
        resource_texture_height<kTessTextureWidth>(totalTessVertexCountWithPadding));
    if (m_resourceCounts.maxTessellatedSegmentCount != 0)
    {
        // Conservatively account for line breaks and padding in the tessellation span count.
        // Line breaks potentially introduce a new span. Count the maximum number of line breaks we
        // might encounter, which is at most TWO for every line in the tessellation texture (one for
        // a forward span, and one for its reflection.)
        size_t maxSpanBreakCount = tessDataHeight * 2;
        // The tessellation texture requires 3 separate spans of padding vertices (see above and
        // below).
        constexpr size_t kPaddingSpanCount = 3;
        m_resourceCounts.maxTessellatedSegmentCount +=
            maxSpanBreakCount + kPaddingSpanCount + kMaxTessellationAlignmentVertices;
    }

    m_flushDesc.renderTarget = flushResources.renderTarget;
    m_flushDesc.interlockMode = m_ctx->frameInterlockMode();
    m_flushDesc.msaaSampleCount = frameDescriptor.msaaSampleCount;

    // In atomic mode, we may be able to skip the explicit clear of the color buffer and fold it
    // into the atomic "resolve" operation instead.
    bool doClearDuringAtomicResolve = false;

    if (logicalFlushIdx != 0)
    {
        // We always have to preserve the renderTarget between logical flushes.
        m_flushDesc.colorLoadAction = gpu::LoadAction::preserveRenderTarget;
    }
    else if (frameDescriptor.loadAction == gpu::LoadAction::clear)
    {
        // In atomic mode, we can clear during the resolve operation if the clearColor is opaque
        // (because we don't want or have a "source only" blend mode).
        doClearDuringAtomicResolve = m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics &&
                                     colorAlpha(frameDescriptor.clearColor) == 255;
        m_flushDesc.colorLoadAction =
            doClearDuringAtomicResolve ? gpu::LoadAction::dontCare : gpu::LoadAction::clear;
    }
    else
    {
        m_flushDesc.colorLoadAction = frameDescriptor.loadAction;
    }
    m_flushDesc.clearColor = frameDescriptor.clearColor;

    if (doClearDuringAtomicResolve)
    {
        // In atomic mode we can accomplish a clear of the color buffer while the shader resolves
        // coverage, instead of actually clearing. writeResources() will configure the fill for
        // pathID=0 to be a solid fill matching the clearColor, so if we just initialize coverage
        // buffer to solid coverage with pathID=0, the resolve step will write out the correct clear
        // color.
        assert(m_flushDesc.interlockMode == gpu::InterlockMode::atomics);
        m_flushDesc.coverageClearValue = static_cast<uint32_t>(FIXED_COVERAGE_ONE);
    }
    else if (m_flushDesc.interlockMode == gpu::InterlockMode::atomics)
    {
        // When we don't skip the initial clear in atomic mode, clear the coverage buffer to
        // pathID=0 and a transparent coverage value.
        // pathID=0 meets the requirement that pathID is always monotonically increasing.
        // Transparent coverage makes sure the clearColor doesn't get written out while resolving.
        m_flushDesc.coverageClearValue = static_cast<uint32_t>(FIXED_COVERAGE_ZERO);
    }
    else
    {
        // In non-atomic mode, the coverage buffer just needs to be initialized with "pathID=0" to
        // avoid collisions with any pathIDs being rendered.
        m_flushDesc.coverageClearValue = 0;
    }

    if (doClearDuringAtomicResolve || m_flushDesc.colorLoadAction == gpu::LoadAction::clear)
    {
        // If we're clearing then we always update the entire render target.
        m_flushDesc.renderTargetUpdateBounds = m_flushDesc.renderTarget->bounds();
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

    m_flushDesc.flushUniformDataOffsetInBytes = logicalFlushIdx * sizeof(gpu::FlushUniforms);
    m_flushDesc.pathCount = math::lossless_numeric_cast<uint32_t>(m_resourceCounts.pathCount);
    m_flushDesc.firstPath =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->pathPaddingCount;
    m_flushDesc.firstPaint =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->paintPaddingCount;
    m_flushDesc.firstPaintAux =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->paintAuxPaddingCount;
    m_flushDesc.contourCount = math::lossless_numeric_cast<uint32_t>(m_resourceCounts.contourCount);
    m_flushDesc.firstContour =
        runningFrameResourceCounts->contourCount + runningFrameLayoutCounts->contourPaddingCount;
    m_flushDesc.complexGradSpanCount =
        math::lossless_numeric_cast<uint32_t>(m_resourceCounts.complexGradientSpanCount);
    m_flushDesc.firstComplexGradSpan = runningFrameResourceCounts->complexGradientSpanCount +
                                       runningFrameLayoutCounts->gradSpanPaddingCount;
    m_flushDesc.simpleGradTexelsWidth =
        std::min<uint32_t>(math::lossless_numeric_cast<uint32_t>(m_simpleGradients.size()),
                           gpu::kGradTextureWidthInSimpleRamps) *
        2;
    m_flushDesc.simpleGradTexelsHeight = static_cast<uint32_t>(
        resource_texture_height<gpu::kGradTextureWidthInSimpleRamps>(m_simpleGradients.size()));
    m_flushDesc.simpleGradDataOffsetInBytes =
        runningFrameLayoutCounts->simpleGradCount * sizeof(gpu::TwoTexelRamp);
    m_flushDesc.complexGradRowsTop = m_flushDesc.simpleGradTexelsHeight;
    m_flushDesc.complexGradRowsHeight =
        math::lossless_numeric_cast<uint32_t>(m_complexGradients.size());
    m_flushDesc.tessDataHeight = tessDataHeight;

    m_flushDesc.externalCommandBuffer = flushResources.externalCommandBuffer;
    if (isFinalFlushOfFrame)
    {
        m_flushDesc.frameCompletionFence = flushResources.frameCompletionFence;
    }

    m_flushDesc.wireframe = frameDescriptor.wireframe;
    m_flushDesc.isFinalFlushOfFrame = isFinalFlushOfFrame;

    *runningFrameResourceCounts = runningFrameResourceCounts->toVec() + m_resourceCounts.toVec();
    runningFrameLayoutCounts->pathPaddingCount += m_pathPaddingCount;
    runningFrameLayoutCounts->paintPaddingCount += m_paintPaddingCount;
    runningFrameLayoutCounts->paintAuxPaddingCount += m_paintAuxPaddingCount;
    runningFrameLayoutCounts->contourPaddingCount += m_contourPaddingCount;
    runningFrameLayoutCounts->simpleGradCount += m_simpleGradients.size();
    runningFrameLayoutCounts->gradSpanPaddingCount += m_gradSpanPaddingCount;
    runningFrameLayoutCounts->maxGradTextureHeight =
        std::max(m_flushDesc.simpleGradTexelsHeight + m_flushDesc.complexGradRowsHeight,
                 runningFrameLayoutCounts->maxGradTextureHeight);
    runningFrameLayoutCounts->maxTessTextureHeight =
        std::max(m_flushDesc.tessDataHeight, runningFrameLayoutCounts->maxTessTextureHeight);

    assert(m_flushDesc.firstPath % gpu::kPathBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstPaint % gpu::kPaintBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstPaintAux % gpu::kPaintAuxBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstContour % gpu::kContourBufferAlignmentInElements == 0);
    assert(m_flushDesc.firstComplexGradSpan % gpu::kGradSpanBufferAlignmentInElements == 0);
    RIVE_DEBUG_CODE(m_hasDoneLayout = true;)
}

void PLSRenderContext::LogicalFlush::writeResources()
{
    const gpu::PlatformFeatures& platformFeatures = m_ctx->platformFeatures();
    assert(m_hasDoneLayout);
    assert(m_flushDesc.firstPath == m_ctx->m_pathData.elementsWritten());
    assert(m_flushDesc.firstPaint == m_ctx->m_paintData.elementsWritten());
    assert(m_flushDesc.firstPaintAux == m_ctx->m_paintAuxData.elementsWritten());

    // Wait until here to layout the gradient texture because the final gradient texture height is
    // not decided until after all LogicalFlushes have run layoutResources().
    m_gradTextureLayout.inverseHeight = 1.f / m_ctx->m_currentResourceAllocations.gradTextureHeight;
    m_gradTextureLayout.complexOffsetY = m_flushDesc.complexGradRowsTop;

    // Exact tessSpan/triangleVertex counts aren't known until after their data is written out.
    size_t firstTessVertexSpan = m_ctx->m_tessSpanData.elementsWritten();
    size_t initialTriangleVertexDataSize = m_ctx->m_triangleVertexData.bytesWritten();

    // Metal requires vertex buffers to be 256-byte aligned.
    size_t tessAlignmentPadding =
        gpu::PaddingToAlignUp<gpu::kTessVertexBufferAlignmentInElements>(firstTessVertexSpan);
    assert(tessAlignmentPadding <= kMaxTessellationAlignmentVertices);
    m_ctx->m_tessSpanData.push_back_n(nullptr, tessAlignmentPadding);
    m_flushDesc.firstTessVertexSpan = firstTessVertexSpan + tessAlignmentPadding;
    assert(m_flushDesc.firstTessVertexSpan == m_ctx->m_tessSpanData.elementsWritten());

    // Write out the uniforms for this flush.
    m_ctx->m_flushUniformData.emplace_back(m_flushDesc, platformFeatures);

    // Write out the simple gradient data.
    assert(m_simpleGradients.size() == m_pendingSimpleGradientWrites.size());
    if (!m_pendingSimpleGradientWrites.empty())
    {
        m_ctx->m_simpleColorRampsData.push_back_n(m_pendingSimpleGradientWrites.data(),
                                                  m_pendingSimpleGradientWrites.size());
    }

    // Write out the vertex data for rendering complex gradients.
    assert(m_complexGradients.size() == m_pendingComplexColorRampDraws.size());
    if (!m_pendingComplexColorRampDraws.empty())
    {
        // The viewport will start at simpleGradDataHeight when rendering color ramps.
        for (uint32_t y = 0; y < m_pendingComplexColorRampDraws.size(); ++y)
        {
            const PLSGradient* gradient = m_pendingComplexColorRampDraws[y];
            const ColorInt* colors = gradient->colors();
            const float* stops = gradient->stops();
            size_t stopCount = gradient->count();

            // Push "GradientSpan" instances that will render each section of the color ramp.
            ColorInt lastColor = colors[0];
            uint32_t lastXFixed = 0;
            // "stop * w + .5" converts a stop position to an x-coordinate in the gradient texture.
            // Stops should be aligned (ideally) on pixel centers to prevent bleed.
            // Render half-pixel-wide caps at the beginning and end to ensure the boundary pixels
            // get filled.
            float w = kGradTextureWidth - 1.f;
            for (size_t i = 0; i < stopCount; ++i)
            {
                float x = stops[i] * w + .5f;
                uint32_t xFixed = static_cast<uint32_t>(x * (65536.f / kGradTextureWidth));
                assert(lastXFixed <= xFixed && xFixed < 65536); // stops[] must be ordered.
                m_ctx->m_gradSpanData.set_back(lastXFixed, xFixed, y, lastColor, colors[i]);
                lastColor = colors[i];
                lastXFixed = xFixed;
            }
            m_ctx->m_gradSpanData.set_back(lastXFixed, 65535u, y, lastColor, lastColor);
        }
    }

    // Write a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    gpu::SimplePaintValue clearColorValue;
    clearColorValue.color = m_ctx->frameDescriptor().clearColor;
    m_ctx->m_pathData.skip_back();
    m_ctx->m_paintData.set_back(FillRule::nonZero,
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
        pushPaddingVertices(0, gpu::kMidpointFanPatchSegmentSpan);
        // Padding between patch types in the tessellation texture.
        if (m_outerCubicTessVertexIdx > m_midpointFanTessEndLocation)
        {
            pushPaddingVertices(m_midpointFanTessEndLocation,
                                m_outerCubicTessVertexIdx - m_midpointFanTessEndLocation);
        }
        // The final vertex of the final patch of each contour crosses over into the next contour.
        // (This is how we wrap around back to the beginning.) Therefore, the final contour of the
        // flush needs an out-of-contour vertex to cross into as well, so we emit a padding vertex
        // here at the end.
        pushPaddingVertices(m_outerCubicTessEndLocation, 1);
    }

    // Write out all the data for our high level draws, and build up a low-level draw list.
    if (m_ctx->frameInterlockMode() == gpu::InterlockMode::rasterOrdering)
    {
        for (const PLSDrawUniquePtr& draw : m_plsDraws)
        {
            draw->pushToRenderContext(this);
        }
    }
    else
    {
        assert(m_plsDraws.size() <= kMaxReorderedDrawCount);

        // Sort the draw list to optimize batching, since we can only batch non-overlapping draws.
        std::vector<int64_t>& indirectDrawList = m_ctx->m_indirectDrawList;
        indirectDrawList.resize(m_plsDraws.size());

        if (m_ctx->m_intersectionBoard == nullptr)
        {
            m_ctx->m_intersectionBoard = std::make_unique<IntersectionBoard>();
        }
        IntersectionBoard* intersectionBoard = m_ctx->m_intersectionBoard.get();
        intersectionBoard->resizeAndReset(m_flushDesc.renderTarget->width(),
                                          m_flushDesc.renderTarget->height());

        // Build a list of sort keys that determine the final draw order.
        constexpr static int kDrawGroupShift = 48; // Where in the key does the draw group begin?
        constexpr static int64_t kDrawGroupMask = 0xffffllu << kDrawGroupShift;
        constexpr static int kDrawTypeShift = 45;
        constexpr static int64_t kDrawTypeMask RIVE_MAYBE_UNUSED = 7llu << kDrawTypeShift;
        constexpr static int kTextureHashShift = 26;
        constexpr static int64_t kTextureHashMask = 0x7ffffllu << kTextureHashShift;
        constexpr static int kBlendModeShift = 22;
        constexpr static int kBlendModeMask = 0xf << kBlendModeShift;
        constexpr static int kDrawContentsShift = 16;
        constexpr static int64_t kDrawContentsMask = 0x3fllu << kDrawContentsShift;
        constexpr static int64_t kDrawIndexMask = 0xffff;
        for (size_t i = 0; i < m_plsDraws.size(); ++i)
        {
            PLSDraw* draw = m_plsDraws[i].get();

            int4 drawBounds = simd::load4i(&m_plsDraws[i]->pixelBounds());

            // Add one extra pixel of padding to the draw bounds to make absolutely certain we get
            // no overlapping pixels, which destroy the atomic shader.
            const int32_t kMax32i = std::numeric_limits<int32_t>::max();
            const int32_t kMin32i = std::numeric_limits<int32_t>::min();
            drawBounds = simd::if_then_else(drawBounds != int4{kMin32i, kMin32i, kMax32i, kMax32i},
                                            drawBounds + int4{-1, -1, 1, 1},
                                            drawBounds);

            // Our top priority in re-ordering is to group non-overlapping draws together, in order
            // to maximize batching while preserving correctness.
            int64_t drawGroupIdx = intersectionBoard->addRectangle(drawBounds);
            assert(drawGroupIdx > 0);
            if (m_flushDesc.interlockMode == gpu::InterlockMode::depthStencil && draw->isOpaque())
            {
                // In depthStencil mode we can reverse-sort opaque paths front to back, draw them
                // first, and take advantage of early Z culling.
                //
                // To keep things simple initially, we don't reverse-sort draws that use clipping.
                // (Otherwise if a clip affects both opaque and transparent content, we would have
                // to apply it twice.)
                bool usesClipping = draw->drawContents() &
                                    (gpu::DrawContents::activeClip | gpu::DrawContents::clipUpdate);
                if (!usesClipping)
                {
                    drawGroupIdx = -drawGroupIdx;
                }
            }
            int64_t key = drawGroupIdx << kDrawGroupShift;

            // Within sub-groups of non-overlapping draws, sort similar draw types together.
            int64_t drawType = static_cast<int64_t>(draw->type());
            assert(drawType <= kDrawTypeMask >> kDrawTypeShift);
            key |= drawType << kDrawTypeShift;

            // Within sub-groups of matching draw type, sort by texture binding.
            int64_t textureHash = draw->imageTexture() != nullptr
                                      ? draw->imageTexture()->textureResourceHash() &
                                            (kTextureHashMask >> kTextureHashShift)
                                      : 0;
            key |= textureHash << kTextureHashShift;

            // If using KHR_blend_equation_advanced, we need a batching barrier between draws with
            // different blend modes.
            // If not using KHR_blend_equation_advanced, sorting by blend mode may still give us
            // better branching on the GPU.
            int64_t blendMode = gpu::ConvertBlendModeToPLSBlendMode(draw->blendMode());
            assert(blendMode <= kBlendModeMask >> kBlendModeShift);
            key |= blendMode << kBlendModeShift;

            // depthStencil mode draws strokes, fills, and even/odd with different stencil settings.
            int64_t drawContents = static_cast<int64_t>(draw->drawContents());
            assert(drawContents <= kDrawContentsMask >> kDrawContentsShift);
            key |= drawContents << kDrawContentsShift;

            // Draw index goes at the bottom of the key so we know which PLSDraw it corresponds to.
            assert(i <= kDrawIndexMask);
            key |= i;

            assert((key & kDrawGroupMask) >> kDrawGroupShift == drawGroupIdx);
            assert((key & kDrawTypeMask) >> kDrawTypeShift == drawType);
            assert((key & kTextureHashMask) >> kTextureHashShift == textureHash);
            assert((key & kBlendModeMask) >> kBlendModeShift == blendMode);
            assert((key & kDrawContentsMask) >> kDrawContentsShift == drawContents);
            assert((key & kDrawIndexMask) == i);

            indirectDrawList[i] = key;
        }

        // Re-order the draws!!
        std::sort(indirectDrawList.begin(), indirectDrawList.end());

        // Atomic mode sometimes needs to initialize PLS with a draw when the backend can't do it
        // with typical clear/load APIs.
        if (m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics &&
            platformFeatures.atomicPLSMustBeInitializedAsDraw)
        {
            m_drawList.emplace_back(m_ctx->perFrameAllocator(),
                                    DrawType::gpuAtomicInitialize,
                                    nullptr,
                                    1,
                                    0);
            pushBarrier();
        }

        // Draws with the same drawGroupIdx don't overlap, but once we cross into a new draw group,
        // we need to insert a barrier between the overlaps.
        int64_t needsBarrierMask = kDrawGroupMask;
        if (m_flushDesc.interlockMode == gpu::InterlockMode::depthStencil)
        {
            // depthStencil mode also draws clips, strokes, fills, and even/odd with different
            // stencil settings, so these also need a barrier.
            needsBarrierMask |= kDrawContentsMask;
            if (platformFeatures.supportsKHRBlendEquations)
            {
                // If using KHR_blend_equation_advanced, we also need a barrier between blend modes
                // in order to change the blend equation.
                needsBarrierMask |= kBlendModeMask;
            }
        }

        // Write out the draw data from the sorted draw list, and build up a condensed/batched list
        // of low-level draws.
        int64_t priorKey = !indirectDrawList.empty() ? indirectDrawList[0] : 0;
        for (int64_t key : indirectDrawList)
        {
            if ((priorKey & needsBarrierMask) != (key & needsBarrierMask))
            {
                pushBarrier();
            }
            // We negate drawGroupIdx on opaque paths in order to draw them first and in reverse
            // order, but their z index should still remain positive.
            m_currentZIndex = math::lossless_numeric_cast<uint32_t>(
                abs(key >> static_cast<int64_t>(kDrawGroupShift)));
            m_plsDraws[key & kDrawIndexMask]->pushToRenderContext(this);
            priorKey = key;
        }

        // Atomic mode needs one more draw to resolve all the pixels.
        if (m_ctx->frameInterlockMode() == gpu::InterlockMode::atomics)
        {
            pushBarrier();
            m_drawList.emplace_back(m_ctx->perFrameAllocator(),
                                    DrawType::gpuAtomicResolve,
                                    nullptr,
                                    1,
                                    0);
            m_drawList.tail().shaderFeatures = m_combinedShaderFeatures;
        }
    }

    // Pad our buffers to 256-byte alignment.
    m_ctx->m_pathData.push_back_n(nullptr, m_pathPaddingCount);
    m_ctx->m_paintData.push_back_n(nullptr, m_paintPaddingCount);
    m_ctx->m_paintAuxData.push_back_n(nullptr, m_paintAuxPaddingCount);
    m_ctx->m_contourData.push_back_n(nullptr, m_contourPaddingCount);
    m_ctx->m_gradSpanData.push_back_n(nullptr, m_gradSpanPaddingCount);

    assert(m_ctx->m_pathData.elementsWritten() ==
           m_flushDesc.firstPath + m_resourceCounts.pathCount + m_pathPaddingCount);
    assert(m_ctx->m_paintData.elementsWritten() ==
           m_flushDesc.firstPaint + m_resourceCounts.pathCount + m_paintPaddingCount);
    assert(m_ctx->m_paintAuxData.elementsWritten() ==
           m_flushDesc.firstPaintAux + m_resourceCounts.pathCount + m_paintAuxPaddingCount);
    assert(m_ctx->m_contourData.elementsWritten() ==
           m_flushDesc.firstContour + m_resourceCounts.contourCount + m_contourPaddingCount);
    assert(m_ctx->m_gradSpanData.elementsWritten() ==
           m_flushDesc.firstComplexGradSpan + m_resourceCounts.complexGradientSpanCount +
               m_gradSpanPaddingCount);

    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);
    assert(m_midpointFanTessVertexIdx == m_midpointFanTessEndLocation);
    assert(m_outerCubicTessVertexIdx == m_outerCubicTessEndLocation);

    // Update the flush descriptor's data counts that aren't known until it's written out.
    m_flushDesc.tessVertexSpanCount = math::lossless_numeric_cast<uint32_t>(
        m_ctx->m_tessSpanData.elementsWritten() - m_flushDesc.firstTessVertexSpan);
    m_flushDesc.hasTriangleVertices =
        m_ctx->m_triangleVertexData.bytesWritten() != initialTriangleVertexDataSize;

    m_flushDesc.drawList = &m_drawList;
    m_flushDesc.combinedShaderFeatures = m_combinedShaderFeatures;
}

void PLSRenderContext::setResourceSizes(ResourceAllocationCounts allocs, bool forceRealloc)
{
#if 0
    class Logger
    {
    public:
        void logSize(const char* name, size_t oldSize, size_t newSize, size_t newSizeInBytes)
        {
            m_totalSizeInBytes += newSizeInBytes;
            if (oldSize == newSize)
            {
                return;
            }
            if (!m_hasChanged)
            {
                printf("PLSRenderContext::setResourceSizes():\n");
                m_hasChanged = true;
            }
            printf("  resize %s: %zu -> %zu (%zu KiB)\n",
                   name,
                   oldSize,
                   newSize,
                   newSizeInBytes >> 10);
        }

        ~Logger()
        {
            if (!m_hasChanged)
            {
                return;
            }
            printf("  TOTAL GPU resource usage: %zu KiB\n", m_totalSizeInBytes >> 10);
        }

    private:
        size_t m_totalSizeInBytes = 0;
        bool m_hasChanged = false;
    } logger;
#define LOG_BUFFER_RING_SIZE(NAME, ITEM_SIZE_IN_BYTES)                                             \
    logger.logSize(#NAME,                                                                          \
                   m_currentResourceAllocations.NAME,                                              \
                   allocs.NAME,                                                                    \
                   allocs.NAME* ITEM_SIZE_IN_BYTES* gpu::kBufferRingSize)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)                                                    \
    logger.logSize(#NAME,                                                                          \
                   m_currentResourceAllocations.NAME,                                              \
                   allocs.NAME,                                                                    \
                   allocs.NAME* BYTES_PER_ROW)
#else
#define LOG_BUFFER_RING_SIZE(NAME, ITEM_SIZE_IN_BYTES)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)
#endif

    LOG_BUFFER_RING_SIZE(flushUniformBufferCount, sizeof(gpu::FlushUniforms));
    if (allocs.flushUniformBufferCount != m_currentResourceAllocations.flushUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeFlushUniformBuffer(allocs.flushUniformBufferCount *
                                         sizeof(gpu::FlushUniforms));
    }

    LOG_BUFFER_RING_SIZE(imageDrawUniformBufferCount, sizeof(gpu::ImageDrawUniforms));
    if (allocs.imageDrawUniformBufferCount !=
            m_currentResourceAllocations.imageDrawUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeImageDrawUniformBuffer(allocs.imageDrawUniformBufferCount *
                                             sizeof(gpu::ImageDrawUniforms));
    }

    LOG_BUFFER_RING_SIZE(pathBufferCount, sizeof(gpu::PathData));
    if (allocs.pathBufferCount != m_currentResourceAllocations.pathBufferCount || forceRealloc)
    {
        m_impl->resizePathBuffer(allocs.pathBufferCount * sizeof(gpu::PathData),
                                 gpu::PathData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintBufferCount, sizeof(gpu::PaintData));
    if (allocs.paintBufferCount != m_currentResourceAllocations.paintBufferCount || forceRealloc)
    {
        m_impl->resizePaintBuffer(allocs.paintBufferCount * sizeof(gpu::PaintData),
                                  gpu::PaintData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintAuxBufferCount, sizeof(gpu::PaintAuxData));
    if (allocs.paintAuxBufferCount != m_currentResourceAllocations.paintAuxBufferCount ||
        forceRealloc)
    {
        m_impl->resizePaintAuxBuffer(allocs.paintAuxBufferCount * sizeof(gpu::PaintAuxData),
                                     gpu::PaintAuxData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(contourBufferCount, sizeof(gpu::ContourData));
    if (allocs.contourBufferCount != m_currentResourceAllocations.contourBufferCount ||
        forceRealloc)
    {
        m_impl->resizeContourBuffer(allocs.contourBufferCount * sizeof(gpu::ContourData),
                                    gpu::ContourData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(simpleGradientBufferCount, sizeof(gpu::TwoTexelRamp));
    if (allocs.simpleGradientBufferCount !=
            m_currentResourceAllocations.simpleGradientBufferCount ||
        forceRealloc)
    {
        m_impl->resizeSimpleColorRampsBuffer(allocs.simpleGradientBufferCount *
                                             sizeof(gpu::TwoTexelRamp));
    }

    LOG_BUFFER_RING_SIZE(complexGradSpanBufferCount, sizeof(gpu::GradientSpan));
    if (allocs.complexGradSpanBufferCount !=
            m_currentResourceAllocations.complexGradSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeGradSpanBuffer(allocs.complexGradSpanBufferCount * sizeof(gpu::GradientSpan));
    }

    LOG_BUFFER_RING_SIZE(tessSpanBufferCount, sizeof(gpu::TessVertexSpan));
    if (allocs.tessSpanBufferCount != m_currentResourceAllocations.tessSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTessVertexSpanBuffer(allocs.tessSpanBufferCount *
                                           sizeof(gpu::TessVertexSpan));
    }

    LOG_BUFFER_RING_SIZE(triangleVertexBufferCount, sizeof(gpu::TriangleVertex));
    if (allocs.triangleVertexBufferCount !=
            m_currentResourceAllocations.triangleVertexBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTriangleVertexBuffer(allocs.triangleVertexBufferCount *
                                           sizeof(gpu::TriangleVertex));
    }

    allocs.gradTextureHeight = std::min<size_t>(allocs.gradTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(gradTextureHeight, gpu::kGradTextureWidth * 4);
    if (allocs.gradTextureHeight != m_currentResourceAllocations.gradTextureHeight || forceRealloc)
    {
        m_impl->resizeGradientTexture(
            gpu::kGradTextureWidth,
            math::lossless_numeric_cast<uint32_t>(allocs.gradTextureHeight));
    }

    allocs.tessTextureHeight = std::min<size_t>(allocs.tessTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(tessTextureHeight, gpu::kTessTextureWidth * 4 * 4);
    if (allocs.tessTextureHeight != m_currentResourceAllocations.tessTextureHeight || forceRealloc)
    {
        m_impl->resizeTessellationTexture(
            gpu::kTessTextureWidth,
            math::lossless_numeric_cast<uint32_t>(allocs.tessTextureHeight));
    }

    m_currentResourceAllocations = allocs;
}

void PLSRenderContext::mapResourceBuffers(const ResourceAllocationCounts& mapCounts)
{
    m_impl->prepareToMapBuffers();

    if (mapCounts.flushUniformBufferCount > 0)
    {
        m_flushUniformData.mapElements(m_impl.get(),
                                       &PLSRenderContextImpl::mapFlushUniformBuffer,
                                       mapCounts.flushUniformBufferCount);
    }
    assert(m_flushUniformData.hasRoomFor(mapCounts.flushUniformBufferCount));

    if (mapCounts.imageDrawUniformBufferCount > 0)
    {
        m_imageDrawUniformData.mapElements(m_impl.get(),
                                           &PLSRenderContextImpl::mapImageDrawUniformBuffer,
                                           mapCounts.imageDrawUniformBufferCount);
    }
    assert(m_imageDrawUniformData.hasRoomFor(mapCounts.imageDrawUniformBufferCount > 0));

    if (mapCounts.pathBufferCount > 0)
    {
        m_pathData.mapElements(m_impl.get(),
                               &PLSRenderContextImpl::mapPathBuffer,
                               mapCounts.pathBufferCount);
    }
    assert(m_pathData.hasRoomFor(mapCounts.pathBufferCount));

    if (mapCounts.paintBufferCount > 0)
    {
        m_paintData.mapElements(m_impl.get(),
                                &PLSRenderContextImpl::mapPaintBuffer,
                                mapCounts.paintBufferCount);
    }
    assert(m_paintData.hasRoomFor(mapCounts.paintBufferCount));

    if (mapCounts.paintAuxBufferCount > 0)
    {
        m_paintAuxData.mapElements(m_impl.get(),
                                   &PLSRenderContextImpl::mapPaintAuxBuffer,
                                   mapCounts.paintAuxBufferCount);
    }
    assert(m_paintAuxData.hasRoomFor(mapCounts.paintAuxBufferCount));

    if (mapCounts.contourBufferCount > 0)
    {
        m_contourData.mapElements(m_impl.get(),
                                  &PLSRenderContextImpl::mapContourBuffer,
                                  mapCounts.contourBufferCount);
    }
    assert(m_contourData.hasRoomFor(mapCounts.contourBufferCount));

    if (mapCounts.simpleGradientBufferCount > 0)
    {
        m_simpleColorRampsData.mapElements(m_impl.get(),
                                           &PLSRenderContextImpl::mapSimpleColorRampsBuffer,
                                           mapCounts.simpleGradientBufferCount);
    }
    assert(m_simpleColorRampsData.hasRoomFor(mapCounts.simpleGradientBufferCount));

    if (mapCounts.complexGradSpanBufferCount > 0)
    {
        m_gradSpanData.mapElements(m_impl.get(),
                                   &PLSRenderContextImpl::mapGradSpanBuffer,
                                   mapCounts.complexGradSpanBufferCount);
    }
    assert(m_gradSpanData.hasRoomFor(mapCounts.complexGradSpanBufferCount));

    if (mapCounts.tessSpanBufferCount > 0)
    {
        m_tessSpanData.mapElements(m_impl.get(),
                                   &PLSRenderContextImpl::mapTessVertexSpanBuffer,
                                   mapCounts.tessSpanBufferCount);
    }
    assert(m_tessSpanData.hasRoomFor(mapCounts.tessSpanBufferCount));

    if (mapCounts.triangleVertexBufferCount > 0)
    {
        m_triangleVertexData.mapElements(m_impl.get(),
                                         &PLSRenderContextImpl::mapTriangleVertexBuffer,
                                         mapCounts.triangleVertexBufferCount);
    }
    assert(m_triangleVertexData.hasRoomFor(mapCounts.triangleVertexBufferCount));
}

void PLSRenderContext::unmapResourceBuffers()
{
    if (m_flushUniformData)
    {
        m_impl->unmapFlushUniformBuffer();
        m_flushUniformData.reset();
    }
    if (m_imageDrawUniformData)
    {
        m_impl->unmapImageDrawUniformBuffer();
        m_imageDrawUniformData.reset();
    }
    if (m_pathData)
    {
        m_impl->unmapPathBuffer();
        m_pathData.reset();
    }
    if (m_paintData)
    {
        m_impl->unmapPaintBuffer();
        m_paintData.reset();
    }
    if (m_paintAuxData)
    {
        m_impl->unmapPaintAuxBuffer();
        m_paintAuxData.reset();
    }
    if (m_contourData)
    {
        m_impl->unmapContourBuffer();
        m_contourData.reset();
    }
    if (m_simpleColorRampsData)
    {
        m_impl->unmapSimpleColorRampsBuffer();
        m_simpleColorRampsData.reset();
    }
    if (m_gradSpanData)
    {
        m_impl->unmapGradSpanBuffer();
        m_gradSpanData.reset();
    }
    if (m_tessSpanData)
    {
        m_impl->unmapTessVertexSpanBuffer();
        m_tessSpanData.reset();
    }
    if (m_triangleVertexData)
    {
        m_impl->unmapTriangleVertexBuffer();
        m_triangleVertexData.reset();
    }
}

void PLSRenderContext::LogicalFlush::pushPaddingVertices(uint32_t tessLocation, uint32_t count)
{
    assert(m_hasDoneLayout);
    assert(count > 0);

    constexpr static Vec2D kEmptyCubic[4]{};
    // This is guaranteed to not collide with a neighboring contour ID.
    constexpr static uint32_t kInvalidContourID = 0;
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);
    m_pathTessLocation = tessLocation;
    RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = m_pathTessLocation + count;)
    assert(m_expectedPathTessLocationAtEndOfPath <= kMaxTessellationVertexCount);
    pushTessellationSpans(kEmptyCubic, {0, 0}, count, 0, 0, 1, kInvalidContourID);
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
}

void PLSRenderContext::LogicalFlush::pushPath(RiveRenderPathDraw* draw,
                                              gpu::PatchType patchType,
                                              uint32_t tessVertexCount)
{
    assert(m_hasDoneLayout);
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);

    m_currentPathIsStroked = draw->strokeRadius() != 0;
    m_currentPathContourDirections = draw->contourDirections();
    ++m_currentPathID;
    assert(0 < m_currentPathID && m_currentPathID <= m_ctx->m_maxPathID);

    m_ctx->m_pathData.set_back(draw->matrix(), draw->strokeRadius(), m_currentZIndex);
    m_ctx->m_paintData.set_back(draw->fillRule(),
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

    assert(m_flushDesc.firstPath + m_currentPathID + 1 == m_ctx->m_pathData.elementsWritten());
    assert(m_flushDesc.firstPaint + m_currentPathID + 1 == m_ctx->m_paintData.elementsWritten());
    assert(m_flushDesc.firstPaintAux + m_currentPathID + 1 ==
           m_ctx->m_paintAuxData.elementsWritten());

    gpu::DrawType drawType;
    uint32_t tessLocation;
    if (patchType == PatchType::midpointFan)
    {
        drawType = DrawType::midpointFanPatches;
        tessLocation = m_midpointFanTessVertexIdx;
        m_midpointFanTessVertexIdx += tessVertexCount;
    }
    else
    {
        drawType = DrawType::outerCurvePatches;
        tessLocation = m_outerCubicTessVertexIdx;
        m_outerCubicTessVertexIdx += tessVertexCount;
    }

    RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = tessLocation + tessVertexCount);
    RIVE_DEBUG_CODE(m_expectedPathMirroredTessLocationAtEndOfPath = tessLocation);
    assert(m_expectedPathTessLocationAtEndOfPath <= kMaxTessellationVertexCount);

    uint32_t patchSize = PatchSegmentSpan(drawType);
    uint32_t baseInstance = math::lossless_numeric_cast<uint32_t>(tessLocation / patchSize);
    assert(baseInstance * patchSize == tessLocation); // flush() is responsible for alignment.

    if (m_currentPathContourDirections == gpu::ContourDirections::reverseAndForward)
    {
        assert(tessVertexCount % 2 == 0);
        m_pathTessLocation = m_pathMirroredTessLocation = tessLocation + tessVertexCount / 2;
    }
    else if (m_currentPathContourDirections == gpu::ContourDirections::forward)
    {
        m_pathTessLocation = m_pathMirroredTessLocation = tessLocation;
    }
    else
    {
        assert(m_currentPathContourDirections == gpu::ContourDirections::reverse);
        m_pathTessLocation = m_pathMirroredTessLocation = tessLocation + tessVertexCount;
    }

    uint32_t instanceCount = tessVertexCount / patchSize;
    assert(instanceCount * patchSize == tessVertexCount); // flush() is responsible for alignment.
    pushPathDraw(draw, drawType, instanceCount, baseInstance);
}

void PLSRenderContext::LogicalFlush::pushContour(Vec2D midpoint,
                                                 bool closed,
                                                 uint32_t paddingVertexCount)
{
    assert(m_hasDoneLayout);
    assert(m_ctx->m_pathData.bytesWritten() > 0);
    assert(m_currentPathIsStroked || closed);
    assert(m_currentPathID != 0); // pathID can't be zero.

    if (m_currentPathIsStroked)
    {
        midpoint.x = closed ? 1 : 0;
    }
    // If the contour is closed, the shader needs a vertex to wrap back around to at the end of it.
    uint32_t vertexIndex0 = m_currentPathContourDirections & gpu::ContourDirections::forward
                                ? m_pathTessLocation
                                : m_pathMirroredTessLocation - 1;
    m_ctx->m_contourData.emplace_back(midpoint, m_currentPathID, vertexIndex0);
    ++m_currentContourID;
    assert(0 < m_currentContourID && m_currentContourID <= gpu::kMaxContourID);
    assert(m_flushDesc.firstContour + m_currentContourID == m_ctx->m_contourData.elementsWritten());

    // The first curve of the contour will be pre-padded with 'paddingVertexCount' tessellation
    // vertices, colocated at T=0. The caller must use this argument align the end of the contour on
    // a boundary of the patch size. (See gpu::PaddingToAlignUp().)
    m_currentContourPaddingVertexCount = paddingVertexCount;
}

void PLSRenderContext::LogicalFlush::pushCubic(const Vec2D pts[4],
                                               Vec2D joinTangent,
                                               uint32_t additionalContourFlags,
                                               uint32_t parametricSegmentCount,
                                               uint32_t polarSegmentCount,
                                               uint32_t joinSegmentCount)
{
    assert(m_hasDoneLayout);
    assert(0 <= parametricSegmentCount && parametricSegmentCount <= kMaxParametricSegments);
    assert(0 <= polarSegmentCount && polarSegmentCount <= kMaxPolarSegments);
    assert(joinSegmentCount > 0);
    assert(m_currentContourID != 0); // contourID can't be zero.

    // Polar and parametric segments share the same beginning and ending vertices, so the merged
    // *vertex* count is equal to the sum of polar and parametric *segment* counts.
    uint32_t curveMergedVertexCount = parametricSegmentCount + polarSegmentCount;
    // -1 because the curve and join share an ending/beginning vertex.
    uint32_t totalVertexCount =
        m_currentContourPaddingVertexCount + curveMergedVertexCount + joinSegmentCount - 1;

    // Only the first curve of a contour gets padding vertices.
    m_currentContourPaddingVertexCount = 0;

    if (m_currentPathContourDirections == gpu::ContourDirections::reverseAndForward)
    {
        pushMirroredAndForwardTessellationSpans(pts,
                                                joinTangent,
                                                totalVertexCount,
                                                parametricSegmentCount,
                                                polarSegmentCount,
                                                joinSegmentCount,
                                                m_currentContourID | additionalContourFlags);
    }
    else if (m_currentPathContourDirections == gpu::ContourDirections::forward)
    {
        pushTessellationSpans(pts,
                              joinTangent,
                              totalVertexCount,
                              parametricSegmentCount,
                              polarSegmentCount,
                              joinSegmentCount,
                              m_currentContourID | additionalContourFlags);
    }
    else
    {
        assert(m_currentPathContourDirections == gpu::ContourDirections::reverse);
        pushMirroredTessellationSpans(pts,
                                      joinTangent,
                                      totalVertexCount,
                                      parametricSegmentCount,
                                      polarSegmentCount,
                                      joinSegmentCount,
                                      m_currentContourID | additionalContourFlags);
    }

    RIVE_DEBUG_CODE(++m_pathCurveCount;)
}

RIVE_ALWAYS_INLINE void PLSRenderContext::LogicalFlush::pushTessellationSpans(
    const Vec2D pts[4],
    Vec2D joinTangent,
    uint32_t totalVertexCount,
    uint32_t parametricSegmentCount,
    uint32_t polarSegmentCount,
    uint32_t joinSegmentCount,
    uint32_t contourIDWithFlags)
{
    assert(m_hasDoneLayout);
    assert(totalVertexCount > 0);

    uint32_t y = m_pathTessLocation / kTessTextureWidth;
    int32_t x0 = m_pathTessLocation % kTessTextureWidth;
    int32_t x1 = x0 + totalVertexCount;
    for (;;)
    {
        m_ctx->m_tessSpanData.set_back(pts,
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
            // The span was too long to fit on the current line. Wrap and draw it again, this
            // time behind the left edge of the texture so we capture what got clipped off last
            // time.
            ++y;
            x0 -= kTessTextureWidth;
            x1 -= kTessTextureWidth;
            continue;
        }
        break;
    }
    assert(y == (m_pathTessLocation + totalVertexCount - 1) / kTessTextureWidth);

    m_pathTessLocation += totalVertexCount;
    assert(m_pathTessLocation <= m_expectedPathTessLocationAtEndOfPath);
}

RIVE_ALWAYS_INLINE void PLSRenderContext::LogicalFlush::pushMirroredTessellationSpans(
    const Vec2D pts[4],
    Vec2D joinTangent,
    uint32_t totalVertexCount,
    uint32_t parametricSegmentCount,
    uint32_t polarSegmentCount,
    uint32_t joinSegmentCount,
    uint32_t contourIDWithFlags)
{
    assert(m_hasDoneLayout);
    assert(totalVertexCount > 0);

    uint32_t reflectionY = (m_pathMirroredTessLocation - 1) / kTessTextureWidth;
    int32_t reflectionX0 = (m_pathMirroredTessLocation - 1) % kTessTextureWidth + 1;
    int32_t reflectionX1 = reflectionX0 - totalVertexCount;

    for (;;)
    {
        m_ctx->m_tessSpanData.set_back(pts,
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
    assert(m_pathMirroredTessLocation >= m_expectedPathMirroredTessLocationAtEndOfPath);
}

RIVE_ALWAYS_INLINE void PLSRenderContext::LogicalFlush::pushMirroredAndForwardTessellationSpans(
    const Vec2D pts[4],
    Vec2D joinTangent,
    uint32_t totalVertexCount,
    uint32_t parametricSegmentCount,
    uint32_t polarSegmentCount,
    uint32_t joinSegmentCount,
    uint32_t contourIDWithFlags)
{
    assert(m_hasDoneLayout);
    assert(totalVertexCount > 0);

    int32_t y = m_pathTessLocation / kTessTextureWidth;
    int32_t x0 = m_pathTessLocation % kTessTextureWidth;
    int32_t x1 = x0 + totalVertexCount;

    uint32_t reflectionY = (m_pathMirroredTessLocation - 1) / kTessTextureWidth;
    int32_t reflectionX0 = (m_pathMirroredTessLocation - 1) % kTessTextureWidth + 1;
    int32_t reflectionX1 = reflectionX0 - totalVertexCount;

    for (;;)
    {
        m_ctx->m_tessSpanData.set_back(pts,
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
            // Either the span or its reflection was too long to fit on the current line. Wrap and
            // draw both of them again, this time beyond the opposite edge of the texture so we
            // capture what got clipped off last time.
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
    assert(m_pathTessLocation <= m_expectedPathTessLocationAtEndOfPath);

    m_pathMirroredTessLocation -= totalVertexCount;
    assert(m_pathMirroredTessLocation >= m_expectedPathMirroredTessLocationAtEndOfPath);
}

void PLSRenderContext::LogicalFlush::pushInteriorTriangulation(InteriorTriangulationDraw* draw)
{
    assert(m_hasDoneLayout);

    assert(m_ctx->m_triangleVertexData.hasRoomFor(draw->triangulator()->maxVertexCount()));
    uint32_t baseVertex =
        math::lossless_numeric_cast<uint32_t>(m_ctx->m_triangleVertexData.elementsWritten());
    size_t actualVertexCount =
        draw->triangulator()->polysToTriangles(&m_ctx->m_triangleVertexData, m_currentPathID);
    assert(actualVertexCount <= draw->triangulator()->maxVertexCount());
    DrawBatch& batch = pushPathDraw(draw,
                                    DrawType::interiorTriangulation,
                                    math::lossless_numeric_cast<uint32_t>(actualVertexCount),
                                    baseVertex);
    // Interior triangulations are allowed to disable raster ordering since they are guaranteed to
    // not overlap.
    batch.needsBarrier = true;
}

void PLSRenderContext::LogicalFlush::pushImageRect(ImageRectDraw* draw)
{
    assert(m_hasDoneLayout);

    // If we support image paints for paths, the client should use pushPath() with an image paint
    // instead of calling this method.
    assert(!m_ctx->frameSupportsImagePaintForPaths());

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(draw->matrix(),
                                               draw->opacity(),
                                               draw->clipRectInverseMatrix(),
                                               draw->clipID(),
                                               draw->blendMode(),
                                               m_currentZIndex);

    DrawBatch& batch = pushDraw(draw, DrawType::imageRect, PaintType::image, 1, 0);
    batch.imageDrawDataOffset = math::lossless_numeric_cast<uint32_t>(imageDrawDataOffset);
}

void PLSRenderContext::LogicalFlush::pushImageMesh(ImageMeshDraw* draw)
{

    assert(m_hasDoneLayout);

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(draw->matrix(),
                                               draw->opacity(),
                                               draw->clipRectInverseMatrix(),
                                               draw->clipID(),
                                               draw->blendMode(),
                                               m_currentZIndex);

    DrawBatch& batch = pushDraw(draw, DrawType::imageMesh, PaintType::image, draw->indexCount(), 0);
    batch.vertexBuffer = draw->vertexBuffer();
    batch.uvBuffer = draw->uvBuffer();
    batch.indexBuffer = draw->indexBuffer();
    batch.imageDrawDataOffset = math::lossless_numeric_cast<uint32_t>(imageDrawDataOffset);
}

void PLSRenderContext::LogicalFlush::pushStencilClipReset(StencilClipReset* draw)
{
    assert(m_hasDoneLayout);

    uint32_t baseVertex =
        math::lossless_numeric_cast<uint32_t>(m_ctx->m_triangleVertexData.elementsWritten());
    auto [L, T, R, B] = AABB(getClipInfo(draw->previousClipID()).contentBounds);
    uint32_t Z = m_currentZIndex;
    assert(AABB(L, T, R, B).round() == draw->pixelBounds());
    assert(draw->resourceCounts().maxTriangleVertexCount == 6);
    assert(m_ctx->m_triangleVertexData.hasRoomFor(6));
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{L, B}, 0, Z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{L, T}, 0, Z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{R, B}, 0, Z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{R, B}, 0, Z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{L, T}, 0, Z);
    m_ctx->m_triangleVertexData.emplace_back(Vec2D{R, T}, 0, Z);
    pushDraw(draw, DrawType::stencilClipReset, PaintType::clipUpdate, 6, baseVertex);
}

void PLSRenderContext::LogicalFlush::pushBarrier()
{
    assert(m_hasDoneLayout);
    assert(m_flushDesc.interlockMode != gpu::InterlockMode::rasterOrdering);

    if (!m_drawList.empty())
    {
        m_drawList.tail().needsBarrier = true;
    }
}

gpu::DrawBatch& PLSRenderContext::LogicalFlush::pushPathDraw(RiveRenderPathDraw* draw,
                                                             DrawType drawType,
                                                             uint32_t vertexCount,
                                                             uint32_t baseVertex)
{
    assert(m_hasDoneLayout);

    DrawBatch& batch = pushDraw(draw, drawType, draw->paintType(), vertexCount, baseVertex);
    auto pathShaderFeatures = gpu::ShaderFeatures::NONE;
    if (draw->fillRule() == FillRule::evenOdd)
    {
        pathShaderFeatures |= ShaderFeatures::ENABLE_EVEN_ODD;
    }
    if (draw->paintType() == PaintType::clipUpdate && draw->simplePaintValue().outerClipID != 0)
    {
        pathShaderFeatures |= ShaderFeatures::ENABLE_NESTED_CLIPPING;
    }
    batch.shaderFeatures |= pathShaderFeatures & m_ctx->m_frameShaderFeaturesMask;
    m_combinedShaderFeatures |= batch.shaderFeatures;
    assert((batch.shaderFeatures &
            gpu::ShaderFeaturesMaskFor(drawType, m_ctx->frameInterlockMode())) ==
           batch.shaderFeatures);
    return batch;
}

RIVE_ALWAYS_INLINE static bool can_combine_draw_images(const PLSTexture* currentDrawTexture,
                                                       const PLSTexture* nextDrawTexture)
{
    if (currentDrawTexture == nullptr || nextDrawTexture == nullptr)
    {
        // We can always combine two draws if one or both do not use an image paint.
        return true;
    }
    // Since the image paint's texture must be bound to a specific slot, we can't combine draws that
    // use different textures.
    return currentDrawTexture == nextDrawTexture;
}

gpu::DrawBatch& PLSRenderContext::LogicalFlush::pushDraw(PLSDraw* draw,
                                                         DrawType drawType,
                                                         gpu::PaintType paintType,
                                                         uint32_t elementCount,
                                                         uint32_t baseElement)
{
    assert(m_hasDoneLayout);

    bool needsNewBatch;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::outerCurvePatches:
        case DrawType::gpuAtomicInitialize:
        case DrawType::gpuAtomicResolve:
        case DrawType::stencilClipReset:
            needsNewBatch =
                m_drawList.empty() || m_drawList.tail().drawType != drawType ||
                m_drawList.tail().needsBarrier ||
                !can_combine_draw_images(m_drawList.tail().imageTexture, draw->imageTexture());
            break;
        case DrawType::interiorTriangulation:
        case DrawType::imageRect:
        case DrawType::imageMesh:
            // We can't combine interior triangulations or image draws yet.
            needsNewBatch = true;
            break;
    }

    DrawBatch& batch = needsNewBatch ? m_drawList.emplace_back(m_ctx->perFrameAllocator(),
                                                               drawType,
                                                               draw,
                                                               elementCount,
                                                               baseElement)
                                     : m_drawList.tail();
    if (!needsNewBatch)
    {
        assert(batch.drawType == drawType);
        assert(can_combine_draw_images(batch.imageTexture, draw->imageTexture()));
        assert(!batch.needsBarrier);
        if (m_flushDesc.interlockMode == gpu::InterlockMode::depthStencil)
        {
            // depthStencil can't mix drawContents in a batch.
            assert(batch.drawContents == draw->drawContents());
            assert((batch.shaderFeatures & gpu::ShaderFeatures::ENABLE_ADVANCED_BLEND) ==
                   (draw->blendMode() != BlendMode::srcOver));
            // If using KHR_blend_equation_advanced, we can't mix blend modes in a batch.
            assert(!m_ctx->platformFeatures().supportsKHRBlendEquations ||
                   batch.internalDrawList->blendMode() == draw->blendMode());
        }
        assert(batch.baseElement + batch.elementCount == baseElement);
        draw->setBatchInternalNeighbor(batch.internalDrawList);
        batch.internalDrawList = draw;
        batch.elementCount += elementCount;
    }

    if (paintType == PaintType::image)
    {
        assert(draw->imageTexture() != nullptr);
        if (batch.imageTexture == nullptr)
        {
            batch.imageTexture = draw->imageTexture();
        }
        assert(batch.imageTexture == draw->imageTexture());
    }

    auto shaderFeatures = ShaderFeatures::NONE;
    if (draw->clipID() != 0)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIPPING;
    }
    if (draw->hasClipRect() && paintType != PaintType::clipUpdate)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIP_RECT;
    }
    if (paintType != PaintType::clipUpdate)
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
    batch.shaderFeatures |= shaderFeatures & m_ctx->m_frameShaderFeaturesMask;
    m_combinedShaderFeatures |= batch.shaderFeatures;
    batch.drawContents |= draw->drawContents();
    assert((batch.shaderFeatures &
            gpu::ShaderFeaturesMaskFor(drawType, m_ctx->frameInterlockMode())) ==
           batch.shaderFeatures);
    return batch;
}
} // namespace rive::gpu
