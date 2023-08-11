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
                       BlendMode,
                       float opacity) override;

    // Most likely temporary. Determines if a path is an axis-aligned rectangle that can be
    // represented by the struct rive::AABB. Used to detect artboard clip candidates.
    static bool IsAABB(const RawPath&);

private:
    class InteriorTriangulationHelper;

    // Pushes any necessary clip updates to m_pathBatch and writes back the clipID the next path
    // should be drawn with.
    // Returns false if the operation failed, at which point the caller should flush and try again.
    [[nodiscard]] bool applyClip(uint32_t* clipID);

    // Pushes all paths in m_pathBatch to the GPU. All paths in the batch are assumed to be clip
    // updates except the final one, which is drawn with 'finalPathPaint'.
    [[nodiscard]] bool pushInternalPathBatch(PLSPaint* finalPathPaint);

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
        RenderState(const Mat2D& m, size_t h) : matrix(m), clipStackHeight(h) {}
        Mat2D matrix;
        size_t clipStackHeight;
    };
    std::vector<RenderState> m_stack{{Mat2D(), 0}};

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
    size_t m_clipStackHeight = 0;
    uint64_t m_clipStackFlushID = -1; // Ensures we invalidate the clip stack after a flush.
    bool m_hasArtboardClipCandidate = false;

    PLSRenderContext* const m_context;

    // Internal list of path draws that get pushed to the GPU in a single batch.
    struct PathDraw
    {
        PathDraw(const Mat2D* matrix_,
                 const RawPath* rawPath_,
                 const AABB& pathBounds_,
                 FillRule fillRule_,
                 uint32_t clipID_) :
            matrix(matrix_),
            rawPath(rawPath_),
            pathBounds(pathBounds_),
            fillRule(fillRule_),
            clipID(clipID_)
        {}
        const Mat2D* matrix;
        const RawPath* rawPath;
        AABB pathBounds;
        FillRule fillRule;
        uint32_t clipID;
        GrInnerFanTriangulator* triangulator = nullptr; // Non-null if using interior triangulation.
        size_t firstContourIdx = 0;
        size_t contourCount = 0;
        uint32_t tessVertexCount = 0;
        uint32_t paddingVertexCount = 0;
    };
    std::vector<PathDraw> m_pathBatch;

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
