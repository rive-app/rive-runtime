/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/vec2d.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/rive_render_factory.hpp"
#include "rive/renderer/render_target.hpp"
#include "rive/renderer/sk_rectanizer_skyline.hpp"
#include "rive/renderer/trivial_block_allocator.hpp"
#include "rive/shapes/paint/color.hpp"
#include <array>
#include <unordered_map>

class PushRetrofittedTrianglesGMDraw;
class RenderContextTest;

namespace rive
{
class RawPath;
class RiveRenderPaint;
class RiveRenderPath;
} // namespace rive

namespace rive::gpu
{
class GradientLibrary;
class IntersectionBoard;
class ImageMeshDraw;
class ImageRectDraw;
class StencilClipReset;
class Draw;
class Gradient;
class RenderContextImpl;
class PathDraw;

// Used as a key for complex gradients.
class GradientContentKey
{
public:
    inline GradientContentKey(rcp<const Gradient> gradient);
    inline GradientContentKey(GradientContentKey&& other);
    bool operator==(const GradientContentKey&) const;
    const Gradient* gradient() const { return m_gradient.get(); }

private:
    rcp<const Gradient> m_gradient;
};

// Hashes all stops and all colors in a complex gradient.
class DeepHashGradient
{
public:
    size_t operator()(const GradientContentKey&) const;
};

// Even though Draw is block-allocated, we still need to call releaseRefs() on
// each individual instance before releasing the block. This smart pointer
// guarantees we always call releaseRefs() (implementation in pls_draw.hpp).
struct DrawReleaseRefs
{
    void operator()(Draw* draw);
};
using DrawUniquePtr = std::unique_ptr<Draw, DrawReleaseRefs>;

// Top-level, API agnostic rendering context for RiveRenderer. This class
// manages all the GPU buffers, context state, and other resources required for
// Rive's pixel local storage path rendering algorithm.
class RenderContext : public RiveRenderFactory
{
public:
    RenderContext(std::unique_ptr<RenderContextImpl>);
    ~RenderContext();

    RenderContextImpl* impl() { return m_impl.get(); }
    template <typename T> T* static_impl_cast()
    {
        return static_cast<T*>(m_impl.get());
    }

    const gpu::PlatformFeatures& platformFeatures() const;

    // Options for controlling how and where a frame is rendered.
    struct FrameDescriptor
    {
        uint32_t renderTargetWidth = 0;
        uint32_t renderTargetHeight = 0;
        LoadAction loadAction = LoadAction::clear;
        ColorInt clearColor = 0;
        // If nonzero, the number of MSAA samples to use.
        // Setting this to a nonzero value forces msaa mode.
        int msaaSampleCount = 0;
        // Use atomic mode (preferred) or msaa instead of rasterOrdering.
        bool disableRasterOrdering = false;

        // Testing flags.
        bool wireframe = false;
        bool fillsDisabled = false;
        bool strokesDisabled = false;
        // Override all paths' fill rules (winding or even/odd) to emulate
        // clockwiseAtomic mode.
        bool clockwiseFillOverride = false;
    };

    // Called at the beginning of a frame and establishes where and how it will
    // be rendered.
    //
    // All rendering related calls must be made between beginFrame() and
    // flush().
    void beginFrame(const FrameDescriptor&);

    const FrameDescriptor& frameDescriptor() const
    {
        assert(m_didBeginFrame);
        return m_frameDescriptor;
    }

    // True if bounds is empty or outside [0, 0, renderTargetWidth,
    // renderTargetHeight].
    bool isOutsideCurrentFrame(const IAABB& pixelBounds);

    // True if the current frame supports draws with clipRects
    // (clipRectInverseMatrix != null). If false, all clipping must be done with
    // clipPaths.
    bool frameSupportsClipRects() const;

    // If the frame doesn't support image paints, the client must draw images
    // with pushImageRect(). If it DOES support image paints, the client CANNOT
    // use pushImageRect(); it should draw images as rectangular paths with an
    // image paint.
    bool frameSupportsImagePaintForPaths() const;

    const gpu::InterlockMode frameInterlockMode() const
    {
        return m_frameInterlockMode;
    }

    // Generates a unique clip ID that is guaranteed to not exist in the current
    // clip buffer, and assigns a contentBounds to it.
    //
    // Returns 0 if a unique ID could not be generated, at which point the
    // caller must issue a logical flush and try again.
    uint32_t generateClipID(const IAABB& contentBounds);

    // Screen-space bounding box of the region inside the given clip.
    const IAABB& getClipContentBounds(uint32_t clipID)
    {
        assert(m_didBeginFrame);
        assert(!m_logicalFlushes.empty());
        return m_logicalFlushes.back()->getClipInfo(clipID).contentBounds;
    }

    // Mark the given clip as being read from within a screen-space bounding
    // box.
    void addClipReadBounds(uint32_t clipID, const IAABB& bounds)
    {
        assert(m_didBeginFrame);
        assert(!m_logicalFlushes.empty());
        return m_logicalFlushes.back()->addClipReadBounds(clipID, bounds);
    }

    // Union of screen-space bounding boxes from all draws that read the given
    // clip element.
    const IAABB& getClipReadBounds(uint32_t clipID)
    {
        assert(m_didBeginFrame);
        assert(!m_logicalFlushes.empty());
        return m_logicalFlushes.back()->getClipInfo(clipID).readBounds;
    }

    // Get/set a "clip content ID" that uniquely identifies the current contents
    // of the clip buffer. This ID is reset to 0 on every logical flush.
    void setClipContentID(uint32_t clipID)
    {
        assert(m_didBeginFrame);
        m_clipContentID = clipID;
    }

    uint32_t getClipContentID()
    {
        assert(m_didBeginFrame);
        return m_clipContentID;
    }

    // Appends a list of high-level Draws to the current frame.
    // Returns false if the draws don't fit within the current resource
    // constraints, at which point the caller must issue a logical flush and try
    // again.
    [[nodiscard]] bool pushDraws(DrawUniquePtr draws[], size_t drawCount);

    // Records a "logical" flush, in that it builds up commands to break up the
    // render pass and re-render the resource textures, but it won't submit any
    // command buffers or rotate/synchronize the buffer rings.
    void logicalFlush();

    // GPU resources required to execute the GPU commands for a frame.
    struct FlushResources
    {
        RenderTarget* renderTarget = nullptr;

        // Command buffer that rendering commands will be added to.
        //  - VkCommandBuffer on Vulkan.
        //  - id<MTLCommandBuffer> on Metal.
        //  - Unused otherwise.
        void* externalCommandBuffer = nullptr;

        // Resource lifetime counters. Resources used during the upcoming flush
        // will belong to 'currentFrameNumber'. Resources last used on or before
        // 'safeFrameNumber' are safe to be released or recycled.
        uint64_t currentFrameNumber = 0;
        uint64_t safeFrameNumber = 0;
    };

    // Submits all GPU commands that have been built up since beginFrame().
    void flush(const FlushResources&);

    // Called when the client will stop rendering. Releases all CPU and GPU
    // resources associated with this render context.
    void releaseResources();

    // Returns the context's TrivialBlockAllocator, which is automatically reset
    // at the end of every frame. (Memory in this allocator is preserved between
    // logical flushes.)
    TrivialBlockAllocator& perFrameAllocator()
    {
        assert(m_didBeginFrame);
        return m_perFrameAllocator;
    }

    // Allocators for intermediate path processing buffers.
    TrivialArrayAllocator<uint8_t>& numChopsAllocator()
    {
        return m_numChopsAllocator;
    }
    TrivialArrayAllocator<Vec2D>& chopVerticesAllocator()
    {
        return m_chopVerticesAllocator;
    }
    TrivialArrayAllocator<std::array<Vec2D, 2>>& tangentPairsAllocator()
    {
        return m_tangentPairsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>&
    polarSegmentCountsAllocator()
    {
        return m_polarSegmentCountsAllocator;
    }
    TrivialArrayAllocator<uint32_t, alignof(float4)>&
    parametricSegmentCountsAllocator()
    {
        return m_parametricSegmentCountsAllocator;
    }

    // Allocates a trivially destructible object that will be automatically
    // dropped at the end of the current frame.
    template <typename T, typename... Args> T* make(Args&&... args)
    {
        assert(m_didBeginFrame);
        return m_perFrameAllocator.make<T>(std::forward<Args>(args)...);
    }

    // Backend-specific RiveRenderFactory implementation.
    rcp<RenderBuffer> makeRenderBuffer(RenderBufferType,
                                       RenderBufferFlags,
                                       size_t) override;
    rcp<RenderImage> decodeImage(Span<const uint8_t>) override;

private:
    friend class Draw;
    friend class PathDraw;
    friend class ImageRectDraw;
    friend class ImageMeshDraw;
    friend class StencilClipReset;
    friend class ::PushRetrofittedTrianglesGMDraw; // For testing.
    friend class ::RenderContextTest;              // For testing.

    // Resets the CPU-side STL containers so they don't have unbounded growth.
    void resetContainers();

    // Throttled width/height of the atlas texture. If drawing to a render
    // target larger than this, we may create a larger atlas anyway.
    uint32_t atlasMaxSize() const
    {
        constexpr static uint32_t MAX_ATLAS_MAX_SIZE = 4096;
        return std::min(platformFeatures().maxTextureSize, MAX_ATLAS_MAX_SIZE);
    }

    // Defines the exact size of each of our GPU resources. Computed during
    // flush(), based on LogicalFlush::ResourceCounters and
    // LogicalFlush::LayoutCounters.
    struct ResourceAllocationCounts
    {
        constexpr static int NUM_ELEMENTS = 14;
        using VecType = simd::gvec<size_t, NUM_ELEMENTS>;

        RIVE_ALWAYS_INLINE VecType toVec() const
        {
            static_assert(sizeof(*this) == sizeof(size_t) * NUM_ELEMENTS);
            static_assert(sizeof(VecType) >= sizeof(*this));
            VecType vec;
            RIVE_INLINE_MEMCPY(&vec, this, sizeof(*this));
            return vec;
        }

        RIVE_ALWAYS_INLINE ResourceAllocationCounts(const VecType& vec)
        {
            static_assert(sizeof(*this) == sizeof(size_t) * NUM_ELEMENTS);
            static_assert(sizeof(VecType) >= sizeof(*this));
            RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
        }

        ResourceAllocationCounts() = default;

        size_t flushUniformBufferCount = 0;
        size_t imageDrawUniformBufferCount = 0;
        size_t pathBufferCount = 0;
        size_t paintBufferCount = 0;
        size_t paintAuxBufferCount = 0;
        size_t contourBufferCount = 0;
        size_t gradSpanBufferCount = 0;
        size_t tessSpanBufferCount = 0;
        size_t triangleVertexBufferCount = 0;
        size_t gradTextureHeight = 0;
        size_t tessTextureHeight = 0;
        size_t atlasTextureWidth = 0;
        size_t atlasTextureHeight = 0;
        size_t coverageBufferLength = 0; // clockwiseAtomic mode only.
    };

    // Reallocates GPU resources and updates m_currentResourceAllocations.
    // If forceRealloc is true, every GPU resource is allocated, even if the
    // size would not change.
    void setResourceSizes(ResourceAllocationCounts, bool forceRealloc = false);

    void mapResourceBuffers(const ResourceAllocationCounts&);
    void unmapResourceBuffers();

    // Returns the next coverage buffer prefix to use in a logical flush.
    // Sets needsCoverageBufferClear if the coverage buffer must be cleared in
    // order to support the returned coverage buffer prefix.
    // (clockwiseAtomic mode only.)
    uint32_t incrementCoverageBufferPrefix(bool* needsCoverageBufferClear);

    const std::unique_ptr<RenderContextImpl> m_impl;
    const size_t m_maxPathID;

    ResourceAllocationCounts m_currentResourceAllocations;
    ResourceAllocationCounts m_maxRecentResourceRequirements;
    double m_lastResourceTrimTimeInSeconds;

    // Per-frame state.
    FrameDescriptor m_frameDescriptor;
    gpu::InterlockMode m_frameInterlockMode;
    gpu::ShaderFeatures m_frameShaderFeaturesMask;
    RIVE_DEBUG_CODE(bool m_didBeginFrame = false;)

    // Clipping state.
    uint32_t m_clipContentID = 0;

    // Monotonically increasing prefix that gets appended to the most
    // significant "32 - CLOCKWISE_COVERAGE_BIT_COUNT" bits of coverage buffer
    // values.
    //
    // Increasing this prefix implicitly clears the entire coverage buffer to
    // zero.
    //
    // (clockwiseAtomic mode only.)
    uint32_t m_coverageBufferPrefix = 0;

    // Used by LogicalFlushes for re-ordering high level draws.
    std::vector<int64_t> m_indirectDrawList;
    std::unique_ptr<IntersectionBoard> m_intersectionBoard;

    WriteOnlyMappedMemory<gpu::FlushUniforms> m_flushUniformData;
    WriteOnlyMappedMemory<gpu::PathData> m_pathData;
    WriteOnlyMappedMemory<gpu::PaintData> m_paintData;
    WriteOnlyMappedMemory<gpu::PaintAuxData> m_paintAuxData;
    WriteOnlyMappedMemory<gpu::ContourData> m_contourData;
    WriteOnlyMappedMemory<gpu::GradientSpan> m_gradSpanData;
    WriteOnlyMappedMemory<gpu::TessVertexSpan> m_tessSpanData;
    WriteOnlyMappedMemory<gpu::TriangleVertex> m_triangleVertexData;
    WriteOnlyMappedMemory<gpu::ImageDrawUniforms> m_imageDrawUniformData;

    // Simple allocator for trivially-destructible data that needs to persist
    // until the current frame has completed. All memory in this allocator is
    // dropped at the end of the every frame.
    constexpr static size_t kPerFlushAllocatorInitialBlockSize =
        1024 * 1024; // 1 MiB.
    TrivialBlockAllocator m_perFrameAllocator{
        kPerFlushAllocatorInitialBlockSize};

    // Allocators for intermediate path processing buffers.
    constexpr static size_t kIntermediateDataInitialStrokes =
        8192; // * 84 == 688 KiB.
    constexpr static size_t kIntermediateDataInitialFillCurves =
        32768; // * 4 == 128 KiB.
    TrivialArrayAllocator<uint8_t> m_numChopsAllocator{
        kIntermediateDataInitialStrokes * 4}; // 4 byte per stroke curve.
    TrivialArrayAllocator<Vec2D> m_chopVerticesAllocator{
        kIntermediateDataInitialStrokes * 4}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<std::array<Vec2D, 2>> m_tangentPairsAllocator{
        kIntermediateDataInitialStrokes * 2}; // 32 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)>
        m_polarSegmentCountsAllocator{kIntermediateDataInitialStrokes *
                                      4}; // 16 bytes per stroke curve.
    TrivialArrayAllocator<uint32_t, alignof(float4)>
        m_parametricSegmentCountsAllocator{
            kIntermediateDataInitialFillCurves}; // 4 bytes per fill curve.

    class TessellationWriter;

    // Manages a list of high-level Draws and their required resources.
    //
    // Since textures have hard size limits, we can't always fit an entire frame
    // into one flush. It's rare for us to require more than one flush in a
    // single frame, but for the times that we do, this flush logic is
    // encapsulated in a nested class that can be built up into a list and
    // executed the end of a frame.
    class LogicalFlush
    {
    public:
        LogicalFlush(RenderContext* parent);

        // Rewinds this flush object back to an empty state without shrinking
        // any internal allocations held by CPU-side STL containers.
        void rewind();

        // Resets the CPU-side STL containers so they don't have unbounded
        // growth.
        void resetContainers();

        const FrameDescriptor& frameDescriptor() const
        {
            return m_ctx->frameDescriptor();
        }
        gpu::InterlockMode interlockMode() const
        {
            return m_ctx->frameInterlockMode();
        }

        // Access this flush's gpu::FlushDescriptor (which is not valid until
        // layoutResources()). NOTE: Some fields in the FlushDescriptor
        // (tessVertexSpanCount, hasTriangleVertices, drawList, and
        // combinedShaderFeatures) do not become valid until after
        // writeResources().
        const gpu::FlushDescriptor& desc()
        {
            assert(m_hasDoneLayout);
            return m_flushDesc;
        }

        // Generates a unique clip ID that is guaranteed to not exist in the
        // current clip buffer.
        //
        // Returns 0 if a unique ID could not be generated, at which point the
        // caller must issue a logical flush and try again.
        uint32_t generateClipID(const IAABB& contentBounds);

        struct ClipInfo
        {
            ClipInfo(const IAABB& contentBounds_) :
                contentBounds(contentBounds_)
            {}

            // Screen-space bounding box of the region inside the clip.
            const IAABB contentBounds;

            // Union of screen-space bounding boxes from all draws that read the
            // clip.
            //
            // (Initialized with a maximally negative rectangle whose union with
            // any other rectangle will be equal to that same rectangle.)
            IAABB readBounds = {std::numeric_limits<int32_t>::max(),
                                std::numeric_limits<int32_t>::max(),
                                std::numeric_limits<int32_t>::min(),
                                std::numeric_limits<int32_t>::min()};
        };

        const ClipInfo& getClipInfo(uint32_t clipID)
        {
            return getWritableClipInfo(clipID);
        }

        // Mark the given clip as being read from within a screen-space bounding
        // box.
        void addClipReadBounds(uint32_t clipID, const IAABB& bounds);

        // Appends a list of high-level Draws to the flush.
        // Returns false if the draws don't fit within the current resource
        // constraints, at which point the context must append a new logical
        // flush and try again.
        [[nodiscard]] bool pushDraws(DrawUniquePtr draws[], size_t drawCount);

        // Running counts of data records required by Draws that need to be
        // allocated in the render context's various GPU buffers.
        struct ResourceCounters
        {
            constexpr static int NUM_ELEMENTS = 7;
            using VecType = simd::gvec<size_t, NUM_ELEMENTS>;

            VecType toVec() const
            {
                static_assert(sizeof(*this) == sizeof(size_t) * NUM_ELEMENTS);
                static_assert(sizeof(VecType) >= sizeof(*this));
                VecType vec;
                RIVE_INLINE_MEMCPY(&vec, this, sizeof(*this));
                return vec;
            }

            ResourceCounters(const VecType& vec)
            {
                static_assert(sizeof(*this) == sizeof(size_t) * NUM_ELEMENTS);
                static_assert(sizeof(VecType) >= sizeof(*this));
                RIVE_INLINE_MEMCPY(this, &vec, sizeof(*this));
            }

            ResourceCounters() = default;

            size_t midpointFanTessVertexCount = 0;
            size_t outerCubicTessVertexCount = 0;
            size_t pathCount = 0;
            size_t contourCount = 0;
            // lines, curves, lone joins, emulated caps, etc.
            size_t maxTessellatedSegmentCount = 0;
            size_t maxTriangleVertexCount = 0;
            size_t imageDrawCount = 0; // imageRect or imageMesh.
        };

        // Additional counters for layout state that don't need to be tracked by
        // individual draws.
        struct LayoutCounters
        {
            uint32_t pathPaddingCount = 0;
            uint32_t paintPaddingCount = 0;
            uint32_t paintAuxPaddingCount = 0;
            uint32_t contourPaddingCount = 0;
            uint32_t gradSpanCount = 0;
            uint32_t gradSpanPaddingCount = 0;
            uint32_t maxGradTextureHeight = 0;
            uint32_t maxTessTextureHeight = 0;
            uint32_t maxAtlasWidth = 0;
            uint32_t maxAtlasHeight = 0;
            size_t maxCoverageBufferLength = 0;
        };

        // Allocates a horizontal span of texels in the gradient texture and
        // schedules either a texture upload or a draw that fills it with the
        // given gradient's color ramp.
        //
        // Fills out a ColorRampLocation record that tells the shader how to
        // access the gradient.
        //
        // Returns false if the gradient texture is out of space, at which point
        // the caller must issue a logical flush and try again.
        [[nodiscard]] bool allocateGradient(const Gradient*,
                                            gpu::ColorRampLocation*);

        // Allocates a rectangular region in the atlas for this draw to use, and
        // registers a future callback to PathDraw::pushAtlasTessellation()
        // where it will render its coverage data to this same region in the
        // atlas.
        //
        // Attempts to leave a border of "desiredPadding" pixels surrounding the
        // rectangular region, but the allocation may not be padded if the path
        // is up against an edge.
        bool allocateAtlasDraw(PathDraw*,
                               uint16_t drawWidth,
                               uint16_t drawHeight,
                               uint16_t desiredPadding,
                               uint16_t* x,
                               uint16_t* y,
                               TAABB<uint16_t>* paddedRegion);

        // Reserves a range within the coverage buffer for a path to use in
        // clockwiseAtomic mode.
        //
        // "length" is the length in pixels of this allocation and must be a
        // multiple of 32*32, in order to support 32x32 tiling.
        //
        // Returns the offset of the allocated range within the coverage buffer,
        // or -1 if there was not room.
        size_t allocateCoverageBufferRange(size_t length);

        // Carves out space for this specific flush within the total frame's
        // resource buffers and lays out the flush-specific resource textures.
        // Updates the total frame running conters based on layout.
        void layoutResources(const FlushResources&,
                             size_t logicalFlushIdx,
                             bool isFinalFlushOfFrame,
                             ResourceCounters* runningFrameResourceCounts,
                             LayoutCounters* runningFrameLayoutCounts);

        // Called after all flushes in a frame have done their layout and the
        // render context has allocated and mapped its resource buffers. Writes
        // the GPU data for this flush to the context's actively mapped resource
        // buffers.
        void writeResources();

        // Reserves a span of "count" vertices from the "midpointFanPatches"
        // section of the tessellation texture.
        //
        // This method must be called for a total count of precisely
        // "m_resourceCounts.midpointFanTessVertexCount" vertices.
        //
        // The caller must fill these vertices in with TessellationWriter.
        //
        // Returns the index of the first vertex in the newly allocated span.
        uint32_t allocateMidpointFanTessVertices(uint32_t count);

        // Reserves a span of "count" vertices from the "outerCurvePatches"
        // section of the tessellation texture.
        //
        // This method must be called for a total count of precisely
        // "m_resourceCounts.outerCubicTessVertexCount" vertices.
        //
        // The caller must fill these vertices in with TessellationWriter.
        //
        // Returns the index of the first vertex in the newly allocated span.
        uint32_t allocateOuterCubicTessVertices(uint32_t count);

        // Allocates and initializes a record on the GPU for the given path.
        //
        // Returns a unique 16-bit "pathID" handle for this specific record.
        //
        // This method does not add the path to the draw list. The caller must
        // define that draw specifically with a separate call to
        // pushMidpointFanDraw() or pushOuterCubicsDraw().
        [[nodiscard]] uint32_t pushPath(const PathDraw* draw);

        // Pushes a contour record to the GPU that references the given path.
        //
        // "vertexIndex0" is the index within the tessellation where the first
        // vertex of the contour resides. Shaders need this when the contour is
        // closed.
        //
        // Returns a unique 16-bit "contourID" handle for this specific record.
        // This ID may be or-ed with '*_CONTOUR_FLAG' bits from constants.glsl.
        [[nodiscard]] uint32_t pushContour(uint32_t pathID,
                                           Vec2D midpoint,
                                           bool isStroke,
                                           bool closed,
                                           uint32_t vertexIndex0);

        // Writes padding vertices to the tessellation texture, with an invalid
        // contour ID that is guaranteed to not be the same ID as any neighbors.
        void pushPaddingVertices(uint32_t count, uint32_t tessLocation);

        // Pushes a "midpointFanPatches" draw to the list. Path, contour, and
        // cubic data are pushed separately.
        //
        // Also adds the PathDraw to a dstRead list if one is
        // required, and if this is the path's first subpass.
        void pushMidpointFanDraw(
            const PathDraw*,
            uint32_t tessVertexCount,
            uint32_t tessLocation,
            gpu::ShaderMiscFlags = gpu::ShaderMiscFlags::none);

        // Pushes an "outerCurvePatches" draw to the list. Path, contour, and
        // cubic data are pushed separately.
        //
        // Also adds the PathDraw to a dstRead list if one is
        // required, and if this is the path's first subpass.
        void pushOuterCubicsDraw(
            const PathDraw*,
            uint32_t tessVertexCount,
            uint32_t tessLocation,
            gpu::ShaderMiscFlags = gpu::ShaderMiscFlags::none);

        // Writes out triangle verties for the desired WindingFaces and pushes
        // an "interiorTriangulation" draw to the list.
        // Returns the number of vertices actually written.
        size_t pushInteriorTriangulationDraw(
            const PathDraw*,
            uint32_t pathID,
            gpu::WindingFaces,
            gpu::ShaderMiscFlags = gpu::ShaderMiscFlags::none);

        // Pushes a screen-space rectangle to the draw list, whose pixel
        // coverage is determined by the atlas region associated with the given
        // pathID.
        void pushAtlasBlit(PathDraw*, uint32_t pathID);

        // Pushes an "imageRect" to the draw list.
        // This should only be used when we in atomic mode. Otherwise, images
        // should be drawn as rectangular paths with an image paint.
        void pushImageRectDraw(ImageRectDraw*);

        // Pushes an "imageMesh" draw to the list.
        void pushImageMeshDraw(ImageMeshDraw*);

        // Pushes a "stencilClipReset" draw to the list.
        void pushStencilClipResetDraw(StencilClipReset*);

    private:
        friend class TessellationWriter;

        ClipInfo& getWritableClipInfo(uint32_t clipID);

        // Adds a barrier to the end of the draw list that prevents further
        // combining/batching and instructs the backend to issue a graphics
        // barrier, if necessary.
        void pushBarrier();

        // Either appends a new drawBatch to m_drawList or merges into
        // m_drawList.tail(). Updates the batch's ShaderFeatures according to
        // the passed parameters.
        DrawBatch& pushPathDraw(const PathDraw*,
                                DrawType,
                                gpu::ShaderMiscFlags,
                                uint32_t vertexCount,
                                uint32_t baseVertex);
        DrawBatch& pushDraw(const Draw*,
                            DrawType,
                            gpu::ShaderMiscFlags,
                            gpu::PaintType,
                            uint32_t elementCount,
                            uint32_t baseElement);

        // Instance pointer to the outer parent class.
        RenderContext* const m_ctx;

        // Running counts of GPU data records that need to be allocated for
        // draws.
        ResourceCounters m_resourceCounts;

        // Running count of combined prepasses and subpasses from every draw in
        // m_draws.
        int m_drawPassCount;

        // Simple gradients have one stop at t=0 and one stop at t=1. They're
        // implemented with 2 texels.
        std::unordered_map<uint64_t, uint32_t>
            m_simpleGradients; // [color0, color1] -> texelsIdx.
        std::vector<gpu::TwoTexelRamp> m_pendingSimpleGradDraws;

        // Complex gradients have stop(s) between t=0 and t=1. In theory they
        // should be scaled to a ramp where every stop lands exactly on a pixel
        // center, but for now we just always scale them to the entire gradient
        // texture width.
        std::unordered_map<GradientContentKey, uint16_t, DeepHashGradient>
            m_complexGradients; // [colors[0..n], stops[0..n]] -> rowIdx
        std::vector<const Gradient*> m_pendingComplexGradDraws;

        // Simple and complex gradients both get uploaded to the GPU as sets of
        // "GradientSpan" instances.
        size_t m_pendingGradSpanCount;

        std::vector<ClipInfo> m_clips;

        // High-level draw list. These get built into a low-level list of
        // gpu::DrawBatch objects during writeResources().
        std::vector<DrawUniquePtr> m_draws;
        IAABB m_combinedDrawBounds;

        // Layout state.
        uint32_t m_pathPaddingCount;
        uint32_t m_paintPaddingCount;
        uint32_t m_paintAuxPaddingCount;
        uint32_t m_contourPaddingCount;
        uint32_t m_gradSpanPaddingCount;
        uint32_t m_midpointFanTessEndLocation;
        uint32_t m_outerCubicTessEndLocation;
        uint32_t m_outerCubicTessVertexIdx;
        uint32_t m_midpointFanTessVertexIdx;

        gpu::GradTextureLayout m_gradTextureLayout;

        gpu::FlushDescriptor m_flushDesc;

        BlockAllocatedLinkedList<DrawBatch> m_drawList;
        gpu::ShaderFeatures m_combinedShaderFeatures;

        // Most recent path and contour state.
        uint32_t m_currentPathID;
        uint32_t m_currentContourID;

        // Atlas for offscreen feathering.
        std::unique_ptr<skgpu::RectanizerSkyline> m_atlasRectanizer;
        uint32_t m_atlasMaxX = 0;
        uint32_t m_atlasMaxY = 0;
        std::vector<PathDraw*> m_pendingAtlasDraws;

        // Total coverage allocated via allocateCoverageBufferRange().
        // (clockwiseAtomic mode only.)
        uint32_t m_coverageBufferLength = 0;

        // Stateful Z index of the current draw being pushed. Used by msaa mode
        // to avoid double hits and to reverse-sort opaque paths front to back.
        uint32_t m_currentZIndex;

        RIVE_DEBUG_CODE(bool m_hasDoneLayout = false;)
    };

    std::vector<std::unique_ptr<LogicalFlush>> m_logicalFlushes;

    // Writes out TessVertexSpans that are used to tessellate the vertices
    // in a path.
    class TessellationWriter
    {
    public:
        // forwardTessLocation & mirroredTessLocation are allocated by
        // allocate*TessVertices().
        //
        // forwardTessLocation starts at the beginning of the vertex span
        // and advances forward.
        //
        // mirroredTessLocation starts at the end of the vertex span and
        // advances backward.
        //
        // If the ContourDirections are double sided, forwardTessVertexCount
        // & mirroredTessVertexCount must both be equal, and
        // forwardTessLocation & mirroredTessLocation must both be valid.
        // Otherwise, one span or the other may be empty.
        TessellationWriter(LogicalFlush* flush,
                           uint32_t pathID,
                           gpu::ContourDirections,
                           uint32_t forwardTessVertexCount,
                           uint32_t forwardTessLocation,
                           uint32_t mirroredTessVertexCount = 0,
                           uint32_t mirroredTessLocation = 0);

        ~TessellationWriter();

        // Returns the index of the next vertex to be written.
        //
        // In the case of double-sided tessellations the next vertex gets
        // tessellated twice, and either index will be identical. So we just
        // return the next *forward* tessellation index when it's double sided.
        uint32_t nextVertexIndex()
        {
            return m_contourDirections != gpu::ContourDirections::reverse
                       ? m_pathTessLocation
                       : m_pathMirroredTessLocation - 1;
        }

        // Wrapper around LogicalFlush::pushContour(), with an additional
        // padding option.
        //
        // The first curve of the contour will be pre-padded with
        // 'paddingVertexCount' tessellation vertices, colocated at T=0. The
        // caller must use this argument to align the end of the contour on
        // a boundary of the patch size. (See gpu::PaddingToAlignUp().)
        [[nodiscard]] uint32_t pushContour(Vec2D midpoint,
                                           bool isStroke,
                                           bool closed,
                                           uint32_t paddingVertexCount);

        // Wites out (potentially wrapped) TessVertexSpan(s) that tessellate
        // a cubic curve & join at the current tessellation location(s).
        // Advances the tessellation location(s).
        //
        // The bottom 16 bits of contourIDWithFlags must match the most
        // recent contourID returned by pushContour(), but it may also have
        // extra '*_CONTOUR_FLAG' bits from constants.glsl
        //
        // An instance consists of a cubic curve with
        // "parametricSegmentCount + polarSegmentCount" segments, followed
        // by a join with "joinSegmentCount" segments, for a grand total of
        // "parametricSegmentCount + polarSegmentCount + joinSegmentCount -
        // 1" vertices.
        //
        // If a cubic has already been pushed to the current contour, pts[0]
        // must be equal to the former cubic's pts[3].
        //
        // "joinTangent" is the ending tangent of the join that follows the
        // cubic.
        void pushCubic(const Vec2D pts[4],
                       gpu::ContourDirections,
                       Vec2D joinTangent,
                       uint32_t parametricSegmentCount,
                       uint32_t polarSegmentCount,
                       uint32_t joinSegmentCount,
                       uint32_t contourIDWithFlags);

        // pushCubic() impl for forward tessellations.
        RIVE_ALWAYS_INLINE void pushTessellationSpans(
            const Vec2D pts[4],
            Vec2D joinTangent,
            uint32_t totalVertexCount,
            uint32_t parametricSegmentCount,
            uint32_t polarSegmentCount,
            uint32_t joinSegmentCount,
            uint32_t contourIDWithFlags);

        // pushCubic() impl for mirrored tessellations.
        RIVE_ALWAYS_INLINE void pushMirroredTessellationSpans(
            const Vec2D pts[4],
            Vec2D joinTangent,
            uint32_t totalVertexCount,
            uint32_t parametricSegmentCount,
            uint32_t polarSegmentCount,
            uint32_t joinSegmentCount,
            uint32_t contourIDWithFlags);

        // Functionally equivalent to "pushMirroredTessellationSpans();
        // pushTessellationSpans();", but packs each forward and mirrored
        // pair into a single gpu::TessVertexSpan.
        RIVE_ALWAYS_INLINE void pushDoubleSidedTessellationSpans(
            const Vec2D pts[4],
            Vec2D joinTangent,
            uint32_t totalVertexCount,
            uint32_t parametricSegmentCount,
            uint32_t polarSegmentCount,
            uint32_t joinSegmentCount,
            uint32_t contourIDWithFlags);

    private:
        LogicalFlush* const m_flush;
        WriteOnlyMappedMemory<gpu::TessVertexSpan>& m_tessSpanData;
        const uint32_t m_pathID;
        const gpu::ContourDirections m_contourDirections;
        uint32_t m_pathTessLocation;
        uint32_t m_pathMirroredTessLocation;
        // Padding to add to the next curve.
        uint32_t m_nextCubicPaddingVertexCount = 0;
        RIVE_DEBUG_CODE(uint32_t m_expectedPathTessEndLocation;)
        RIVE_DEBUG_CODE(uint32_t m_expectedPathMirroredTessEndLocation;)
    };
};
} // namespace rive::gpu
