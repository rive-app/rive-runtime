/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/renderer/gpu.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/fixed_queue.hpp"
#include "rive/shapes/paint/stroke_cap.hpp"
#include "rive/shapes/paint/stroke_join.hpp"
#include "rive/refcnt.hpp"

namespace rive::gpu
{
class PLSDraw;
class RiveRenderPath;
class RiveRenderPaint;
class PLSRenderContext;
class PLSGradient;

// High level abstraction of a single object to be drawn (path, imageRect, or imageMesh). These get
// built up for an entire frame in order to count GPU resource allocation sizes, and then sorted,
// batched, and drawn.
class PLSDraw
{
public:
    // Use a "fullscreen" bounding box that is reasonably larger than any screen, but not so big
    // that it runs the risk of overflowing.
    constexpr static IAABB kFullscreenPixelBounds = {0, 0, 1 << 24, 1 << 24};

    enum class Type : uint8_t
    {
        midpointFanPath,
        interiorTriangulationPath,
        imageRect,
        imageMesh,
        stencilClipReset,
    };

    PLSDraw(IAABB pixelBounds, const Mat2D&, BlendMode, rcp<const PLSTexture> imageTexture, Type);

    const PLSTexture* imageTexture() const { return m_imageTextureRef; }
    const IAABB& pixelBounds() const { return m_pixelBounds; }
    const Mat2D& matrix() const { return m_matrix; }
    BlendMode blendMode() const { return m_blendMode; }
    Type type() const { return m_type; }
    gpu::DrawContents drawContents() const { return m_drawContents; }
    bool isStroked() const { return m_drawContents & gpu::DrawContents::stroke; }
    bool isEvenOddFill() const { return m_drawContents & gpu::DrawContents::evenOddFill; }
    bool isOpaque() const { return m_drawContents & gpu::DrawContents::opaquePaint; }
    uint32_t clipID() const { return m_clipID; }
    bool hasClipRect() const { return m_clipRectInverseMatrix != nullptr; }
    const gpu::ClipRectInverseMatrix* clipRectInverseMatrix() const
    {
        return m_clipRectInverseMatrix;
    }
    gpu::SimplePaintValue simplePaintValue() const { return m_simplePaintValue; }
    const PLSGradient* gradient() const { return m_gradientRef; }

    // Clipping setup.
    void setClipID(uint32_t clipID);
    void setClipRect(const gpu::ClipRectInverseMatrix* m) { m_clipRectInverseMatrix = m; }

    // Used to allocate GPU resources for a collection of draws.
    using ResourceCounters = PLSRenderContext::LogicalFlush::ResourceCounters;
    const ResourceCounters& resourceCounts() const { return m_resourceCounts; }

    // Linked list of all PLSDraws within a gpu::DrawBatch.
    void setBatchInternalNeighbor(const PLSDraw* neighbor)
    {
        assert(m_batchInternalNeighbor == nullptr);
        m_batchInternalNeighbor = neighbor;
    };
    const PLSDraw* batchInternalNeighbor() const { return m_batchInternalNeighbor; }

    // Adds the gradient (if any) for this draw to the render context's gradient texture.
    // Returns false if this draw needed a gradient but there wasn't room for it in the texture, at
    // which point the gradient texture will need to be re-rendered mid flight.
    bool allocateGradientIfNeeded(PLSRenderContext::LogicalFlush*, ResourceCounters*);

    // Pushes the data for this draw to the render context. Called once the GPU buffers have been
    // counted and allocated, and the draws have been sorted.
    virtual void pushToRenderContext(PLSRenderContext::LogicalFlush*) = 0;

    // We can't have a destructor because we're block-allocated. Instead, the client calls this
    // method before clearing the drawList to release all our held references.
    virtual void releaseRefs();

protected:
    const PLSTexture* const m_imageTextureRef;
    const IAABB m_pixelBounds;
    const Mat2D m_matrix;
    const BlendMode m_blendMode;
    const Type m_type;

    uint32_t m_clipID = 0;
    const gpu::ClipRectInverseMatrix* m_clipRectInverseMatrix = nullptr;

    gpu::DrawContents m_drawContents = gpu::DrawContents::none;

    // Filled in by the subclass constructor.
    ResourceCounters m_resourceCounts;

    // Gradient data used by some draws. Stored in the base class so allocateGradientIfNeeded()
    // doesn't have to be virtual.
    const PLSGradient* m_gradientRef = nullptr;
    gpu::SimplePaintValue m_simplePaintValue;

    // Linked list of all PLSDraws within a gpu::DrawBatch.
    const PLSDraw* m_batchInternalNeighbor = nullptr;
};

// Implement PLSDrawReleaseRefs (defined in pls_render_context.hpp) now that PLSDraw is defined.
inline void PLSDrawReleaseRefs::operator()(PLSDraw* draw) { draw->releaseRefs(); }

// High level abstraction of a single path to be drawn (midpoint fan or interior triangulation).
class RiveRenderPathDraw : public PLSDraw
{
public:
    // Creates either a normal path draw or an interior triangulation if the path is large enough.
    static PLSDrawUniquePtr Make(PLSRenderContext*,
                                 const Mat2D&,
                                 rcp<const RiveRenderPath>,
                                 FillRule,
                                 const RiveRenderPaint*,
                                 RawPath* scratchPath);

    FillRule fillRule() const { return m_fillRule; }
    gpu::PaintType paintType() const { return m_paintType; }
    float strokeRadius() const { return m_strokeRadius; }
    gpu::ContourDirections contourDirections() const { return m_contourDirections; }

    void pushToRenderContext(PLSRenderContext::LogicalFlush*) final;

    void releaseRefs() override;

public:
    RiveRenderPathDraw(IAABB pathBounds,
                       const Mat2D&,
                       rcp<const RiveRenderPath>,
                       FillRule,
                       const RiveRenderPaint*,
                       Type,
                       gpu::InterlockMode);

    virtual void onPushToRenderContext(PLSRenderContext::LogicalFlush*) = 0;

    const RiveRenderPath* const m_pathRef;
    const FillRule
        m_fillRule; // Bc RiveRenderPath fillRule can mutate during the artboard draw process.
    const gpu::PaintType m_paintType;
    float m_strokeRadius = 0;
    gpu::ContourDirections m_contourDirections;

    // Used to guarantee m_pathRef doesn't change for the entire time we hold it.
    RIVE_DEBUG_CODE(uint64_t m_rawPathMutationID;)
};

// Draws a path by fanning tessellation patches around the midpoint of each contour.
class MidpointFanPathDraw : public RiveRenderPathDraw
{
public:
    MidpointFanPathDraw(PLSRenderContext*,
                        IAABB pixelBounds,
                        const Mat2D&,
                        rcp<const RiveRenderPath>,
                        FillRule,
                        const RiveRenderPaint*);

protected:
    void onPushToRenderContext(PLSRenderContext::LogicalFlush*) override;

    // Emulates a stroke cap before the given cubic by pushing a copy of the cubic, reversed, with 0
    // tessellation segments leading up to the join section, and a 180-degree join that looks like
    // the desired stroke cap.
    void pushEmulatedStrokeCapAsJoinBeforeCubic(PLSRenderContext::LogicalFlush*,
                                                const Vec2D cubic[],
                                                uint32_t emulatedCapAsJoinFlags,
                                                uint32_t strokeCapSegmentCount);

    float m_strokeMatrixMaxScale;
    StrokeJoin m_strokeJoin;
    StrokeCap m_strokeCap;

    struct ContourInfo
    {
        RawPath::Iter endOfContour;
        size_t endLineIdx;
        size_t firstCurveIdx;
        size_t endCurveIdx;
        size_t firstRotationIdx; // We measure rotations on both curves and round joins.
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

    // Consistency checks for onPushToRenderContext().
    RIVE_DEBUG_CODE(size_t m_pendingLineCount;)
    RIVE_DEBUG_CODE(size_t m_pendingCurveCount;)
    RIVE_DEBUG_CODE(size_t m_pendingRotationCount;)
    RIVE_DEBUG_CODE(size_t m_pendingStrokeJoinCount;)
    RIVE_DEBUG_CODE(size_t m_pendingStrokeCapCount;)
    // Counts how many additional curves were pushed by pushEmulatedStrokeCapAsJoinBeforeCubic().
    RIVE_DEBUG_CODE(size_t m_pendingEmptyStrokeCountForCaps;)
};

// Draws a path by triangulating the interior into non-overlapping triangles and tessellating the
// outer curves.
class InteriorTriangulationDraw : public RiveRenderPathDraw
{
public:
    enum class TriangulatorAxis
    {
        horizontal,
        vertical,
        dontCare,
    };

    InteriorTriangulationDraw(PLSRenderContext*,
                              IAABB pixelBounds,
                              const Mat2D&,
                              rcp<const RiveRenderPath>,
                              FillRule,
                              const RiveRenderPaint*,
                              RawPath* scratchPath,
                              TriangulatorAxis);

    GrInnerFanTriangulator* triangulator() const { return m_triangulator; }

protected:
    void onPushToRenderContext(PLSRenderContext::LogicalFlush*) override;

    // The final segment in an outerCurve patch is a bowtie join.
    constexpr static size_t kJoinSegmentCount = 1;
    constexpr static size_t kPatchSegmentCountExcludingJoin =
        kOuterCurvePatchSegmentSpan - kJoinSegmentCount;

    // Maximum # of outerCurve patches a curve on the path can be subdivided into.
    constexpr static size_t kMaxCurveSubdivisions =
        (kMaxParametricSegments + kPatchSegmentCountExcludingJoin - 1) /
        kPatchSegmentCountExcludingJoin;

    static uint32_t FindSubdivisionCount(const Vec2D pts[],
                                         const wangs_formula::VectorXform& vectorXform)
    {
        float numSubdivisions = ceilf(wangs_formula::cubic(pts, kParametricPrecision, vectorXform) *
                                      (1.f / kPatchSegmentCountExcludingJoin));
        return static_cast<uint32_t>(math::clamp(numSubdivisions, 1, kMaxCurveSubdivisions));
    }

    enum class PathOp : bool
    {
        countDataAndTriangulate,
        submitOuterCubics,
    };

    // For now, we just iterate and subdivide the path twice (once for each enum in PathOp).
    // Since we only do this for large paths, and since we're triangulating the path interior
    // anyway, adding complexity to only run Wang's formula and chop once would save about ~5%
    // of the total CPU time. (And large paths are GPU-bound anyway.)
    void processPath(PathOp op,
                     TrivialBlockAllocator*,
                     RawPath* scratchPath,
                     TriangulatorAxis,
                     PLSRenderContext::LogicalFlush*);

    GrInnerFanTriangulator* m_triangulator = nullptr;
};

// Pushes an imageRect to the render context.
// This should only be used when we don't have bindless textures in atomic mode. Otherwise, images
// should be drawn as rectangular paths with an image paint.
class ImageRectDraw : public PLSDraw
{
public:
    ImageRectDraw(PLSRenderContext*,
                  IAABB pixelBounds,
                  const Mat2D&,
                  BlendMode,
                  rcp<const PLSTexture>,
                  float opacity);

    float opacity() const { return m_opacity; }

    void pushToRenderContext(PLSRenderContext::LogicalFlush*) override;

protected:
    const float m_opacity;
};

// Pushes an imageMesh to the render context.
class ImageMeshDraw : public PLSDraw
{
public:
    ImageMeshDraw(IAABB pixelBounds,
                  const Mat2D&,
                  BlendMode,
                  rcp<const PLSTexture>,
                  rcp<const RenderBuffer> vertexBuffer,
                  rcp<const RenderBuffer> uvBuffer,
                  rcp<const RenderBuffer> indexBuffer,
                  uint32_t indexCount,
                  float opacity);

    const RenderBuffer* vertexBuffer() const { return m_vertexBufferRef; }
    const RenderBuffer* uvBuffer() const { return m_uvBufferRef; }
    const RenderBuffer* indexBuffer() const { return m_indexBufferRef; }
    uint32_t indexCount() const { return m_indexCount; }
    float opacity() const { return m_opacity; }

    void pushToRenderContext(PLSRenderContext::LogicalFlush*) override;

    void releaseRefs() override;

protected:
    const RenderBuffer* const m_vertexBufferRef;
    const RenderBuffer* const m_uvBufferRef;
    const RenderBuffer* const m_indexBufferRef;
    const uint32_t m_indexCount;
    const float m_opacity;
};

// Resets the stencil clip by either entirely erasing the existing clip, or intersecting it with a
// nested clip (i.e., erasing the region outside the nested clip).
class StencilClipReset : public PLSDraw
{
public:
    enum class ResetAction
    {
        clearPreviousClip,
        intersectPreviousClip,
    };

    StencilClipReset(PLSRenderContext*, uint32_t previousClipID, ResetAction);

    uint32_t previousClipID() const { return m_previousClipID; }

    void pushToRenderContext(PLSRenderContext::LogicalFlush*) override;

protected:
    const uint32_t m_previousClipID;
};
} // namespace rive::gpu
