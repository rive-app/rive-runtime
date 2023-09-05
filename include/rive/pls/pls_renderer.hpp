/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/pls/aligned_buffer.hpp"
#include "rive/pls/fixed_queue.hpp"
#include "rive/pls/pls.hpp"
#include "rive/pls/pls_render_context.hpp"
#include <vector>

namespace rive
{
class GrInnerFanTriangulator;
};

namespace rive::pls
{
class PLSPath;
class PLSPaint;
class PLSRenderContext;

// Renderer implementation for Rive's pixel local storage renderer.
class PLSRenderer : public Renderer
{
public:
    PLSRenderer(PLSRenderContext*);
    ~PLSRenderer() override;

    void save() override;
    void restore() override;
    void transform(const Mat2D& matrix) override;
    void drawPath(RenderPath*, RenderPaint*) override;
    void clipPath(RenderPath*) override;
    void drawImage(const RenderImage*, BlendMode, float opacity) override;
    void drawImageMesh(const RenderImage*,
                       rcp<RenderBuffer> vertices_f32,
                       rcp<RenderBuffer> uvCoords_f32,
                       rcp<RenderBuffer> indices_u16,
                       uint32_t vertexCount,
                       uint32_t indexCount,
                       BlendMode,
                       float opacity) override;

    // Determines if a path is an axis-aligned rectangle that can be represented by rive::AABB.
    static bool IsAABB(const RawPath&, AABB* result);

#ifdef TESTING
    bool hasClipRect() const { return m_stack.back().hasClipRect; }
    const AABB& getClipRect() const { return m_stack.back().clipRect; }
    const Mat2D& getClipRectMatrix() const { return m_stack.back().clipRectMatrix; }
#endif

private:
    class InteriorTriangulationHelper;

    void clipRect(AABB, PLSPath* originalPath);
    void clipPath(PLSPath*);

    // Implements drawPath() by pushing to m_context. Returns false if there was not enough room in
    // the GPU buffers to draw the path, at which point the caller mush flush before continuing.
    [[nodiscard]] bool pushPathDraw(PLSPath*, PLSPaint*);

    // Implements drawImageMesh() by pushing to m_context. Returns false if there was not enough
    // room in the GPU buffers to draw the path, at which point the caller mush flush before
    // continuing.
    [[nodiscard]] bool pushImageMeshDraw(const PLSTexture*,
                                         RenderBuffer* vertices_f32,
                                         RenderBuffer* uvCoords_f32,
                                         RenderBuffer* indices_u16,
                                         uint32_t vertexCount,
                                         uint32_t indexCount,
                                         BlendMode,
                                         float opacity);

    // Pushes any necessary clip updates to m_internalPathBatch and writes back the clipID the next
    // path should be drawn with.
    // Returns false if the operation failed, at which point the caller should flush and try again.
    [[nodiscard]] bool applyClip(uint32_t* clipID);

    // Pushes all paths in m_internalPathBatch to m_context.
    [[nodiscard]] bool pushInternalPathBatchToContext();

    struct ContourData
    {
        ContourData(const RawPath::Iter& endOfContour_,
                    size_t endLineIdx_,
                    size_t endCurveIdx_,
                    size_t endRotationIdx_,
                    Vec2D midpoint_,
                    bool closed_,
                    size_t strokeJoinCount_) :
            endOfContour(endOfContour_),
            endLineIdx(endLineIdx_),
            endCurveIdx(endCurveIdx_),
            endRotationIdx(endRotationIdx_),
            midpoint(midpoint_),
            closed(closed_),
            strokeJoinCount(strokeJoinCount_)
        {}
        RawPath::Iter endOfContour;
        size_t endLineIdx;
        size_t endCurveIdx;
        size_t endRotationIdx; // We measure rotations on both curves and round joins.
        Vec2D midpoint;
        bool closed;
        size_t strokeJoinCount;
        uint32_t strokeCapSegmentCount = 0;
        uint32_t paddingVertexCount = 0;
        RIVE_DEBUG_CODE(uint32_t tessVertexCount = 0;)
    };

    void pushContour(RawPath::Iter iter,
                     const ContourData&,
                     size_t curveIdx,
                     size_t rotationIdx,
                     float matrixMaxScale,
                     const PLSPaint* strokePaint);

    // Emulates a stroke cap before the given cubic by pushing a copy of the cubic, reversed, with 0
    // tessellation segments leading up to the join section, and a 180-degree join that looks like
    // the desired stroke cap.
    void pushEmulatedStrokeCapAsJoinBeforeCubic(const Vec2D cubic[],
                                                uint32_t emulatedCapAsJoinFlags,
                                                uint32_t strokeCapSegmentCount);

    struct RenderState
    {
        Mat2D matrix;
        size_t clipStackHeight = 0;
        AABB clipRect;
        Mat2D clipRectMatrix;
        pls::ClipRectInverseMatrix clipRectInverseMatrix;
        bool hasClipRect = false;
    };
    std::vector<RenderState> m_stack{1};

    struct ClipElement
    {
        ClipElement() = default;
        ClipElement(const Mat2D& matrix_, PLSPath* path_) { reset(matrix_, path_); }
        void reset(const Mat2D&, PLSPath*);
        bool isEquivalent(const Mat2D&, PLSPath*) const;

        Mat2D matrix;
        uint64_t pathUniqueID;
        RawPath path;
        AABB pathBounds;
        FillRule fillRule;
        uint32_t clipID;
    };
    std::vector<ClipElement> m_clipStack;
    uint64_t m_clipStackFlushID = -1; // Ensures we invalidate the clip stack after a flush.

    PLSRenderContext* const m_context;

    // Internal list of path draws that get pushed to the GPU in a single batch.
    struct PathDraw
    {
        PathDraw(const Mat2D* matrix_,
                 const RawPath* rawPath_,
                 const AABB& pathBounds_,
                 FillRule fillRule_,
                 pls::PaintType paintType_,
                 const PLSPaint* paint_,
                 uint32_t clipID_,
                 uint32_t outerClipID_);

        const Mat2D* matrix;
        const RawPath* rawPath;
        AABB pathBounds;
        FillRule fillRule;
        pls::PaintType paintType;
        const PLSPaint* paint;
        bool stroked;
        float strokeMatrixMaxScale;
        uint32_t clipID;      // ID to clip against if drawing a path;
                              // ID to write to the clip buffer if updating the clip.
        uint32_t outerClipID; // Ignored if drawing a path;
                              // 0 if drawing a top-level clip;
                              // ID to clip against if drawing a nested clip.
        GrInnerFanTriangulator* triangulator = nullptr; // Non-null if using interior triangulation.
        size_t firstContourIdx = 0;
        size_t contourCount = 0;
        uint32_t tessVertexCount = 0;
        uint32_t paddingVertexCount = 0;
        pls::PaintData paintRenderData; // Tells the GPU how to render the paint (or clip).
    };
    std::vector<PathDraw> m_internalPathBatch;

    // Temporary storage used during drawPath. Stored as a persistent class member to avoid
    // redundant mallocs.
    std::vector<ContourData> m_contourBatch;

    AlignedBuffer<4, float> m_parametricSegmentCounts_pow4;
    FixedQueue<uint8_t> m_numChops;
    FixedQueue<Vec2D> m_chops;
    AlignedBuffer<1, std::array<Vec2D, 2>> m_tangentPairs;
    AlignedBuffer<4, uint32_t> m_parametricSegmentCounts;
    AlignedBuffer<4, uint32_t> m_polarSegmentCounts;

    // Used to build coarse path interiors for the "interior triangulation" algorithm.
    RawPath m_scratchPath;

    // Consistency checks for pushContour.
    RIVE_DEBUG_CODE(size_t m_pushedLineCount;)
    RIVE_DEBUG_CODE(size_t m_pushedCurveCount;)
    RIVE_DEBUG_CODE(size_t m_pushedRotationCount;)
    RIVE_DEBUG_CODE(size_t m_pushedStrokeJoinCount;)
    RIVE_DEBUG_CODE(size_t m_pushedStrokeCapCount;)
    // Counts how many additional curves were pushed by pushEmulatedStrokeCapAsJoinBeforeCubic().
    RIVE_DEBUG_CODE(size_t m_pushedEmptyStrokeCountForCaps;)
};
} // namespace rive::pls
