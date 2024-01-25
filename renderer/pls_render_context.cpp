/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_render_context.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "intersection_board.hpp"
#include "pls_paint.hpp"
#include "rive/pls/pls_draw.hpp"
#include "rive/pls/pls_image.hpp"
#include "rive/pls/pls_render_context_impl.hpp"
#include "shaders/constants.glsl"

#include <string_view>

namespace rive::pls
{
constexpr size_t kDefaultSimpleGradientCapacity = 512;
constexpr size_t kDefaultComplexGradientCapacity = 1024;
constexpr size_t kDefaultDrawCapacity = 2048;

constexpr size_t kMaxTextureHeight = 2048; // TODO: Move this variable to PlatformFeatures.
constexpr size_t kMaxTessellationVertexCount = kMaxTextureHeight * kTessTextureWidth;
constexpr size_t kMaxTessellationVertexCountBeforePadding =
    kMaxTessellationVertexCount -
    pls::kMidpointFanPatchSegmentSpan -       // Padding at the beginning of the tess texture
    (pls::kMidpointFanPatchSegmentSpan - 1) - // Max padding between patch types in the tess texture
    1;                                        // Padding at the end of the tessellation texture

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
    return resource_texture_height<pls::kGradTextureWidthInSimpleRamps>(simpleRampCount) +
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
    resetContainers();
}

PLSRenderContext::~PLSRenderContext()
{
    // Always call flush() to avoid deadlock.
    assert(!m_didBeginFrame);
    // Delete the logical flushes before the block allocators let go of their allocations.
    m_logicalFlushes.clear();
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

void PLSRenderContext::resetGPUResources()
{
    assert(!m_didBeginFrame);
    setResourceSizes(ResourceAllocationCounts());
    m_maxRecentResourceRequirements = ResourceAllocationCounts();
    resetContainers();
}

void PLSRenderContext::shrinkGPUResourcesToFit()
{
    assert(!m_didBeginFrame);

    // Shrink GPU resource allocations to 125% of their maximum recent usage, and only if the recent
    // usage is 2/3 or less of the current allocation.
    ResourceAllocationCounts shrinks =
        simd::if_then_else(m_maxRecentResourceRequirements.toVec() <=
                               m_currentResourceAllocations.toVec() * size_t(2) / size_t(3),
                           m_maxRecentResourceRequirements.toVec() * size_t(5) / size_t(4),
                           m_currentResourceAllocations.toVec());
    setResourceSizes(shrinks);

    // Zero out m_maxRecentResourceRequirements for the next interval.
    m_maxRecentResourceRequirements = ResourceAllocationCounts();

    resetContainers();
}

PLSRenderContext::LogicalFlush::LogicalFlush(PLSRenderContext* parent) : m_ctx(parent) { rewind(); }

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

void PLSRenderContext::LogicalFlush::rewind()
{
    m_resourceCounts = PLSDraw::ResourceCounters();
    m_simpleGradients.clear();
    m_pendingSimpleGradientWrites.clear();
    m_complexGradients.clear();
    m_pendingComplexColorRampDraws.clear();
    m_plsDraws.clear();
    m_combinedDrawBounds = {std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::max(),
                            std::numeric_limits<int32_t>::min(),
                            std::numeric_limits<int32_t>::min()};

    m_pathPaddingCount = 0;
    m_paintPaddingCount = 0;
    m_paintAuxPaddingCount = 0;
    m_contourPaddingCount = 0;
    m_midpointFanTessEndLocation = 0;
    m_outerCubicTessEndLocation = 0;
    m_outerCubicTessVertexIdx = 0;
    m_midpointFanTessVertexIdx = 0;

    m_flushDesc = FlushDescriptor();

    m_drawList.reset();
    m_combinedShaderFeatures = pls::ShaderFeatures::NONE;

    m_currentPathIsStroked = 0;
    m_currentPathNeedsMirroredContours = 0;
    m_currentPathID = 0;
    m_currentContourID = 0;
    m_currentContourPaddingVertexCount = 0;
    m_pathTessLocation = 0;
    m_pathMirroredTessLocation = 0;
    RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(m_expectedPathMirroredTessLocationAtEndOfPath = 0;)
    RIVE_DEBUG_CODE(m_pathCurveCount = 0;)

    RIVE_DEBUG_CODE(m_hasDoneLayout = false;)
}

void PLSRenderContext::LogicalFlush::resetContainers()
{
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

void PLSRenderContext::beginFrame(FrameDescriptor&& frameDescriptor)
{
    assert(!m_didBeginFrame);
    m_frameDescriptor = std::move(frameDescriptor);
    m_impl->prepareToMapBuffers();
    if (m_logicalFlushes.empty())
    {
        m_logicalFlushes.emplace_back(new LogicalFlush(this));
    }
    RIVE_DEBUG_CODE(m_didBeginFrame = true);
}

uint32_t PLSRenderContext::generateClipID()
{
    assert(m_didBeginFrame);
    if (m_lastGeneratedClipID < m_maxPathID) // maxClipID == maxPathID.
    {
        ++m_lastGeneratedClipID;
        assert(m_clipContentID != m_lastGeneratedClipID); // Totally unexpected, but just in case.
        return m_lastGeneratedClipID;
    }
    return 0; // There are no available clip IDs. The caller should flush and try again.
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

    if (m_ctx->frameDescriptor().enableExperimentalAtomicMode &&
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
                                                      pls::ColorRampLocation* colorRampLocation)
{
    assert(!m_hasDoneLayout);

    const float* stops = gradient->stops();
    size_t stopCount = gradient->count();

    if (stopCount == 2 && stops[0] == 0)
    {
        // This is a simple gradient that can be implemented by a two-texel color ramp.
        assert(stops[1] == 1); // PLSGradient transforms the stops so that the final stop == 1.
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
            rampTexelsIdx = m_simpleGradients.size() * 2;
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

void PLSRenderContext::flush(pls::FlushType flushType)
{
    assert(m_didBeginFrame);

    ++m_flushCount;

    // Reset clipping state after every logical flush because the clip buffer is not preserved
    // between render passes.
    m_lastGeneratedClipID = 0;
    m_clipContentID = 0;

    if (flushType == pls::FlushType::logical)
    {
        // Don't issue any GPU commands between logical flushes. Instead, build up a list of flushes
        // that we will submit all at once at the end of the frame.
        m_logicalFlushes.emplace_back(new LogicalFlush(this));
        return;
    }

    // Layout this frame's resource buffers and textures.
    LogicalFlush::ResourceCounters totalFrameResourceCounts;
    LogicalFlush::LayoutCounters layoutCounts;
    for (size_t i = 0; i < m_logicalFlushes.size(); ++i)
    {
        m_logicalFlushes[i]->layoutResources(
            m_frameDescriptor,
            i,
            i + 1 == m_logicalFlushes.size() ? pls::FlushType::endOfFrame : pls::FlushType::logical,
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
        layoutCounts.simpleGradCount + pls::kGradTextureWidthInSimpleRamps - 1;
    allocs.complexGradSpanBufferCount = totalFrameResourceCounts.complexGradientSpanCount;
    allocs.tessSpanBufferCount = totalFrameResourceCounts.maxTessellatedSegmentCount;
    allocs.triangleVertexBufferCount = totalFrameResourceCounts.maxTriangleVertexCount;
    allocs.gradTextureHeight = layoutCounts.maxGradTextureHeight;
    allocs.tessTextureHeight = layoutCounts.maxTessTextureHeight;

    // Track m_maxRecentResourceRequirements so shrinkGPUResourcesToFit() can select decent sizes
    // the next time it's called.
    m_maxRecentResourceRequirements =
        simd::max(allocs.toVec(), m_maxRecentResourceRequirements.toVec());

    // If "allocs" already fits in our current allocations, then don't change them.
    // If they don't fit, overallocate resources by 25% in order to create some slack for growth.
    setResourceSizes(simd::if_then_else(allocs.toVec() <= m_currentResourceAllocations.toVec(),
                                        m_currentResourceAllocations.toVec(),
                                        allocs.toVec() * size_t(5) / size_t(4)));

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
    assert(m_gradSpanData.elementsWritten() == totalFrameResourceCounts.complexGradientSpanCount);
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
    assert(flushType == FlushType::endOfFrame);
    m_perFrameAllocator.reset();
    m_numChopsAllocator.reset();
    m_chopVerticesAllocator.reset();
    m_tangentPairsAllocator.reset();
    m_polarSegmentCountsAllocator.reset();
    m_parametricSegmentCountsAllocator.reset();

    m_frameDescriptor = FrameDescriptor();
    RIVE_DEBUG_CODE(m_didBeginFrame = false;)
}

void PLSRenderContext::LogicalFlush::layoutResources(const FrameDescriptor& frameDescriptor,
                                                     size_t flushIdx,
                                                     pls::FlushType flushType,
                                                     ResourceCounters* runningFrameResourceCounts,
                                                     LayoutCounters* runningFrameLayoutCounts)
{
    assert(!m_hasDoneLayout);

    // Reserve a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    ++m_resourceCounts.pathCount;

    // Storage buffer offsets are required to be aligned on multiples of 256, so add padding
    // elements to our storage buffers.
    m_pathPaddingCount =
        pls::PaddingToAlignUp<pls::kPathBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_paintPaddingCount =
        pls::PaddingToAlignUp<pls::kPaintBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_paintAuxPaddingCount =
        pls::PaddingToAlignUp<pls::kPaintAuxBufferAlignmentInElements>(m_resourceCounts.pathCount);
    m_contourPaddingCount = pls::PaddingToAlignUp<pls::kContourBufferAlignmentInElements>(
        m_resourceCounts.contourCount);

    size_t totalTessVertexCountWithPadding = 0;
    if ((m_resourceCounts.midpointFanTessVertexCount |
         m_resourceCounts.outerCubicTessVertexCount) != 0)
    {
        // midpointFan tessellation vertices reside at the beginning of the tessellation texture,
        // after 1 patch of padding vertices.
        constexpr uint32_t padding = pls::kMidpointFanPatchSegmentSpan;
        m_midpointFanTessVertexIdx = padding;
        m_midpointFanTessEndLocation =
            m_midpointFanTessVertexIdx + m_resourceCounts.midpointFanTessVertexCount;

        // outerCubic tessellation vertices reside after the midpointFan vertices, aligned on a
        // multiple of the outerCubic patch size.
        m_outerCubicTessVertexIdx =
            m_midpointFanTessEndLocation +
            PaddingToAlignUp<pls::kOuterCurvePatchSegmentSpan>(m_midpointFanTessEndLocation);
        m_outerCubicTessEndLocation =
            m_outerCubicTessVertexIdx + m_resourceCounts.outerCubicTessVertexCount;

        // We need one more padding vertex after all the tessellation vertices.
        totalTessVertexCountWithPadding = m_outerCubicTessEndLocation + 1;
        assert(totalTessVertexCountWithPadding <= kMaxTessellationVertexCount);
    }

    uint32_t tessDataHeight =
        resource_texture_height<kTessTextureWidth>(totalTessVertexCountWithPadding);
    if (m_resourceCounts.maxTessellatedSegmentCount != 0)
    {
        // Conservatively account for line breaks and padding in the tessellation span count.
        // Line breaks potentially introduce a new span. Count the maximum number of line breaks we
        // might encounter, which is at most one for every line in the tessellation texture,
        // excluding the bottom line.
        size_t maxSpanBreakCount = tessDataHeight - 1;
        // The tessellation texture requires 3 separate spans of padding vertices (see above and
        // below).
        constexpr size_t kPaddingSpanCount = 3;
        m_resourceCounts.maxTessellatedSegmentCount += maxSpanBreakCount + kPaddingSpanCount;
    }

    m_flushDesc.flushType = flushType;
    m_flushDesc.renderTarget = frameDescriptor.renderTarget.get();
    m_flushDesc.loadAction =
        flushIdx == 0 ? frameDescriptor.loadAction : LoadAction::preserveRenderTarget;
    m_flushDesc.clearColor = frameDescriptor.clearColor;
    if (m_flushDesc.loadAction == LoadAction::clear)
    {
        // If we're clearing then we always update the entire render target.
        m_flushDesc.updateBounds = frameDescriptor.renderTarget->bounds();
    }
    else
    {
        // When we don't clear, we only update the draw bounds.
        m_flushDesc.updateBounds =
            frameDescriptor.renderTarget->bounds().intersect(m_combinedDrawBounds);
    }
    if (m_flushDesc.updateBounds.empty())
    {
        // If this is empty it means there are no draws and no clear.
        m_flushDesc.updateBounds = {0, 0, 0, 0};
    }
    m_flushDesc.interlockMode = frameDescriptor.enableExperimentalAtomicMode
                                    ? pls::InterlockMode::experimentalAtomics
                                    : pls::InterlockMode::rasterOrdered;
    m_flushDesc.skipExplicitColorClear =
        m_flushDesc.interlockMode == pls::InterlockMode::experimentalAtomics &&
        m_flushDesc.loadAction == LoadAction::clear && colorAlpha(m_flushDesc.clearColor) == 255;

    m_flushDesc.flushUniformDataOffsetInBytes = flushIdx * sizeof(pls::FlushUniforms);
    m_flushDesc.pathCount = m_resourceCounts.pathCount;
    m_flushDesc.pathCount = m_resourceCounts.pathCount;
    m_flushDesc.firstPath =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->pathPaddingCount;
    m_flushDesc.firstPaint =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->paintPaddingCount;
    m_flushDesc.firstPaintAux =
        runningFrameResourceCounts->pathCount + runningFrameLayoutCounts->paintAuxPaddingCount;
    m_flushDesc.contourCount = m_resourceCounts.contourCount;
    m_flushDesc.firstContour =
        runningFrameResourceCounts->contourCount + runningFrameLayoutCounts->contourPaddingCount;
    m_flushDesc.complexGradSpanCount = m_resourceCounts.complexGradientSpanCount;
    m_flushDesc.firstComplexGradSpan = runningFrameResourceCounts->complexGradientSpanCount;
    m_flushDesc.simpleGradTexelsWidth =
        std::min<uint32_t>(m_simpleGradients.size(), pls::kGradTextureWidthInSimpleRamps) * 2;
    m_flushDesc.simpleGradTexelsHeight =
        resource_texture_height<pls::kGradTextureWidthInSimpleRamps>(m_simpleGradients.size());
    m_flushDesc.simpleGradDataOffsetInBytes =
        runningFrameLayoutCounts->simpleGradCount * sizeof(pls::TwoTexelRamp);
    m_flushDesc.complexGradRowsTop = m_flushDesc.simpleGradTexelsHeight;
    m_flushDesc.complexGradRowsHeight = m_complexGradients.size();
    m_flushDesc.tessDataHeight = tessDataHeight;

    m_flushDesc.wireframe = frameDescriptor.wireframe;
    m_flushDesc.backendSpecificData = frameDescriptor.backendSpecificData;

    *runningFrameResourceCounts = runningFrameResourceCounts->toVec() + m_resourceCounts.toVec();
    runningFrameLayoutCounts->pathPaddingCount += m_pathPaddingCount;
    runningFrameLayoutCounts->paintPaddingCount += m_paintPaddingCount;
    runningFrameLayoutCounts->paintAuxPaddingCount += m_paintAuxPaddingCount;
    runningFrameLayoutCounts->contourPaddingCount += m_contourPaddingCount;
    runningFrameLayoutCounts->simpleGradCount += m_simpleGradients.size();
    runningFrameLayoutCounts->maxGradTextureHeight =
        std::max(m_flushDesc.simpleGradTexelsHeight + m_flushDesc.complexGradRowsHeight,
                 runningFrameLayoutCounts->maxGradTextureHeight);
    runningFrameLayoutCounts->maxTessTextureHeight =
        std::max(m_flushDesc.tessDataHeight, runningFrameLayoutCounts->maxTessTextureHeight);

    RIVE_DEBUG_CODE(m_hasDoneLayout = true;)
}

void PLSRenderContext::LogicalFlush::writeResources()
{
    assert(m_hasDoneLayout);

    // Wait until here to layout the gradient texture because the final gradient texture height is
    // not decided until after all LogicalFlushes have run layoutResources().
    m_gradTextureLayout.inverseHeight = 1.f / m_ctx->m_currentResourceAllocations.gradTextureHeight;
    m_gradTextureLayout.complexOffsetY = m_flushDesc.complexGradRowsTop;

    // Exact tessSpan/triangleVertex counts aren't known until after their data is written out.
    m_flushDesc.firstTessVertexSpan = m_ctx->m_tessSpanData.elementsWritten();
    size_t initialTriangleVertexDataSize = m_ctx->m_triangleVertexData.bytesWritten();

    m_ctx->m_flushUniformData.emplace_back(m_complexGradients.size(),
                                           m_flushDesc.tessDataHeight,
                                           m_flushDesc.renderTarget->width(),
                                           m_flushDesc.renderTarget->height(),
                                           m_flushDesc.updateBounds,
                                           m_ctx->impl()->platformFeatures());

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
                assert(lastXFixed <= xFixed && xFixed < 65536);
                m_ctx->m_gradSpanData.set_back(lastXFixed, xFixed, y, lastColor, colors[i]);
                lastColor = colors[i];
                lastXFixed = xFixed;
            }
            m_ctx->m_gradSpanData.set_back(lastXFixed, 65535u, y, lastColor, lastColor);
        }
    }

    // Write a path record for the clearColor paint (used by atomic mode).
    // This also allows us to index the storage buffers directly by pathID.
    pls::SimplePaintValue clearColorValue;
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
        pushPaddingVertices(0, pls::kMidpointFanPatchSegmentSpan);
        // Padding between patch types in the tessellation texture.
        pushPaddingVertices(m_midpointFanTessEndLocation,
                            m_outerCubicTessVertexIdx - m_midpointFanTessEndLocation);
        // The final vertex of the final patch of each contour crosses over into the next contour.
        // (This is how we wrap around back to the beginning.) Therefore, the final contour of the
        // flush needs an out-of-contour vertex to cross into as well, so we emit a padding vertex
        // here at the end.
        pushPaddingVertices(m_outerCubicTessEndLocation, 1);
    }

    if (m_ctx->frameDescriptor().enableExperimentalAtomicMode)
    {
        assert(m_plsDraws.size() <= kMaxReorderedDrawCount);

        // Sort the draw list to optimize batching, since we can only batch non-overlapping draws.
        std::vector<uint64_t>& indirectDrawList = m_ctx->m_indirectDrawList;
        indirectDrawList.resize(m_plsDraws.size());

        if (m_ctx->m_intersectionBoard == nullptr)
        {
            m_ctx->m_intersectionBoard = std::make_unique<IntersectionBoard>();
        }
        IntersectionBoard* intersectionBoard = m_ctx->m_intersectionBoard.get();
        intersectionBoard->resizeAndReset(m_ctx->frameDescriptor().renderTarget->width(),
                                          m_ctx->frameDescriptor().renderTarget->height());

        // Build a list of sort keys that determine the final draw order.
        for (size_t i = 0; i < m_plsDraws.size(); ++i)
        {
            PLSDraw* draw = m_plsDraws[i].get();

            // Add one extra pixel of padding to the draw bounds to make absolutely certain we get
            // no overlapping pixels, which destroy the atomic shader.
            int4 drawBounds = simd::load4i(&m_plsDraws[i]->pixelBounds());
            drawBounds += int4{-1, -1, 1, 1};

            // Our top priority in re-ordering is to group non-overlapping draws together, in order
            // to maximize batching while preserving correctness.
            uint16_t drawGroupIdx = intersectionBoard->addRectangle(drawBounds);
            assert(drawGroupIdx != 0);
            uint64_t key = uint64_t(drawGroupIdx) << 48;

            // Within sub-groups of nonoverlapping draws, sort similar draw types together.
            uint64_t drawType = static_cast<uint64_t>(draw->type());
            assert(drawType < (1 << 8));
            key |= drawType << 40;

            // Within sub-groups of matching draw type, sort by texture binding.
            uint64_t textureResourceHash =
                draw->imageTexture() != nullptr ? draw->imageTexture()->textureResourceHash() : 0;
            key |= (textureResourceHash & 0xffffff) << 16;

            // Draw index goes at the bottom of the key so we know which PLSDraw it corresponds to.
            assert(i < (1 << 16));
            key |= i;

            indirectDrawList[i] = key;
        }

        // Re-order the draws!!
        std::sort(indirectDrawList.begin(), indirectDrawList.end());

        // Write out the draw data from the sorted draw list, and build up a condensed/batched list
        // of low-level draws.
        uint16_t priorDrawGroupIdx = !indirectDrawList.empty() ? indirectDrawList[0] >> 48 : 1;
        assert(priorDrawGroupIdx == 1);
        for (uint64_t key : indirectDrawList)
        {
            uint16_t drawGroupIdx = key >> 48;
            if (drawGroupIdx != priorDrawGroupIdx)
            {
                // Draws within the same group don't overlap, but once we cross into a new draw
                // group we need to insert a barrier for the overlaps to be rendered properly.
                pushBarrier();
                priorDrawGroupIdx = drawGroupIdx;
            }
            size_t drawIndex = key & 0xffff;
            m_plsDraws[drawIndex]->pushToRenderContext(this);
        }
        pushBarrier();

        // Atomic mode needs one more draw to resolve all the pixels.
        m_drawList.emplace_back(m_ctx->perFrameAllocator(), DrawType::plsAtomicResolve, 0);
        m_drawList.tail().elementCount = 1;
        m_drawList.tail().shaderFeatures = m_combinedShaderFeatures;
    }
    else
    {
        // Write out all the data for our high level draws, and build up a low-level draw list.
        for (const PLSDrawUniquePtr& draw : m_plsDraws)
        {
            draw->pushToRenderContext(this);
        }
    }

    // Pad our storage buffers to 256-byte alignment.
    m_ctx->m_pathData.push_back_n(nullptr, m_pathPaddingCount);
    m_ctx->m_paintData.push_back_n(nullptr, m_paintPaddingCount);
    m_ctx->m_paintAuxData.push_back_n(nullptr, m_paintAuxPaddingCount);
    m_ctx->m_contourData.push_back_n(nullptr, m_contourPaddingCount);

    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);
    assert(m_midpointFanTessVertexIdx == m_midpointFanTessEndLocation);
    assert(m_outerCubicTessVertexIdx == m_outerCubicTessEndLocation);

    // Update the flush descriptor's data counts that aren't known until it's written out.
    m_flushDesc.tessVertexSpanCount =
        m_ctx->m_tessSpanData.elementsWritten() - m_flushDesc.firstTessVertexSpan;
    m_flushDesc.hasTriangleVertices =
        m_ctx->m_triangleVertexData.bytesWritten() != initialTriangleVertexDataSize;

    m_flushDesc.drawList = &m_drawList;
    m_flushDesc.combinedShaderFeatures = m_combinedShaderFeatures;
    m_flushDesc.renderDirectToRasterPipeline =
        m_flushDesc.interlockMode == InterlockMode::experimentalAtomics &&
        !(m_flushDesc.combinedShaderFeatures & ShaderFeatures::ENABLE_ADVANCED_BLEND);
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
                   allocs.NAME* ITEM_SIZE_IN_BYTES* pls::kBufferRingSize)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)                                                    \
    logger.logSize(#NAME,                                                                          \
                   m_currentResourceAllocations.NAME,                                              \
                   allocs.NAME,                                                                    \
                   allocs.NAME* BYTES_PER_ROW)
#else
#define LOG_BUFFER_RING_SIZE(NAME, ITEM_SIZE_IN_BYTES)
#define LOG_TEXTURE_HEIGHT(NAME, BYTES_PER_ROW)
#endif

    LOG_BUFFER_RING_SIZE(flushUniformBufferCount, sizeof(pls::FlushUniforms));
    if (allocs.flushUniformBufferCount != m_currentResourceAllocations.flushUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeFlushUniformBuffer(allocs.flushUniformBufferCount *
                                         sizeof(pls::FlushUniforms));
    }

    LOG_BUFFER_RING_SIZE(imageDrawUniformBufferCount, sizeof(pls::ImageDrawUniforms));
    if (allocs.imageDrawUniformBufferCount !=
            m_currentResourceAllocations.imageDrawUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeImageDrawUniformBuffer(allocs.imageDrawUniformBufferCount *
                                             sizeof(pls::ImageDrawUniforms));
    }

    LOG_BUFFER_RING_SIZE(pathBufferCount, sizeof(pls::PathData));
    if (allocs.pathBufferCount != m_currentResourceAllocations.pathBufferCount || forceRealloc)
    {
        m_impl->resizePathBuffer(allocs.pathBufferCount * sizeof(pls::PathData),
                                 pls::PathData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintBufferCount, sizeof(pls::PaintData));
    if (allocs.paintBufferCount != m_currentResourceAllocations.paintBufferCount || forceRealloc)
    {
        m_impl->resizePaintBuffer(allocs.paintBufferCount * sizeof(pls::PaintData),
                                  pls::PaintData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(paintAuxBufferCount, sizeof(pls::PaintAuxData));
    if (allocs.paintAuxBufferCount != m_currentResourceAllocations.paintAuxBufferCount ||
        forceRealloc)
    {
        m_impl->resizePaintAuxBuffer(allocs.paintAuxBufferCount * sizeof(pls::PaintAuxData),
                                     pls::PaintAuxData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(contourBufferCount, sizeof(pls::ContourData));
    if (allocs.contourBufferCount != m_currentResourceAllocations.contourBufferCount ||
        forceRealloc)
    {
        m_impl->resizeContourBuffer(allocs.contourBufferCount * sizeof(pls::ContourData),
                                    pls::ContourData::kBufferStructure);
    }

    LOG_BUFFER_RING_SIZE(simpleGradientBufferCount, sizeof(pls::TwoTexelRamp));
    if (allocs.simpleGradientBufferCount !=
            m_currentResourceAllocations.simpleGradientBufferCount ||
        forceRealloc)
    {
        m_impl->resizeSimpleColorRampsBuffer(allocs.simpleGradientBufferCount *
                                             sizeof(pls::TwoTexelRamp));
    }

    LOG_BUFFER_RING_SIZE(complexGradSpanBufferCount, sizeof(pls::GradientSpan));
    if (allocs.complexGradSpanBufferCount !=
            m_currentResourceAllocations.complexGradSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeGradSpanBuffer(allocs.complexGradSpanBufferCount * sizeof(pls::GradientSpan));
    }

    LOG_BUFFER_RING_SIZE(tessSpanBufferCount, sizeof(pls::TessVertexSpan));
    if (allocs.tessSpanBufferCount != m_currentResourceAllocations.tessSpanBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTessVertexSpanBuffer(allocs.tessSpanBufferCount *
                                           sizeof(pls::TessVertexSpan));
    }

    LOG_BUFFER_RING_SIZE(triangleVertexBufferCount, sizeof(pls::TriangleVertex));
    if (allocs.triangleVertexBufferCount !=
            m_currentResourceAllocations.triangleVertexBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTriangleVertexBuffer(allocs.triangleVertexBufferCount *
                                           sizeof(pls::TriangleVertex));
    }

    allocs.gradTextureHeight = std::min(allocs.gradTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(gradTextureHeight, pls::kGradTextureWidth * 4);
    if (allocs.gradTextureHeight != m_currentResourceAllocations.gradTextureHeight || forceRealloc)
    {
        m_impl->resizeGradientTexture(pls::kGradTextureWidth, allocs.gradTextureHeight);
    }

    allocs.tessTextureHeight = std::min(allocs.tessTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(tessTextureHeight, pls::kTessTextureWidth * 4 * 4);
    if (allocs.tessTextureHeight != m_currentResourceAllocations.tessTextureHeight || forceRealloc)
    {
        m_impl->resizeTessellationTexture(pls::kTessTextureWidth, allocs.tessTextureHeight);
    }

    m_currentResourceAllocations = allocs;
}

void PLSRenderContext::mapResourceBuffers(const ResourceAllocationCounts& mapCounts)
{
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

void PLSRenderContext::LogicalFlush::pushPath(
    PatchType patchType,
    const Mat2D& matrix,
    float strokeRadius,
    FillRule fillRule,
    PaintType paintType,
    pls::SimplePaintValue simplePaintValue,
    const PLSGradient* gradient,
    const PLSTexture* imageTexture,
    uint32_t clipID,
    const pls::ClipRectInverseMatrix* clipRectInverseMatrix,
    BlendMode blendMode,
    uint32_t tessVertexCount)
{
    assert(m_hasDoneLayout);
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);

    m_currentPathIsStroked = strokeRadius != 0;
    m_currentPathNeedsMirroredContours = !m_currentPathIsStroked;
    m_ctx->m_pathData.set_back(matrix, strokeRadius);
    m_ctx->m_paintData.set_back(fillRule,
                                paintType,
                                simplePaintValue,
                                m_gradTextureLayout,
                                clipID,
                                clipRectInverseMatrix != nullptr,
                                blendMode);
    m_ctx->m_paintAuxData.set_back(matrix,
                                   paintType,
                                   simplePaintValue,
                                   gradient,
                                   imageTexture,
                                   clipRectInverseMatrix,
                                   m_ctx->frameDescriptor().renderTarget.get(),
                                   m_ctx->impl()->platformFeatures());

    ++m_currentPathID;
    assert(0 < m_currentPathID && m_currentPathID <= m_ctx->m_maxPathID);
    assert(m_flushDesc.firstPath + m_currentPathID == m_ctx->m_pathData.elementsWritten() - 1);
    assert(m_flushDesc.firstPaint + m_currentPathID == m_ctx->m_paintData.elementsWritten() - 1);
    assert(m_flushDesc.firstPaintAux + m_currentPathID ==
           m_ctx->m_paintAuxData.elementsWritten() - 1);

    pls::DrawType drawType;
    if (patchType == PatchType::midpointFan)
    {
        drawType = DrawType::midpointFanPatches;
        m_pathTessLocation = m_midpointFanTessVertexIdx;
        m_midpointFanTessVertexIdx += tessVertexCount;
        RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = m_midpointFanTessVertexIdx);
    }
    else
    {
        drawType = DrawType::outerCurvePatches;
        m_pathTessLocation = m_outerCubicTessVertexIdx;
        m_outerCubicTessVertexIdx += tessVertexCount;
        RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = m_outerCubicTessVertexIdx);
    }
    assert(m_expectedPathTessLocationAtEndOfPath <= kMaxTessellationVertexCount);

    uint32_t patchSize = PatchSegmentSpan(drawType);
    uint32_t baseInstance = m_pathTessLocation / patchSize;
    assert(baseInstance * patchSize == m_pathTessLocation); // flush() is responsible for alignment.

    if (m_currentPathNeedsMirroredContours)
    {
        assert(tessVertexCount % 2 == 0);
        m_pathTessLocation = m_pathMirroredTessLocation = m_pathTessLocation + tessVertexCount / 2;
        RIVE_DEBUG_CODE(m_expectedPathMirroredTessLocationAtEndOfPath =
                            m_pathMirroredTessLocation - tessVertexCount / 2);
    }

    DrawBatch& batch = pushPathDraw(drawType,
                                    baseInstance,
                                    fillRule,
                                    paintType,
                                    simplePaintValue,
                                    imageTexture,
                                    clipID,
                                    clipRectInverseMatrix != nullptr,
                                    blendMode);
    assert(batch.baseElement + batch.elementCount == baseInstance);
    uint32_t instanceCount = tessVertexCount / patchSize;
    assert(instanceCount * patchSize == tessVertexCount); // flush() is responsible for alignment.
    batch.elementCount += instanceCount;
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
    m_ctx->m_contourData.emplace_back(midpoint,
                                      m_currentPathID,
                                      static_cast<uint32_t>(m_pathTessLocation));
    ++m_currentContourID;
    assert(0 < m_currentContourID && m_currentContourID <= pls::kMaxContourID);
    assert(m_flushDesc.firstContour + m_currentContourID == m_ctx->m_contourData.elementsWritten());

    // The first curve of the contour will be pre-padded with 'paddingVertexCount' tessellation
    // vertices, colocated at T=0. The caller must use this argument align the end of the contour on
    // a boundary of the patch size. (See pls::PaddingToAlignUp().)
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

    if (m_currentPathNeedsMirroredContours)
    {
        pushMirroredTessellationSpans(pts,
                                      joinTangent,
                                      totalVertexCount,
                                      parametricSegmentCount,
                                      polarSegmentCount,
                                      joinSegmentCount,
                                      m_currentContourID | additionalContourFlags);
    }
    else
    {
        pushTessellationSpans(pts,
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
            // draw one both of them both again, this time behind the opposite edge of the texture
            // so we capture what got clipped off last time.
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

void PLSRenderContext::LogicalFlush::pushInteriorTriangulation(
    GrInnerFanTriangulator* triangulator,
    PaintType paintType,
    pls::SimplePaintValue simplePaintValue,
    const PLSTexture* imageTexture,
    uint32_t clipID,
    bool hasClipRect,
    BlendMode blendMode)
{
    assert(m_hasDoneLayout);

    DrawBatch& batch = pushPathDraw(DrawType::interiorTriangulation,
                                    m_ctx->m_triangleVertexData.elementsWritten(),
                                    triangulator->fillRule(),
                                    paintType,
                                    simplePaintValue,
                                    imageTexture,
                                    clipID,
                                    hasClipRect,
                                    blendMode);
    assert(m_ctx->m_triangleVertexData.hasRoomFor(triangulator->maxVertexCount()));
    size_t actualVertexCount =
        triangulator->polysToTriangles(&m_ctx->m_triangleVertexData, m_currentPathID);
    assert(actualVertexCount <= triangulator->maxVertexCount());
    batch.elementCount = actualVertexCount;
    // Interior triangulations are allowed to disable raster ordering since they are guaranteed to
    // not overlap.
    batch.needsBarrier = true;
}

void PLSRenderContext::LogicalFlush::pushImageRect(
    const Mat2D& matrix,
    float opacity,
    const PLSTexture* imageTexture,
    uint32_t clipID,
    const pls::ClipRectInverseMatrix* clipRectInverseMatrix,
    BlendMode blendMode)
{
    assert(m_hasDoneLayout);

    if (m_ctx->impl()->platformFeatures().supportsBindlessTextures)
    {
        fprintf(stderr,
                "PLSRenderContext::pushImageRect is only supported when the platform does not "
                "support bindless textures.\n");
        return;
    }
    if (!m_ctx->frameDescriptor().enableExperimentalAtomicMode)
    {
        fprintf(stderr, "PLSRenderContext::pushImageRect is only supported in atomic mode.\n");
        return;
    }

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(matrix,
                                               opacity,
                                               clipRectInverseMatrix,
                                               clipID,
                                               blendMode);

    DrawBatch& batch = pushDraw(DrawType::imageRect,
                                0,
                                PaintType::image,
                                imageTexture,
                                clipID,
                                clipRectInverseMatrix != nullptr,
                                blendMode);

    batch.elementCount = 1;
    batch.imageDrawDataOffset = imageDrawDataOffset;
    assert((batch.shaderFeatures & pls::kImageDrawShaderFeaturesMask) == batch.shaderFeatures);
}

void PLSRenderContext::LogicalFlush::pushImageMesh(
    const Mat2D& matrix,
    float opacity,
    const PLSTexture* imageTexture,
    const RenderBuffer* vertexBuffer,
    const RenderBuffer* uvBuffer,
    const RenderBuffer* indexBuffer,
    uint32_t indexCount,
    uint32_t clipID,
    const pls::ClipRectInverseMatrix* clipRectInverseMatrix,
    BlendMode blendMode)
{
    assert(m_hasDoneLayout);

    size_t imageDrawDataOffset = m_ctx->m_imageDrawUniformData.bytesWritten();
    m_ctx->m_imageDrawUniformData.emplace_back(matrix,
                                               opacity,
                                               clipRectInverseMatrix,
                                               clipID,
                                               blendMode);

    DrawBatch& batch = pushDraw(DrawType::imageMesh,
                                0,
                                PaintType::image,
                                imageTexture,
                                clipID,
                                clipRectInverseMatrix != nullptr,
                                blendMode);

    batch.elementCount = indexCount;
    batch.vertexBuffer = vertexBuffer;
    batch.uvBuffer = uvBuffer;
    batch.indexBuffer = indexBuffer;
    batch.imageDrawDataOffset = imageDrawDataOffset;
    assert((batch.shaderFeatures & pls::kImageDrawShaderFeaturesMask) == batch.shaderFeatures);
}

void PLSRenderContext::LogicalFlush::pushBarrier()
{
    assert(m_hasDoneLayout);

    if (!m_drawList.empty())
    {
        m_drawList.tail().needsBarrier = true;
    }
}

pls::DrawBatch& PLSRenderContext::LogicalFlush::pushPathDraw(DrawType drawType,
                                                             size_t baseVertex,
                                                             FillRule fillRule,
                                                             PaintType paintType,
                                                             pls::SimplePaintValue simplePaintValue,
                                                             const PLSTexture* imageTexture,
                                                             uint32_t clipID,
                                                             bool hasClipRect,
                                                             BlendMode blendMode)
{
    assert(m_hasDoneLayout);

    DrawBatch& batch =
        pushDraw(drawType, baseVertex, paintType, imageTexture, clipID, hasClipRect, blendMode);
    if (fillRule == FillRule::evenOdd)
    {
        batch.shaderFeatures |= ShaderFeatures::ENABLE_EVEN_ODD;
    }
    if (paintType == PaintType::clipUpdate && simplePaintValue.outerClipID != 0)
    {
        batch.shaderFeatures |= ShaderFeatures::ENABLE_NESTED_CLIPPING;
    }
    m_combinedShaderFeatures |= batch.shaderFeatures;
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

pls::DrawBatch& PLSRenderContext::LogicalFlush::pushDraw(DrawType drawType,
                                                         size_t baseVertex,
                                                         PaintType paintType,
                                                         const PLSTexture* imageTexture,
                                                         uint32_t clipID,
                                                         bool hasClipRect,
                                                         BlendMode blendMode)
{
    assert(m_hasDoneLayout);

    bool needsNewBatch;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::outerCurvePatches:
            needsNewBatch = m_drawList.empty() || m_drawList.tail().drawType != drawType ||
                            m_drawList.tail().needsBarrier ||
                            !can_combine_draw_images(m_drawList.tail().imageTexture, imageTexture);
            break;
        case DrawType::interiorTriangulation:
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::plsAtomicResolve:
            // We can't combine interior triangulations or image draws yet.
            needsNewBatch = true;
    }

    DrawBatch& batch =
        needsNewBatch ? m_drawList.emplace_back(m_ctx->perFrameAllocator(), drawType, baseVertex)
                      : m_drawList.tail();

    if (paintType == PaintType::image)
    {
        assert(imageTexture != nullptr);
        if (batch.imageTexture == nullptr)
        {
            batch.imageTexture = imageTexture;
        }
        assert(batch.imageTexture == imageTexture);
    }

    if (clipID != 0)
    {
        batch.shaderFeatures |= ShaderFeatures::ENABLE_CLIPPING;
    }
    if (hasClipRect && paintType != PaintType::clipUpdate)
    {
        batch.shaderFeatures |= ShaderFeatures::ENABLE_CLIP_RECT;
    }
    if (paintType != PaintType::clipUpdate)
    {
        switch (blendMode)
        {
            case BlendMode::hue:
            case BlendMode::saturation:
            case BlendMode::color:
            case BlendMode::luminosity:
                batch.shaderFeatures |= ShaderFeatures::ENABLE_HSL_BLEND_MODES;
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
                batch.shaderFeatures |= ShaderFeatures::ENABLE_ADVANCED_BLEND;
                break;
            case BlendMode::srcOver:
                break;
        }
    }

    m_combinedShaderFeatures |= batch.shaderFeatures;
    return batch;
}
} // namespace rive::pls
