/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/fixed_queue.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class RiveRenderPath;
class RiveRenderPaint;
} // namespace rive

namespace rive::gpu
{
class Draw;
class RenderContext;
class Gradient;

// High level abstraction of a single object to be drawn (path, imageRect, or
// imageMesh). These get built up for an entire frame in order to count GPU
// resource allocation sizes, and then sorted, batched, and drawn.
class Draw
{
public:
    // Use a "fullscreen" bounding box that is reasonably larger than any
    // screen, but not so big that it runs the risk of overflowing.
    constexpr static IAABB kFullscreenPixelBounds = {0, 0, 1 << 24, 1 << 24};

    enum class Type : uint8_t
    {
        path,
        imageRect,
        imageMesh,
        stencilClipReset,
    };

    Draw(IAABB pixelBounds,
         const Mat2D&,
         BlendMode,
         rcp<const Texture> imageTexture,
         Type);

    const Texture* imageTexture() const { return m_imageTextureRef; }
    const IAABB& pixelBounds() const { return m_pixelBounds; }
    const Mat2D& matrix() const { return m_matrix; }
    BlendMode blendMode() const { return m_blendMode; }
    Type type() const { return m_type; }
    gpu::DrawContents drawContents() const { return m_drawContents; }
    bool isOpaque() const
    {
        return m_drawContents & gpu::DrawContents::opaquePaint;
    }
    uint32_t clipID() const { return m_clipID; }
    bool hasClipRect() const { return m_clipRectInverseMatrix != nullptr; }
    const gpu::ClipRectInverseMatrix* clipRectInverseMatrix() const
    {
        return m_clipRectInverseMatrix;
    }
    gpu::SimplePaintValue simplePaintValue() const
    {
        return m_simplePaintValue;
    }

    // Clipping setup.
    void setClipID(uint32_t clipID);
    void setClipRect(const gpu::ClipRectInverseMatrix* m)
    {
        m_clipRectInverseMatrix = m;
    }

    // Used to allocate GPU resources for a collection of draws.
    using ResourceCounters = RenderContext::LogicalFlush::ResourceCounters;
    const ResourceCounters& resourceCounts() const { return m_resourceCounts; }

    // Combined number of low-level draws required to render this object. The
    // renderContext will call pushToRenderContext() "prepassCount() +
    // subpassCount()" times, potentially with other non-overlapping draws in
    // between.
    //
    // All prepasses from all draws are rendered first, before drawing any
    // subpasses. Prepasses are rendered in front-to-back order and subpasses
    // are drawn back-to-front.
    int prepassCount() const { return m_prepassCount; }
    int subpassCount() const { return m_subpassCount; }

    // When shaders don't have a mechanism to read the framebuffer (e.g.,
    // WebGL msaa), this is a linked list of all the draws from a single batch
    // whose bounding boxes needs to be blitted to the "dstRead" texture before
    // drawing.
    const Draw* addToDstReadList(const Draw* head) const
    {
        assert(m_nextDstRead == nullptr);
        m_nextDstRead = head;
        return this;
    };
    const Draw* nextDstRead() const { return m_nextDstRead; }

    // Finalizes m_prepassCount and m_subpassCount.
    virtual void determineSubpasses()
    {
        // The subclass must set m_prepassCount and m_subpassCount in this call
        // if they are not 0 & 1.
        assert(m_prepassCount == 0);
        assert(m_subpassCount == 1);
    }

    // Allocates any remaining resources necessary for the draw (gradients,
    // coverage buffer ranges, atlas slots, etc.).
    //
    // Returns false if any allocation failed due to resource constraints, at
    // which point the caller will have to issue a logical flush and try again.
    virtual bool allocateResources(RenderContext::LogicalFlush*)
    {
        return true;
    }

    // Pushes the data for the given subpassIndex of this draw to the
    // renderContext. Called once the GPU buffers have been counted and
    // allocated, and the draws have been sorted.
    //
    // NOTE: Subpasses are not necessarily rendered one after the other.
    // Separate, non-overlapping draws may have gotten sorted between subpasses.
    virtual void pushToRenderContext(RenderContext::LogicalFlush*,
                                     int subpassIndex) = 0;

    // We can't have a destructor because we're block-allocated. Instead, the
    // client calls this method before clearing the drawList to release all our
    // held references.
    virtual void releaseRefs();

protected:
    const Texture* const m_imageTextureRef;
    const IAABB m_pixelBounds;
    const Mat2D m_matrix;
    const BlendMode m_blendMode;
    const Type m_type;

    uint32_t m_clipID = 0;
    const gpu::ClipRectInverseMatrix* m_clipRectInverseMatrix = nullptr;

    gpu::DrawContents m_drawContents = gpu::DrawContents::none;

    // Filled in by the subclass constructor.
    ResourceCounters m_resourceCounts;

    // Before issuing the main draws, the renderContext may do a front-to-back
    // pass. Any draw who wants to participate in front-to-back rendering can
    // register a positive prepass count during determineSubpasses().
    //
    // For prepasses, pushToRenderContext() gets called with subpassIndex
    // values: [-m_prepassCount, .., -1].
    int m_prepassCount = 0;

    // This is the number of low-level draws that the draw requires during main
    // (back-to-front) rendering. A draw can register the number of subpasses it
    // requires during determineSubpasses().
    //
    // For subpasses, pushToRenderContext() gets called with subpassIndex
    // values: [0, .., m_subpassCount - 1].
    int m_subpassCount = 1;

    gpu::SimplePaintValue m_simplePaintValue;

    // When shaders don't have a mechanism to read the framebuffer (e.g.,
    // WebGL msaa), this is a linked list of all the draws from a single batch
    // whose bounding boxes needs to be blitted to the "dstRead" texture before
    // drawing.
    const Draw mutable* m_nextDstRead = nullptr;
};

// Implement DrawReleaseRefs (defined in pls_render_context.hpp) now that Draw
// is defined.
inline void DrawReleaseRefs::operator()(Draw* draw) { draw->releaseRefs(); }

// High level abstraction of a single path to be drawn (midpoint fan or interior
// triangulation).
class PathDraw : public Draw
{
public:
    // Creates either a normal path draw or an interior triangulation if the
    // path is large enough.
    static DrawUniquePtr Make(RenderContext*,
                              const Mat2D&,
                              rcp<const RiveRenderPath>,
                              FillRule,
                              const RiveRenderPaint*,
                              RawPath* scratchPath);

    // Determines how coverage is calculated for antialiasing and feathers.
    // CoverageType is mostly decided by the InterlockMode, but we keep these
    // concepts separate because atlas coverage may be used with any
    // InterlockMode.
    enum class CoverageType
    {
        pixelLocalStorage, // InterlockMode::rasterOrdering and atomics
        clockwiseAtomic,   // InterlockMode::clockwiseAtomic
        msaa,              // InterlockMode::msaa
        atlas, // Any InterlockMode may opt to use atlas coverage for large
               // feathers; msaa always has to use an atlas for feathers.
    };

    PathDraw(IAABB pixelBounds,
             const Mat2D&,
             rcp<const RiveRenderPath>,
             FillRule,
             const RiveRenderPaint*,
             CoverageType,
             const RenderContext::FrameDescriptor&);

    CoverageType coverageType() const { return m_coverageType; }

    const Gradient* gradient() const { return m_gradientRef; }
    gpu::PaintType paintType() const { return m_paintType; }
    bool isFeatheredFill() const
    {
        return m_featherRadius != 0 && m_strokeRadius == 0;
    }
    bool isStrokeOrFeather() const
    {
        return (math::bit_cast<uint32_t>(m_featherRadius) |
                math::bit_cast<uint32_t>(m_strokeRadius)) != 0;
    }
    bool isStroke() const { return m_strokeRadius != 0; }
    float strokeRadius() const { return m_strokeRadius; }
    float featherRadius() const { return m_featherRadius; }
    gpu::ContourDirections contourDirections() const
    {
        return m_contourDirections;
    }

    // Only used when rendering coverage via the atlas.
    const gpu::AtlasTransform& atlasTransform() const
    {
        return m_atlasTransform;
    }
    const TAABB<uint16_t>& atlasScissor() const { return m_atlasScissor; }
    bool atlasScissorEnabled() const { return m_atlasScissorEnabled; }

    // clockwiseAtomic only.
    const gpu::CoverageBufferRange& coverageBufferRange() const
    {
        return m_coverageBufferRange;
    }

    GrInnerFanTriangulator* triangulator() const { return m_triangulator; }

    bool allocateResources(RenderContext::LogicalFlush*) override;
    void determineSubpasses() override;

    void pushToRenderContext(RenderContext::LogicalFlush*,
                             int subpassIndex) override;

    // Called after pushToRenderContext(), and only when this draw uses an atlas
    // for tessellation. In the CoverageType::atlas case, pushToRenderContext()
    // is where we emit the rectangle that reads the atlas (and writes to the
    // main render target). So this method is where we push the tessellation
    // that gets rendered separately to the offscreen atlas.
    void pushAtlasTessellation(RenderContext::LogicalFlush*,
                               uint32_t* tessVertexCount,
                               uint32_t* tessBaseVertex);

    void releaseRefs() override;

protected:
    static CoverageType SelectCoverageType(const RiveRenderPaint*,
                                           float matrixMaxScale,
                                           const gpu::PlatformFeatures&,
                                           gpu::InterlockMode);

    // Prepares to draw the path by tessellating a fan around its midpoint.
    void initForMidpointFan(RenderContext*, const RiveRenderPaint*);

    enum class TriangulatorAxis
    {
        horizontal,
        vertical,
        dontCare,
    };

    // Prepares to draw the path by triangulating the interior into
    // non-overlapping triangles and tessellating the outer cubics.
    void initForInteriorTriangulation(RenderContext*,
                                      RawPath*,
                                      TriangulatorAxis);

    void pushTessellationData(RenderContext::LogicalFlush*,
                              uint32_t* tessVertexCount,
                              uint32_t* tessLocation);

    // Pushes the contours and cubics to the renderContext for a
    // "midpointFanPatches" draw.
    void pushMidpointFanTessellationData(RenderContext::TessellationWriter*);

    // Emulates a stroke cap before the given cubic by pushing a copy of the
    // cubic, reversed, with 0 tessellation segments leading up to the join
    // section, and a 180-degree join that looks like the desired stroke cap.
    void pushEmulatedStrokeCapAsJoinBeforeCubic(
        RenderContext::TessellationWriter*,
        const Vec2D cubic[],
        uint32_t strokeCapSegmentCount,
        uint32_t contourIDWithFlags);

    enum class InteriorTriangulationOp : bool
    {
        // Fills in m_resourceCounts and runs a GrInnerFanTriangulator on the
        // path's interior polygon.
        countDataAndTriangulate,

        // Pushes the contours and cubics to the renderContext for an
        // "outerCurvePatches" draw.
        pushOuterCubicTessellationData,
    };

    // Called to processes the interior triangulation both during initialization
    // and submission. For now, we just iterate and subdivide the path twice
    // (once for each enum in InteriorTriangulationOp). Since we only do this
    // for large paths, and since we're triangulating the path interior anyway,
    // adding complexity to only run Wang's formula and chop once would save
    // about ~5% of the total CPU time. (And large paths are GPU-bound anyway.)
    void iterateInteriorTriangulation(InteriorTriangulationOp op,
                                      TrivialBlockAllocator*,
                                      RawPath* scratchPath,
                                      TriangulatorAxis,
                                      RenderContext::TessellationWriter*);

    const RiveRenderPath* const m_pathRef;
    const FillRule m_pathFillRule; // Fill rule can mutate on RenderPath.
    const Gradient* m_gradientRef;
    const gpu::PaintType m_paintType;
    const CoverageType m_coverageType;
    float m_strokeRadius = 0;
    float m_featherRadius = 0;
    gpu::ContourDirections m_contourDirections;
    uint32_t m_contourFlags = 0;

    // Only used when rendering coverage via the atlas.
    gpu::AtlasTransform m_atlasTransform;
    TAABB<uint16_t> m_atlasScissor; // Scissor rect when rendering to the atlas.
    bool m_atlasScissorEnabled;

    // clockwiseAtomic only.
    gpu::CoverageBufferRange m_coverageBufferRange;

    GrInnerFanTriangulator* m_triangulator = nullptr;

    StrokeJoin m_strokeJoin;
    StrokeCap m_strokeCap;
    float m_strokeMatrixMaxScale;
    float m_polarSegmentsPerRadian;

    struct ContourInfo
    {
        RawPath::Iter endOfContour;
        size_t endLineIdx;
        size_t firstCurveIdx;
        size_t endCurveIdx;
        size_t firstRotationIdx; // We measure rotations on both curves and
                                 // round joins.
        size_t endRotationIdx;
        Vec2D midpoint;
        bool closed;
        size_t strokeJoinCount;
        uint32_t strokeCapSegmentCount;
        uint32_t paddingVertexCount;
        RIVE_DEBUG_CODE(uint32_t tessVertexCount;)
    };

    ContourInfo* m_contours;
    FixedQueue<uint8_t> m_numChops;
    FixedQueue<Vec2D> m_chopVertices;
    std::array<Vec2D, 2>* m_tangentPairs = nullptr;
    uint32_t* m_polarSegmentCounts = nullptr;
    uint32_t* m_parametricSegmentCounts = nullptr;

    // Unique ID used by shaders for the current frame.
    uint32_t m_pathID = 0;

    // Used in clockwiseAtomic mode. The negative triangles get rendered in a
    // separate prepass, and their tessellations need to be allocated before the
    // main subpass pushes the path to the renderContext.
    uint32_t m_prepassTessLocation = 0;

    // Used to guarantee m_pathRef doesn't change for the entire time we hold
    // it.
    RIVE_DEBUG_CODE(uint64_t m_rawPathMutationID;)

    // Consistency checks for pushToRenderContext().
    RIVE_DEBUG_CODE(size_t m_pendingLineCount;)
    RIVE_DEBUG_CODE(size_t m_pendingCurveCount;)
    RIVE_DEBUG_CODE(size_t m_pendingRotationCount;)
    RIVE_DEBUG_CODE(size_t m_pendingStrokeJoinCount;)
    RIVE_DEBUG_CODE(size_t m_pendingStrokeCapCount;)
    // Counts how many additional curves were pushed by
    // pushEmulatedStrokeCapAsJoinBeforeCubic().
    RIVE_DEBUG_CODE(size_t m_pendingEmptyStrokeCountForCaps;)
    RIVE_DEBUG_CODE(size_t m_numInteriorTriangleVerticesPushed = 0;)
};

// Pushes an imageRect to the render context.
// This should only be used in atomic mode. Otherwise, images should be drawn as
// rectangular paths with an image paint.
class ImageRectDraw : public Draw
{
public:
    ImageRectDraw(RenderContext*,
                  IAABB pixelBounds,
                  const Mat2D&,
                  BlendMode,
                  rcp<const Texture>,
                  float opacity);

    float opacity() const { return m_opacity; }

    void pushToRenderContext(RenderContext::LogicalFlush*,
                             int subpassIndex) override;

protected:
    const float m_opacity;
};

// Pushes an imageMesh to the render context.
class ImageMeshDraw : public Draw
{
public:
    ImageMeshDraw(IAABB pixelBounds,
                  const Mat2D&,
                  BlendMode,
                  rcp<const Texture>,
                  rcp<RenderBuffer> vertexBuffer,
                  rcp<RenderBuffer> uvBuffer,
                  rcp<RenderBuffer> indexBuffer,
                  uint32_t indexCount,
                  float opacity);

    RenderBuffer* vertexBuffer() const { return m_vertexBufferRef; }
    RenderBuffer* uvBuffer() const { return m_uvBufferRef; }
    RenderBuffer* indexBuffer() const { return m_indexBufferRef; }
    uint32_t indexCount() const { return m_indexCount; }
    float opacity() const { return m_opacity; }

    void pushToRenderContext(RenderContext::LogicalFlush*,
                             int subpassIndex) override;

    void releaseRefs() override;

protected:
    RenderBuffer* const m_vertexBufferRef;
    RenderBuffer* const m_uvBufferRef;
    RenderBuffer* const m_indexBufferRef;
    const uint32_t m_indexCount;
    const float m_opacity;
};

// Resets the stencil clip by either entirely erasing the existing clip, or
// intersecting it with a nested clip (i.e., erasing the region outside the
// nested clip).
class StencilClipReset : public Draw
{
public:
    enum class ResetAction
    {
        clearPreviousClip,
        intersectPreviousClip,
    };

    StencilClipReset(RenderContext*,
                     uint32_t previousClipID,
                     gpu::DrawContents previousClipDrawContents,
                     ResetAction);

    uint32_t previousClipID() const { return m_previousClipID; }

    void pushToRenderContext(RenderContext::LogicalFlush*,
                             int subpassIndex) override;

protected:
    const uint32_t m_previousClipID;
};
} // namespace rive::gpu
