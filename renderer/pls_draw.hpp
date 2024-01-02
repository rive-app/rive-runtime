/*
 * Copyright 2023 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_context.hpp"
#include "rive/pls/pls_render_context_impl.hpp"
#include "rive/pls/pls_renderer.hpp"
#include "rive/pls/fixed_queue.hpp"

namespace rive::pls
{
class PLSPath;
class PLSPaint;

// High level abstraction of a single object to be drawn (path, imageRect, or imageMesh). These get
// built up for an entire frame in order to count GPU resource allocation sizes, and then sorted,
// batched, and drawn.
class PLSDraw
{
public:
    PLSDraw(const Mat2D&, const AABB& bounds, BlendMode, rcp<const PLSTexture> imageTexture);

    // Clipping setup.
    void setClipID(uint32_t clipID) { m_clipID = clipID; }
    void setClipRect(const pls::ClipRectInverseMatrix* m) { m_clipRectInverseMatrix = m; }

    // Running counter of GPU resource allocation sizes for an entire frame. This gets sent to
    // countResources() for every draw in a frame.
    struct ResourceCounters
    {
        ResourceCounters(PLSRenderContext* context) : tessVertexCounter(context) {}

        PLSRenderContext::TessVertexCounter tessVertexCounter;
        size_t pathCount = 0;
        size_t contourCount = 0;
        size_t curveCount = 0;
        size_t imageDrawUniformsCount = 0;
    };

    virtual void countResources(ResourceCounters*) = 0;

    // Adds the gradient (if any) for this draw to the render context's gradient texture.
    // Returns false if this draw needed a gradient but there wasn't room for it in the texture, at
    // which point the gradient texture will need to be re-rendered mid flight.
    bool pushGradientIfNeeded(PLSRenderContext* context)
    {
        return m_gradientRef == nullptr || context->pushGradient(m_gradientRef, &m_paintRenderData);
    }

    // Pushes the data for this draw to the render context. Called once the GPU buffers have been
    // counted and allocated, and the draws have been sorted.
    virtual void pushToRenderContext(PLSRenderContext*) = 0;

    // We can't have a destructor because we're block-allocated. Instead, the client calls this
    // method before clearing the drawList to release all our held references.
    virtual void releaseRefs();

protected:
    const Mat2D m_matrix;
    const AABB m_bounds;
    const BlendMode m_blendMode;
    const PLSTexture* const m_imageTextureRef;

    uint32_t m_clipID = 0;
    const pls::ClipRectInverseMatrix* m_clipRectInverseMatrix = nullptr;

    // Gradient data used by some draws. Stored in the base class so pushGradientIfNeeded() doesn't
    // have to be virtual.
    const PLSGradient* m_gradientRef = nullptr;
    pls::PaintData m_paintRenderData;
};

// High level abstraction of a single path to be drawn (midpoint fan or interior triangulation).
class PLSPathDraw : public PLSDraw
{
public:
    // Creates either a normal path draw or an interior triangulation if the path is large enough.
    static PLSPathDraw* Make(PLSRenderContext*,
                             const Mat2D&,
                             rcp<const PLSPath>,
                             FillRule,
                             const PLSPaint*,
                             RawPath* scratchPath);

    void pushToRenderContext(PLSRenderContext*) final;

    void releaseRefs() override;

protected:
    PLSPathDraw(const Mat2D&,
                const AABB& bounds,
                rcp<const PLSPath>,
                FillRule,
                const PLSPaint*,
                pls::PatchType);

    virtual void onPushToRenderContext(PLSRenderContext*) = 0;

    const PLSPath* const m_pathRef;
    const FillRule m_fillRule; // Bc PLSPath fillRule can mutate during the artboard draw process.
    const pls::PaintType m_paintType;
    const bool m_isStroked;
    const float m_strokeRadius;

    const pls::PatchType m_patchType;
    size_t m_contourCount = 0;
    uint32_t m_tessVertexCount = 0;
    size_t m_paddingVertexCount = 0;
};

// Draws a path by fanning tessellation patches around the midpoint of each contour.
class MidpointFanPathDraw : public PLSPathDraw
{
public:
    MidpointFanPathDraw(PLSRenderContext*,
                        const Mat2D&,
                        const AABB& bounds,
                        rcp<const PLSPath>,
                        FillRule,
                        const PLSPaint*);

    void countResources(ResourceCounters*) override;

protected:
    void onPushToRenderContext(PLSRenderContext*) override;

    // Emulates a stroke cap before the given cubic by pushing a copy of the cubic, reversed, with 0
    // tessellation segments leading up to the join section, and a 180-degree join that looks like
    // the desired stroke cap.
    void pushEmulatedStrokeCapAsJoinBeforeCubic(PLSRenderContext*,
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
    size_t m_curveCount = 0;

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
class InteriorTriangulationDraw : public PLSPathDraw
{
public:
    InteriorTriangulationDraw(PLSRenderContext*,
                              const Mat2D&,
                              const AABB& bounds,
                              rcp<const PLSPath>,
                              FillRule,
                              const PLSPaint*,
                              RawPath* scratchPath);

    void countResources(ResourceCounters*) override;

protected:
    void onPushToRenderContext(PLSRenderContext*) override;

    // The final segment in an outerCurve patch is a bowtie join.
    constexpr static size_t kJoinSegmentCount = 1;
    constexpr static size_t kPatchSegmentCountExcludingJoin =
        kOuterCurvePatchSegmentSpan - kJoinSegmentCount;

    // Maximum # of outerCurve patches a curve on the path can be subdivided into.
    constexpr static size_t kMaxCurveSubdivisions =
        (kMaxParametricSegments + kPatchSegmentCountExcludingJoin - 1) /
        kPatchSegmentCountExcludingJoin;

    static size_t FindSubdivisionCount(const Vec2D pts[],
                                       const wangs_formula::VectorXform& vectorXform)
    {
        size_t numSubdivisions =
            ceilf(wangs_formula::cubic(pts, kParametricPrecision, vectorXform) *
                  (1.f / kPatchSegmentCountExcludingJoin));
        return std::clamp<size_t>(numSubdivisions, 1, kMaxCurveSubdivisions);
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
    void processPath(PLSRenderContext* context, PathOp op, RawPath* scratchPath = nullptr);

    size_t m_patchCount = 0;
    GrInnerFanTriangulator* m_triangulator = nullptr;
};

// Pushes an imageRect to the render context.
// This should only be used when we don't have bindless textures in atomic mode. Otherwise, images
// should be drawn as rectangular paths with an image paint.
class ImageRectDraw : public PLSDraw
{
public:
    ImageRectDraw(PLSRenderContext* context,
                  const Mat2D& matrix,
                  const AABB& bounds,
                  BlendMode blendMode,
                  rcp<const PLSTexture> imageTexture,
                  float opacity) :
        PLSDraw(matrix, bounds, blendMode, std::move(imageTexture)), m_opacity(opacity)
    {
        // If we support image paints for paths, the client should draw a rectangular path with an
        // image paint instead of using this draw.
        assert(!context->impl()->platformFeatures().supportsBindlessTextures);
        assert(context->frameDescriptor().enableExperimentalAtomicMode);
    }

    void countResources(ResourceCounters* counters) override { ++counters->imageDrawUniformsCount; }

    void pushToRenderContext(PLSRenderContext*) override;

protected:
    const float m_opacity;
};

// Pushes an imageMesh to the render context.
class ImageMeshDraw : public PLSDraw
{
public:
    ImageMeshDraw(const Mat2D& matrix,
                  const AABB& bounds,
                  BlendMode blendMode,
                  rcp<const PLSTexture> imageTexture,
                  rcp<const RenderBuffer> vertexBuffer,
                  rcp<const RenderBuffer> uvBuffer,
                  rcp<const RenderBuffer> indexBuffer,
                  uint32_t indexCount,
                  float opacity) :
        PLSDraw(matrix, bounds, blendMode, std::move(imageTexture)),
        m_vertexBufferRef(vertexBuffer.release()),
        m_uvBufferRef(uvBuffer.release()),
        m_indexBufferRef(indexBuffer.release()),
        m_indexCount(indexCount),
        m_opacity(opacity)
    {
        assert(m_vertexBufferRef != nullptr);
        assert(m_uvBufferRef != nullptr);
        assert(m_indexBufferRef != nullptr);
    }

    void countResources(ResourceCounters* counters) override { ++counters->imageDrawUniformsCount; }

    void pushToRenderContext(PLSRenderContext*) override;

    void releaseRefs() override;

protected:
    const RenderBuffer* const m_vertexBufferRef;
    const RenderBuffer* const m_uvBufferRef;
    const RenderBuffer* const m_indexBufferRef;
    const uint32_t m_indexCount;
    const float m_opacity;
};
} // namespace rive::pls
