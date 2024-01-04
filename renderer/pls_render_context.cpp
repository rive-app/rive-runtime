/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_render_context.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "pls_draw.hpp"
#include "pls_paint.hpp"
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
    m_impl(std::move(impl)), m_maxPathID(MaxPathID(m_impl->platformFeatures().pathIDGranularity))
{
    setResourceSizes(ResourceAllocationSizes(), /*forceRealloc =*/true);
    resetContainers();
}

PLSRenderContext::~PLSRenderContext()
{
    // Always call flush() to avoid deadlock.
    assert(!m_didBeginFrame);
    resetPLSDraws();
}

void PLSRenderContext::resetPLSDraws()
{
    for (PLSDraw* draw : m_plsDraws)
    {
        draw->releaseRefs();
    }
    m_plsDraws.clear();
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
    setResourceSizes(ResourceAllocationSizes());
    m_maxRecentResourceRequirements = ResourceAllocationSizes();
    resetContainers();
}

void PLSRenderContext::shrinkGPUResourcesToFit()
{
    assert(!m_didBeginFrame);

    // Shrink GPU resource allocations to 125% of their maximum recent usage, and only if the recent
    // usage is 2/3 or less of the current allocation.
    ResourceAllocationSizes shrinks =
        simd::if_then_else(m_maxRecentResourceRequirements.toVec() <=
                               m_currentResourceAllocations.toVec() * size_t(2) / size_t(3),
                           m_maxRecentResourceRequirements.toVec() * size_t(5) / size_t(4),
                           m_currentResourceAllocations.toVec());
    setResourceSizes(shrinks);

    // Zero out m_maxRecentResourceRequirements for the next interval.
    m_maxRecentResourceRequirements = ResourceAllocationSizes();

    resetContainers();
}

void PLSRenderContext::resetContainers()
{
    assert(!m_didBeginFrame);

    m_simpleGradients.rehash(0);
    m_simpleGradients.reserve(kDefaultSimpleGradientCapacity);

    m_pendingSimpleGradientWrites.shrink_to_fit();
    m_pendingSimpleGradientWrites.reserve(kDefaultSimpleGradientCapacity);

    m_complexGradients.rehash(0);
    m_complexGradients.reserve(kDefaultComplexGradientCapacity);

    m_pendingComplexColorRampDraws.shrink_to_fit();
    m_pendingComplexColorRampDraws.reserve(kDefaultComplexGradientCapacity);

    m_plsDraws.shrink_to_fit();
    m_plsDraws.reserve(kDefaultDrawCapacity);
}

void PLSRenderContext::beginFrame(FrameDescriptor&& frameDescriptor)
{
    assert(!m_didBeginFrame);
    m_frameDescriptor = std::move(frameDescriptor);
    if (m_frameDescriptor.enableExperimentalAtomicMode)
    {
        if (m_atomicModeData == nullptr)
        {
            m_atomicModeData = std::make_unique<pls::ExperimentalAtomicModeData>();
        }
        m_atomicModeData->setClearColorPaint(m_frameDescriptor.clearColor);
    }
    m_isFirstFlushOfFrame = true;
    m_impl->prepareToMapBuffers();
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

bool PLSRenderContext::pushDrawBatch(PLSDraw* draws[], size_t drawCount)
{
    auto countsVector = m_currentFlushCounters.toVec();
    for (size_t i = 0; i < drawCount; ++i)
    {
        countsVector += draws[i]->resourceCounts().toVec();
    }
    ResourceCounters countsWithNewBatch = countsVector;

    // Textures have hard size limits. If new batch doesn't fit in one of the textures, the caller
    // needs to flush and try again.
    if (countsWithNewBatch.pathCount > m_maxPathID ||
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

    m_plsDraws.insert(m_plsDraws.end(), draws, draws + drawCount);
    m_currentFlushCounters = countsWithNewBatch;
    return true;
}

bool PLSRenderContext::allocateGradient(const PLSGradient* gradient,
                                        ResourceCounters* counters,
                                        PaintData* paintData)
{
    assert(m_didBeginFrame);
    const float* stops = gradient->stops();
    size_t stopCount = gradient->count();

    uint32_t row, left, right;
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
        row = rampTexelsIdx / kGradTextureWidth;
        left = rampTexelsIdx % kGradTextureWidth;
        right = left + 2;
    }
    else
    {
        // This is a complex gradient. Render it to an entire row of the gradient texture.
        left = 0;
        right = kGradTextureWidth;
        GradientContentKey key(ref_rcp(gradient));
        auto iter = m_complexGradients.find(key);
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
    }
    *paintData = PaintData::MakeGradient(row, left, right, gradient->coeffs());
    return true;
}

template <typename T> bool bits_equal(const T* a, const T* b)
{
    return memcmp(a, b, sizeof(T)) == 0;
}

void PLSRenderContext::flush(FlushType flushType)
{
    assert(m_didBeginFrame);
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);

    PLSRenderContextImpl::FlushDescriptor flushDesc;
    flushDesc.backendSpecificData = m_frameDescriptor.backendSpecificData;
    flushDesc.renderTarget = m_frameDescriptor.renderTarget.get();
    flushDesc.loadAction =
        m_isFirstFlushOfFrame ? m_frameDescriptor.loadAction : LoadAction::preserveRenderTarget;
    flushDesc.clearColor = m_frameDescriptor.clearColor;
    flushDesc.pathTexelsWidth =
        std::min<uint32_t>(m_currentFlushCounters.pathCount, pls::kPathTextureWidthInItems) *
        pls::kPathTexelsPerItem;
    flushDesc.pathTexelsHeight =
        resource_texture_height<pls::kPathTextureWidthInItems>(m_currentFlushCounters.pathCount);
    flushDesc.pathDataOffset = 0;
    flushDesc.contourTexelsWidth =
        std::min<uint32_t>(m_currentFlushCounters.contourCount, pls::kContourTextureWidthInItems) *
        pls::kContourTexelsPerItem;
    flushDesc.contourTexelsHeight = resource_texture_height<pls::kContourTextureWidthInItems>(
        m_currentFlushCounters.contourCount);
    flushDesc.contourDataOffset = 0;
    flushDesc.complexGradSpanCount = m_currentFlushCounters.complexGradientSpanCount;
    flushDesc.simpleGradTexelsWidth =
        std::min<uint32_t>(m_simpleGradients.size(), pls::kGradTextureWidthInSimpleRamps) * 2;
    flushDesc.simpleGradTexelsHeight =
        resource_texture_height<pls::kGradTextureWidthInSimpleRamps>(m_simpleGradients.size());
    flushDesc.simpleGradDataOffset = 0;
    flushDesc.complexGradRowsTop = flushDesc.simpleGradTexelsHeight;
    flushDesc.complexGradRowsHeight = m_complexGradients.size();
    flushDesc.wireframe = m_frameDescriptor.wireframe;
    flushDesc.pathCount = m_currentFlushCounters.pathCount;
    flushDesc.interlockMode = m_frameDescriptor.enableExperimentalAtomicMode
                                  ? pls::InterlockMode::experimentalAtomics
                                  : pls::InterlockMode::rasterOrdered;
    if (flushDesc.interlockMode == pls::InterlockMode::experimentalAtomics)
    {
        flushDesc.experimentalAtomicModeData = m_atomicModeData.get();
    }

    // Layout the tessellation texture.
    size_t totalTessVertexCountWithPadding = 0;
    size_t midpointFanTessEndLocation = 0;
    size_t outerCubicTessEndLocation = 0;
    m_outerCubicTessVertexIdx = 0;
    m_midpointFanTessVertexIdx = 0;
    if ((m_currentFlushCounters.midpointFanTessVertexCount |
         m_currentFlushCounters.outerCubicTessVertexCount) != 0)
    {
        // midpointFan tessellation vertices reside at the beginning of the tessellation texture,
        // after 1 patch of padding vertices.
        m_midpointFanTessVertexIdx = pls::kMidpointFanPatchSegmentSpan;
        midpointFanTessEndLocation =
            m_midpointFanTessVertexIdx + (m_currentFlushCounters.midpointFanTessVertexCount);

        // outerCubic tessellation vertices reside after the midpointFan vertices, aligned on a
        // multiple of the outerCubic patch size.
        m_outerCubicTessVertexIdx =
            midpointFanTessEndLocation +
            PaddingToAlignUp<pls::kOuterCurvePatchSegmentSpan>(midpointFanTessEndLocation);
        outerCubicTessEndLocation =
            m_outerCubicTessVertexIdx + m_currentFlushCounters.outerCubicTessVertexCount;

        // We need one more padding vertex after all the tessellation vertices.
        totalTessVertexCountWithPadding = outerCubicTessEndLocation + 1;
        assert(totalTessVertexCountWithPadding <= kMaxTessellationVertexCount);
    }
    flushDesc.tessDataHeight =
        resource_texture_height<kTessTextureWidth>(totalTessVertexCountWithPadding);

    // Determine the minimum required resource allocation sizes to service this flush.
    ResourceAllocationSizes allocs;
    allocs.pathBufferCount = m_currentFlushCounters.pathCount;
    allocs.contourBufferCount = m_currentFlushCounters.contourCount;
    allocs.simpleGradientBufferCount = m_simpleGradients.size();
    allocs.complexGradSpanBufferCount = m_currentFlushCounters.complexGradientSpanCount;
    if (m_currentFlushCounters.tessellatedSegmentCount != 0)
    {
        // Conservatively account for line breaks and padding in the tessellation span count.
        // Line breaks potentially introduce a new span. Count the maximum number of line breaks we
        // might encounter, which is at most one for every line in the tessellation texture,
        // excluding the bottom line.
        size_t maxSpanBreakCount = flushDesc.tessDataHeight - 1;
        // The tessellation texture requires 3 separate spans of padding vertices (see above and
        // below).
        constexpr size_t kPaddingSpanCount = 3;
        allocs.tessSpanBufferCount =
            m_currentFlushCounters.tessellatedSegmentCount + maxSpanBreakCount + kPaddingSpanCount;
    }
    allocs.triangleVertexBufferCount = m_currentFlushCounters.maxTriangleVertexCount;
    allocs.imageDrawUniformBufferCount = m_currentFlushCounters.imageDrawCount;
    allocs.pathTextureHeight = flushDesc.pathTexelsHeight;
    allocs.contourTextureHeight = flushDesc.contourTexelsHeight;
    allocs.gradTextureHeight = flushDesc.simpleGradTexelsHeight + flushDesc.complexGradRowsHeight;
    allocs.tessTextureHeight = flushDesc.tessDataHeight;

    // The texture-transfer buffers need to update the texture in entire rows at a time.
    allocs.pathBufferCount =
        math::round_up_to_multiple_of<pls::kPathTextureWidthInItems>(allocs.pathBufferCount);
    allocs.contourBufferCount =
        math::round_up_to_multiple_of<pls::kContourTextureWidthInItems>(allocs.contourBufferCount);
    allocs.simpleGradientBufferCount =
        math::round_up_to_multiple_of<pls::kGradTextureWidthInSimpleRamps>(
            allocs.simpleGradientBufferCount);

    // Update m_maxRecentResourceRequirements before selecting the final resource sizes we will set.
    // This will prevent the next shrinkGPUResourcesToFit() from shrinking smaller than "allocs".
    m_maxRecentResourceRequirements =
        simd::max(allocs.toVec(), m_maxRecentResourceRequirements.toVec());

    // If "allocs" already fits in our current allocations, then don't change them.
    // If they don't fit, overallocate resources by 25% in order to create some slack for growth.
    setResourceSizes(simd::if_then_else(allocs.toVec() <= m_currentResourceAllocations.toVec(),
                                        m_currentResourceAllocations.toVec(),
                                        allocs.toVec() * size_t(5) / size_t(4)));

    // Update the flush uniform buffer if needed.
    FlushUniforms uniformData(m_complexGradients.size(),
                              flushDesc.tessDataHeight,
                              m_frameDescriptor.renderTarget->width(),
                              m_frameDescriptor.renderTarget->height(),
                              m_currentResourceAllocations.gradTextureHeight,
                              flushDesc.simpleGradTexelsHeight,
                              m_impl->platformFeatures());
    if (!bits_equal(&m_currentFlushUniforms, &uniformData))
    {
        WriteOnlyMappedMemory<pls::FlushUniforms> flushUniformData;
        m_impl->mapFlushUniformBuffer(&flushUniformData);
        flushUniformData.emplace_back(uniformData);
        m_impl->unmapFlushUniformBuffer();
        m_currentFlushUniforms = uniformData;
    }

    mapResourceBuffers();

    // Write out the simple gradient data.
    assert(m_simpleGradients.size() == m_pendingSimpleGradientWrites.size());
    if (!m_pendingSimpleGradientWrites.empty())
    {
        m_simpleColorRampsData.push_back_n(m_pendingSimpleGradientWrites.data(),
                                           m_pendingSimpleGradientWrites.size());
    }
    m_pendingSimpleGradientWrites.clear();
    assert(m_simpleColorRampsData.elementsWritten() == m_simpleGradients.size());

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
                m_gradSpanData.set_back(lastXFixed, xFixed, y, lastColor, colors[i]);
                lastColor = colors[i];
                lastXFixed = xFixed;
            }
            m_gradSpanData.set_back(lastXFixed, 65535u, y, lastColor, lastColor);
        }
    }
    m_pendingComplexColorRampDraws.clear();
    assert(m_gradSpanData.elementsWritten() == m_currentFlushCounters.complexGradientSpanCount);

    // Write out the vertex data for rendering padding vertices in the tessellation texture.
    if (flushDesc.tessDataHeight > 0)
    {
        // Padding at the beginning of the tessellation texture.
        pushPaddingVertices(0, pls::kMidpointFanPatchSegmentSpan);
        // Padding between patch types in the tessellation texture.
        pushPaddingVertices(midpointFanTessEndLocation,
                            m_outerCubicTessVertexIdx - midpointFanTessEndLocation);
        // The final vertex of the final patch of each contour crosses over into the next contour.
        // (This is how we wrap around back to the beginning.) Therefore, the final contour of the
        // flush needs an out-of-contour vertex to cross into as well, so we emit a padding vertex
        // here at the end.
        pushPaddingVertices(outerCubicTessEndLocation, 1);
    }

    // Write out all the data for our high level draws, and build up a low-level draw list.
    for (PLSDraw* draw : m_plsDraws)
    {
        draw->pushToRenderContext(this);
    }

    assert(m_pathData.elementsWritten() == m_currentFlushCounters.pathCount);
    assert(m_contourData.elementsWritten() == m_currentFlushCounters.contourCount);
    assert(m_tessSpanData.elementsWritten() <= totalTessVertexCountWithPadding);
    assert(m_imageDrawUniformData.elementsWritten() == m_currentFlushCounters.imageDrawCount);
    assert(m_midpointFanTessVertexIdx == midpointFanTessEndLocation);
    assert(m_outerCubicTessVertexIdx == outerCubicTessEndLocation);

    flushDesc.tessVertexSpanCount = m_tessSpanData.elementsWritten();
    flushDesc.hasTriangleVertices = m_triangleVertexData.bytesWritten() != 0;

    for (const Draw& draw : m_drawList)
    {
        flushDesc.needsClipBuffer =
            flushDesc.needsClipBuffer || (draw.shaderFeatures & ShaderFeatures::ENABLE_CLIPPING);
        flushDesc.combinedShaderFeatures |= draw.shaderFeatures;
    }

    if (m_frameDescriptor.enableExperimentalAtomicMode)
    {
        // Atomic mode needs one more draw to resolve all the pixels.
        Draw& draw = m_drawList.emplace_back(this, DrawType::plsAtomicResolve, 0);
        draw.elementCount = 1;
        draw.shaderFeatures = flushDesc.combinedShaderFeatures;
    }

    flushDesc.drawList = &m_drawList;

    unmapResourceBuffers();

    m_impl->flush(flushDesc);

    m_lastGeneratedClipID = 0;
    m_clipContentID = 0;

    m_currentFlushCounters = ResourceCounters();
    m_simpleGradients.clear();
    m_complexGradients.clear();

    m_currentPathID = 0;
    m_currentContourID = 0;

    m_isFirstFlushOfFrame = false;

    m_drawList.reset();

    // Manually release the references held by the draw list before resetting it to empty, since
    // TrivialBlockAllocator prevents us from doing this automatically in a destructor.
    resetPLSDraws();

    if (flushType == FlushType::intermediate)
    {
        // The frame isn't complete yet. The caller will begin preparing a new flush immediately
        // after this method returns, so lock buffers for the next flush now.
        m_impl->prepareToMapBuffers();
    }
    else
    {
        // Delete all objects that were allocted for this frame using TrivialBlockAllocator.
        // Keep them alive between intermediate flushes since this data is designed to persist for
        // an entire frame.
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

    ++m_flushCount;
}

void PLSRenderContext::setResourceSizes(ResourceAllocationSizes allocs, bool forceRealloc)
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

    LOG_BUFFER_RING_SIZE(pathBufferCount, sizeof(pls::PathData));
    if (allocs.pathBufferCount != m_currentResourceAllocations.pathBufferCount || forceRealloc)
    {
        m_impl->resizePathBuffer(allocs.pathBufferCount * sizeof(pls::PathData));
    }

    LOG_BUFFER_RING_SIZE(contourBufferCount, sizeof(pls::ContourData));
    if (allocs.contourBufferCount != m_currentResourceAllocations.contourBufferCount ||
        forceRealloc)
    {
        m_impl->resizeContourBuffer(allocs.contourBufferCount * sizeof(pls::ContourData));
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

    LOG_BUFFER_RING_SIZE(imageDrawUniformBufferCount, sizeof(pls::ImageDrawUniforms));
    if (allocs.imageDrawUniformBufferCount !=
            m_currentResourceAllocations.imageDrawUniformBufferCount ||
        forceRealloc)
    {
        m_impl->resizeImageDrawUniformBuffer(allocs.imageDrawUniformBufferCount *
                                             sizeof(pls::ImageDrawUniforms));
    }

    LOG_BUFFER_RING_SIZE(triangleVertexBufferCount, sizeof(pls::TriangleVertex));
    if (allocs.triangleVertexBufferCount !=
            m_currentResourceAllocations.triangleVertexBufferCount ||
        forceRealloc)
    {
        m_impl->resizeTriangleVertexBuffer(allocs.triangleVertexBufferCount *
                                           sizeof(pls::TriangleVertex));
    }

    allocs.pathTextureHeight = std::min(allocs.pathTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(pathTextureHeight, pls::kPathTextureWidthInItems * sizeof(pls::PathData));
    if (allocs.pathTextureHeight != m_currentResourceAllocations.pathTextureHeight || forceRealloc)
    {
        m_impl->resizePathTexture(pls::kPathTextureWidthInTexels, allocs.pathTextureHeight);
    }

    allocs.contourTextureHeight = std::min(allocs.contourTextureHeight, kMaxTextureHeight);
    LOG_TEXTURE_HEIGHT(contourTextureHeight,
                       pls::kContourTextureWidthInItems * sizeof(pls::ContourData));
    if (allocs.contourTextureHeight != m_currentResourceAllocations.contourTextureHeight ||
        forceRealloc)
    {
        m_impl->resizeContourTexture(pls::kContourTextureWidthInTexels,
                                     allocs.contourTextureHeight);
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

void PLSRenderContext::mapResourceBuffers()
{
    if (m_currentResourceAllocations.pathBufferCount > 0)
        m_impl->mapPathBuffer(&m_pathData);
    assert(m_pathData.hasRoomFor(m_currentResourceAllocations.pathBufferCount));

    if (m_currentResourceAllocations.contourBufferCount > 0)
        m_impl->mapContourBuffer(&m_contourData);
    assert(m_contourData.hasRoomFor(m_currentResourceAllocations.contourBufferCount));

    if (m_currentResourceAllocations.simpleGradientBufferCount > 0)
        m_impl->mapSimpleColorRampsBuffer(&m_simpleColorRampsData);
    assert(
        m_simpleColorRampsData.hasRoomFor(m_currentResourceAllocations.simpleGradientBufferCount));

    if (m_currentResourceAllocations.complexGradSpanBufferCount > 0)
        m_impl->mapGradSpanBuffer(&m_gradSpanData);
    assert(m_gradSpanData.hasRoomFor(m_currentResourceAllocations.complexGradSpanBufferCount));

    if (m_currentResourceAllocations.tessSpanBufferCount > 0)
        m_impl->mapTessVertexSpanBuffer(&m_tessSpanData);
    assert(m_tessSpanData.hasRoomFor(m_currentResourceAllocations.tessSpanBufferCount));

    if (m_currentResourceAllocations.imageDrawUniformBufferCount > 0)
        m_impl->mapImageDrawUniformBuffer(&m_imageDrawUniformData);
    assert(m_imageDrawUniformData.hasRoomFor(
        m_currentResourceAllocations.imageDrawUniformBufferCount > 0));

    if (m_currentResourceAllocations.triangleVertexBufferCount > 0)
        m_impl->mapTriangleVertexBuffer(&m_triangleVertexData);
    assert(m_triangleVertexData.hasRoomFor(m_currentResourceAllocations.triangleVertexBufferCount));
}

void PLSRenderContext::unmapResourceBuffers()
{
    if (m_pathData)
    {
        m_impl->unmapPathBuffer(m_pathData.bytesWritten());
        m_pathData.reset();
    }
    if (m_contourData)
    {
        m_impl->unmapContourBuffer(m_contourData.bytesWritten());
        m_contourData.reset();
    }
    if (m_simpleColorRampsData)
    {
        m_impl->unmapSimpleColorRampsBuffer(m_simpleColorRampsData.bytesWritten());
        m_simpleColorRampsData.reset();
    }
    if (m_gradSpanData)
    {
        m_impl->unmapGradSpanBuffer(m_gradSpanData.bytesWritten());
        m_gradSpanData.reset();
    }
    if (m_tessSpanData)
    {
        m_impl->unmapTessVertexSpanBuffer(m_tessSpanData.bytesWritten());
        m_tessSpanData.reset();
    }
    if (m_triangleVertexData)
    {
        m_impl->unmapTriangleVertexBuffer(m_triangleVertexData.bytesWritten());
        m_triangleVertexData.reset();
    }
    if (m_imageDrawUniformData)
    {
        m_impl->unmapImageDrawUniformBuffer(m_imageDrawUniformData.bytesWritten());
        m_imageDrawUniformData.reset();
    }
}

void PLSRenderContext::pushPaddingVertices(uint32_t tessLocation, uint32_t count)
{
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

void PLSRenderContext::pushPath(PatchType patchType,
                                const Mat2D& matrix,
                                float strokeRadius,
                                FillRule fillRule,
                                PaintType paintType,
                                const PaintData& paintData,
                                const PLSTexture* imageTexture,
                                uint32_t clipID,
                                const pls::ClipRectInverseMatrix* clipRectInverseMatrix,
                                BlendMode blendMode,
                                uint32_t tessVertexCount)
{
    assert(m_didBeginFrame);
    assert(m_pathTessLocation == m_expectedPathTessLocationAtEndOfPath);
    assert(m_pathMirroredTessLocation == m_expectedPathMirroredTessLocationAtEndOfPath);

    m_currentPathIsStroked = strokeRadius != 0;
    m_currentPathNeedsMirroredContours = !m_currentPathIsStroked;
    m_pathData.set_back(matrix,
                        strokeRadius,
                        fillRule,
                        paintType,
                        clipID,
                        blendMode,
                        paintData,
                        clipRectInverseMatrix);

    ++m_currentPathID;
    assert(0 < m_currentPathID && m_currentPathID <= m_maxPathID);
    assert(m_currentPathID == m_pathData.bytesWritten() / sizeof(PathData));

    auto drawType = patchType == PatchType::midpointFan ? DrawType::midpointFanPatches
                                                        : DrawType::outerCurvePatches;
    m_pathTessLocation = patchType == PatchType::midpointFan ? m_midpointFanTessVertexIdx
                                                             : m_outerCubicTessVertexIdx;
    uint32_t patchSize = PatchSegmentSpan(drawType);
    uint32_t baseInstance = m_pathTessLocation / patchSize;
    // flush() pads m_midpointFanTessVertexIdx and m_outerCubicTessVertexIdx to ensure they begin on
    // multiples of the patch size.
    assert(baseInstance * patchSize == m_pathTessLocation);
    pushPathDraw(drawType,
                 baseInstance,
                 fillRule,
                 paintType,
                 paintData,
                 imageTexture,
                 clipID,
                 clipRectInverseMatrix != nullptr,
                 blendMode);
    assert(m_drawList.tail().baseElement + m_drawList.tail().elementCount == baseInstance);
    uint32_t instanceCount = tessVertexCount / patchSize;
    // The caller is responsible to pad each contour so it ends on a multiple of the patch size.
    assert(instanceCount * patchSize == tessVertexCount);
    m_drawList.tail().elementCount += instanceCount;

    if (patchType == PatchType::midpointFan)
    {
        m_midpointFanTessVertexIdx = m_pathTessLocation + tessVertexCount;
        RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = m_midpointFanTessVertexIdx);
    }
    else
    {
        m_outerCubicTessVertexIdx = m_pathTessLocation + tessVertexCount;
        RIVE_DEBUG_CODE(m_expectedPathTessLocationAtEndOfPath = m_outerCubicTessVertexIdx);
    }
    assert(m_expectedPathTessLocationAtEndOfPath <= kMaxTessellationVertexCount);

    if (m_currentPathNeedsMirroredContours)
    {
        assert(tessVertexCount % 2 == 0);
        m_pathTessLocation = m_pathMirroredTessLocation = m_pathTessLocation + tessVertexCount / 2;
        RIVE_DEBUG_CODE(m_expectedPathMirroredTessLocationAtEndOfPath =
                            m_pathMirroredTessLocation - tessVertexCount / 2);
    }

    if (m_frameDescriptor.enableExperimentalAtomicMode)
    {
        // Atomic mode fetches the paint and clip info from storage buffers in the fragment shader.
        m_atomicModeData->setDataForPath(m_currentPathID,
                                         matrix,
                                         fillRule,
                                         paintType,
                                         paintData,
                                         imageTexture,
                                         clipID,
                                         clipRectInverseMatrix,
                                         blendMode,
                                         m_frameDescriptor.renderTarget.get(),
                                         m_currentFlushUniforms,
                                         m_impl->platformFeatures());
    }
}

void PLSRenderContext::pushContour(Vec2D midpoint, bool closed, uint32_t paddingVertexCount)
{
    assert(m_didBeginFrame);
    assert(m_pathData.bytesWritten() > 0);
    assert(m_currentPathIsStroked || closed);
    assert(m_currentPathID != 0); // pathID can't be zero.

    if (m_currentPathIsStroked)
    {
        midpoint.x = closed ? 1 : 0;
    }
    m_contourData.emplace_back(midpoint,
                               m_currentPathID,
                               static_cast<uint32_t>(m_pathTessLocation));
    ++m_currentContourID;
    assert(0 < m_currentContourID && m_currentContourID <= kMaxContourID);
    assert(m_currentContourID == m_contourData.bytesWritten() / sizeof(ContourData));

    // The first curve of the contour will be pre-padded with 'paddingVertexCount' tessellation
    // vertices, colocated at T=0. The caller must use this argument align the end of the contour on
    // a boundary of the patch size. (See pls::PaddingToAlignUp().)
    m_currentContourPaddingVertexCount = paddingVertexCount;
}

void PLSRenderContext::pushCubic(const Vec2D pts[4],
                                 Vec2D joinTangent,
                                 uint32_t additionalContourFlags,
                                 uint32_t parametricSegmentCount,
                                 uint32_t polarSegmentCount,
                                 uint32_t joinSegmentCount)
{
    assert(m_didBeginFrame);
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

RIVE_ALWAYS_INLINE void PLSRenderContext::pushTessellationSpans(const Vec2D pts[4],
                                                                Vec2D joinTangent,
                                                                uint32_t totalVertexCount,
                                                                uint32_t parametricSegmentCount,
                                                                uint32_t polarSegmentCount,
                                                                uint32_t joinSegmentCount,
                                                                uint32_t contourIDWithFlags)
{
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

RIVE_ALWAYS_INLINE void PLSRenderContext::pushMirroredTessellationSpans(
    const Vec2D pts[4],
    Vec2D joinTangent,
    uint32_t totalVertexCount,
    uint32_t parametricSegmentCount,
    uint32_t polarSegmentCount,
    uint32_t joinSegmentCount,
    uint32_t contourIDWithFlags)
{
    int32_t y = m_pathTessLocation / kTessTextureWidth;
    int32_t x0 = m_pathTessLocation % kTessTextureWidth;
    int32_t x1 = x0 + totalVertexCount;

    uint32_t reflectionY = (m_pathMirroredTessLocation - 1) / kTessTextureWidth;
    int32_t reflectionX0 = (m_pathMirroredTessLocation - 1) % kTessTextureWidth + 1;
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

void PLSRenderContext::pushInteriorTriangulation(GrInnerFanTriangulator* triangulator,
                                                 PaintType paintType,
                                                 const PaintData& paintData,
                                                 const PLSTexture* imageTexture,
                                                 uint32_t clipID,
                                                 bool hasClipRect,
                                                 BlendMode blendMode)
{
    pushPathDraw(DrawType::interiorTriangulation,
                 m_triangleVertexData.elementsWritten(),
                 triangulator->fillRule(),
                 paintType,
                 paintData,
                 imageTexture,
                 clipID,
                 hasClipRect,
                 blendMode);
    assert(m_triangleVertexData.hasRoomFor(triangulator->maxVertexCount()));
    size_t actualVertexCount =
        triangulator->polysToTriangles(&m_triangleVertexData, m_currentPathID);
    assert(actualVertexCount <= triangulator->maxVertexCount());
    m_drawList.tail().elementCount = actualVertexCount;
}

void PLSRenderContext::pushImageRect(const Mat2D& matrix,
                                     float opacity,
                                     const PLSTexture* imageTexture,
                                     uint32_t clipID,
                                     const pls::ClipRectInverseMatrix* clipRectInverseMatrix,
                                     BlendMode blendMode)
{
    if (m_impl->platformFeatures().supportsBindlessTextures)
    {
        fprintf(stderr,
                "PLSRenderContext::pushImageRect is only supported when the platform does not "
                "support bindless textures.\n");
        return;
    }
    if (!frameDescriptor().enableExperimentalAtomicMode)
    {
        fprintf(stderr, "PLSRenderContext::pushImageRect is only supported in atomic mode.\n");
        return;
    }

    size_t imageDrawDataOffset = m_imageDrawUniformData.bytesWritten();
    m_imageDrawUniformData.emplace_back(matrix, opacity, clipRectInverseMatrix, clipID, blendMode);

    pushDraw(DrawType::imageRect,
             0,
             PaintType::image,
             imageTexture,
             clipID,
             clipRectInverseMatrix != nullptr,
             blendMode);

    Draw& draw = m_drawList.tail();
    draw.elementCount = 1;
    draw.imageDrawDataOffset = imageDrawDataOffset;
    assert((draw.shaderFeatures & pls::kImageDrawShaderFeaturesMask) == draw.shaderFeatures);
}

void PLSRenderContext::pushImageMesh(const Mat2D& matrix,
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
    size_t imageDrawDataOffset = m_imageDrawUniformData.bytesWritten();
    m_imageDrawUniformData.emplace_back(matrix, opacity, clipRectInverseMatrix, clipID, blendMode);

    pushDraw(DrawType::imageMesh,
             0,
             PaintType::image,
             imageTexture,
             clipID,
             clipRectInverseMatrix != nullptr,
             blendMode);

    Draw& draw = m_drawList.tail();
    draw.elementCount = indexCount;
    draw.vertexBuffer = vertexBuffer;
    draw.uvBuffer = uvBuffer;
    draw.indexBuffer = indexBuffer;
    draw.imageDrawDataOffset = imageDrawDataOffset;
    assert((draw.shaderFeatures & pls::kImageDrawShaderFeaturesMask) == draw.shaderFeatures);
}

void PLSRenderContext::pushPathDraw(DrawType drawType,
                                    size_t baseVertex,
                                    FillRule fillRule,
                                    PaintType paintType,
                                    const PaintData& paintData,
                                    const PLSTexture* imageTexture,
                                    uint32_t clipID,
                                    bool hasClipRect,
                                    BlendMode blendMode)
{
    pushDraw(drawType, baseVertex, paintType, imageTexture, clipID, hasClipRect, blendMode);

    ShaderFeatures& shaderFeatures = m_drawList.tail().shaderFeatures;
    if (fillRule == FillRule::evenOdd)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_EVEN_ODD;
    }
    if (paintType == PaintType::clipUpdate && paintData.outerClipIDIfClipUpdate() != 0)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_NESTED_CLIPPING;
    }
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

void PLSRenderContext::pushDraw(DrawType drawType,
                                size_t baseVertex,
                                PaintType paintType,
                                const PLSTexture* imageTexture,
                                uint32_t clipID,
                                bool hasClipRect,
                                BlendMode blendMode)
{
    bool needsNewDraw;
    switch (drawType)
    {
        case DrawType::midpointFanPatches:
        case DrawType::outerCurvePatches:
            needsNewDraw = m_frameDescriptor.enableExperimentalAtomicMode || m_drawList.empty() ||
                           m_drawList.tail().drawType != drawType ||
                           !can_combine_draw_images(m_drawList.tail().imageTexture, imageTexture);
            break;
        case DrawType::interiorTriangulation:
        case DrawType::imageRect:
        case DrawType::imageMesh:
        case DrawType::plsAtomicResolve:
            // We can't combine interior triangulations or image draws yet.
            needsNewDraw = true;
    }
    if (needsNewDraw)
    {
        m_drawList.emplace_back(this, drawType, baseVertex);
    }

    if (paintType == PaintType::image)
    {
        assert(imageTexture != nullptr);
        if (m_drawList.tail().imageTexture == nullptr)
        {
            m_drawList.tail().imageTexture = imageTexture;
        }
        assert(m_drawList.tail().imageTexture == imageTexture);
    }

    ShaderFeatures& shaderFeatures = m_drawList.tail().shaderFeatures;
    if (clipID != 0)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIPPING;
    }
    if (hasClipRect && paintType != PaintType::clipUpdate)
    {
        shaderFeatures |= ShaderFeatures::ENABLE_CLIP_RECT;
    }
    if (paintType != PaintType::clipUpdate)
    {
        switch (blendMode)
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
}
} // namespace rive::pls
