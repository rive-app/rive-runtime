/*
 * Copyright 2022 Rive
 */

#include "rive/pls/pls_renderer.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "path_utils.hpp"
#include "pls_paint.hpp"
#include "pls_path.hpp"
#include "rive/math/math_types.hpp"
#include "rive/math/simd.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/pls/pls_image.hpp"

namespace rive::pls
{
constexpr static int kNumSegmentsInMiterOrBevelJoin = 5;

void PLSRenderer::ClipElement::reset(const Mat2D& matrix_, PLSPath* path_)
{
    matrix = matrix_;
    pathUniqueID = path_->getUniqueID();
    path = path_->getRawPath();
    pathBounds = path_->getBounds();
    fillRule = path_->getFillRule();
    clipID = 0; // This gets initialized lazily.
}

bool PLSRenderer::ClipElement::isEquivalent(const Mat2D& matrix_, PLSPath* path_) const
{
    return matrix_ == matrix && path_->getUniqueID() == pathUniqueID;
}

PLSRenderer::PLSRenderer(PLSRenderContext* context) : m_context(context) {}

PLSRenderer::~PLSRenderer() {}

void PLSRenderer::save()
{
    // Copy the back of the stack before pushing, in case the vector grows and invalidates the
    // reference.
    RenderState copy = m_stack.back();
    m_stack.push_back(copy);
}

void PLSRenderer::restore()
{
    assert(!m_stack.empty());
    assert(m_stack.back().clipStackHeight >= m_stack.back().clipStackHeight);
    m_stack.pop_back();
}

void PLSRenderer::transform(const Mat2D& matrix)
{
    m_stack.back().matrix = m_stack.back().matrix * matrix;
}

bool PLSRenderer::applyClip(uint32_t* clipID)
{
    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    if (clipStackHeight == 0)
    {
        *clipID = 0;
        return true;
    }

    // Generate new clipIDs (definitely causing us to redraw the clip buffer) for each element that
    // hasn't been assigned an ID yet, OR if we are on a new flush. (New flushes invalidate the clip
    // buffer.)
    bool needsClipInvalidate = m_clipStackFlushID != m_context->getFlushCount();
    // Also detect which clip element (if any) is already rendered in the clip buffer.
    size_t clipIdxCurrentlyInClipBuffer = -1; // i.e., "none".
    for (size_t i = 0; i < clipStackHeight; ++i)
    {
        ClipElement& clip = m_clipStack[i];
        if (clip.clipID == 0 || needsClipInvalidate)
        {
            clip.clipID = m_context->generateClipID();
            assert(m_context->getClipContentID() != clip.clipID);
            if (clip.clipID == 0)
            {
                return false; // The context is out of clipIDs. We will flush and try again.
            }
            continue;
        }
        if (clip.clipID == m_context->getClipContentID())
        {
            // This is the clip that's currently drawn in the clip buffer! We should only find a
            // match once, so clipIdxCurrentlyInClipBuffer better be at the initial "none" value at
            // this point.
            assert(clipIdxCurrentlyInClipBuffer == -1);
            clipIdxCurrentlyInClipBuffer = i;
        }
    }

    // Draw the necessary updates to the clip buffer (i.e., draw every clip element after
    // clipIdxCurrentlyInClipBuffer).
    uint32_t outerClipID = clipIdxCurrentlyInClipBuffer == -1
                               ? 0 // The next clip to be drawn is not nested.
                               : m_clipStack[clipIdxCurrentlyInClipBuffer].clipID;
    for (size_t i = clipIdxCurrentlyInClipBuffer + 1; i < clipStackHeight; ++i)
    {
        const ClipElement& clip = m_clipStack[i];
        assert(clip.clipID != 0);
        m_pathBatch.emplace_back(&clip.matrix,
                                 &clip.path,
                                 clip.pathBounds,
                                 clip.fillRule,
                                 clip.clipID,
                                 outerClipID);
        outerClipID = clip.clipID; // Nest the next clip (if any) inside the one we just rendered.
    }
    *clipID = m_clipStack[clipStackHeight - 1].clipID;
    m_context->setClipContentID(*clipID);
    m_clipStackFlushID = m_context->getFlushCount();
    return true;
}

void PLSRenderer::drawPath(RenderPath* renderPath, RenderPaint* renderPaint)
{
    PLSPath* path = static_cast<PLSPath*>(renderPath);
    PLSPaint* paint = static_cast<PLSPaint*>(renderPaint);

    bool stroked = paint->getIsStroked();
    if (stroked && m_context->frameDescriptor().strokesDisabled)
    {
        return;
    }
    if (!stroked && m_context->frameDescriptor().fillsDisabled)
    {
        return;
    }

    // A stroke width of zero means a path is filled in PLS.
    if (stroked && paint->getThickness() <= 0)
    {
        return;
    }

    // Make (up to) two attempts to draw the path plus any necessary clip updates in a single batch.
    // If the first attempt fails, flush to make room and try again.
    for (size_t i = 0; i < 2; ++i)
    {
        m_pathBatch.clear();
        uint32_t clipID;
        if (!applyClip(&clipID))
        {
            m_context->flush(PLSRenderContext::FlushType::intermediate);
            continue;
        }
        m_pathBatch.emplace_back(&m_stack.back().matrix,
                                 &path->getRawPath(),
                                 path->getBounds(),
                                 path->getFillRule(),
                                 clipID,
                                 0);
        if (!pushInternalPathBatch(paint))
        {
            m_context->flush(PLSRenderContext::FlushType::intermediate);
            continue;
        }
        return;
    }

    fprintf(
        stderr,
        "PLSRenderer::drawPath failed. The path and/or clip stack and/or paint are too complex.\n");
}

void PLSRenderer::clipPath(RenderPath* renderPath)
{
    PLSPath* plsPath = static_cast<PLSPath*>(renderPath);

    // First try to handle axis-aligned rectangles using the "ENABLE_CLIP_RECT" shader feature.
    // Multiple axis-aligned rectangles can be intersected into a single rectangle if their matrices
    // are compatible.
    AABB clipRectCandidate;
    if (IsAABB(plsPath->getRawPath(), &clipRectCandidate))
    {
        clipRect(clipRectCandidate, plsPath);
    }
    else
    {
        clipPath(plsPath);
    }
}

// Finds a new rect, if such a rect exists, such that:
//
//     currentMatrix * rect == newMatrix * newRect
//
// Returns true if *rect was replaced with newRect.
static bool transform_rect_to_new_space(AABB* rect,
                                        const Mat2D& currentMatrix,
                                        const Mat2D& newMatrix)
{
    if (currentMatrix == newMatrix)
    {
        return true;
    }
    Mat2D currentToNew;
    if (!newMatrix.invert(&currentToNew))
    {
        return false;
    }
    currentToNew = currentToNew * currentMatrix;
    float maxSkew = std::max(fabsf(currentToNew.xy()), fabsf(currentToNew.yx()));
    float maxScale = std::max(fabsf(currentToNew.xx()), fabsf(currentToNew.yy()));
    if (maxSkew > math::EPSILON && maxScale > math::EPSILON)
    {
        // Transforming this rect to the new view matrix would turn it into something that isn't a
        // rect.
        return false;
    }
    Vec2D pts[2] = {{rect->left(), rect->top()}, {rect->right(), rect->bottom()}};
    currentToNew.mapPoints(pts, pts, 2);
    float4 p = simd::load4f(pts);
    float2 topLeft = simd::min(p.xy, p.zw);
    float2 botRight = simd::max(p.xy, p.zw);
    *rect = {topLeft.x, topLeft.y, botRight.x, botRight.y};
    return true;
}

void PLSRenderer::clipRect(AABB rect, PLSPath* originalPath)
{
    // If there already is a clipRect, we can only accept another one by intersecting it with the
    // existing one. This means the new rect must be axis-aligned with the existing clipRect.
    if (m_stack.back().hasClipRect &&
        !transform_rect_to_new_space(&rect, m_stack.back().matrix, m_stack.back().clipRectMatrix))
    {
        // 'rect' is not axis-aligned with the existing clipRect. Fall back to clipPath.
        clipPath(originalPath);
        return;
    }

    if (!m_stack.back().hasClipRect)
    {
        // There wasn't an existing clipRect. This is the one!
        m_stack.back().clipRect = rect;
        m_stack.back().clipRectMatrix = m_stack.back().matrix;
        m_stack.back().hasClipRect = true;
    }
    else
    {
        // Both rects are in the same space now. Intersect the two geometrically.
        float4 a = simd::load4f(&m_stack.back().clipRect);
        float4 b = simd::load4f(&rect);
        float4 intersection = simd::join(simd::max(a.xy, b.xy), simd::min(a.zw, b.zw));
        simd::store(&m_stack.back().clipRect, intersection);
    }

    // Find the matrix that transforms from pixel space to "normalized clipRect space", where the
    // clipRect is the normalized rectangle: [-1, -1, +1, +1].
    const AABB& clipRect = m_stack.back().clipRect;
    Mat2D m = m_stack.back().clipRectMatrix * Mat2D(clipRect.width() * .5f,
                                                    0,
                                                    0,
                                                    clipRect.height() * .5f,
                                                    clipRect.center().x,
                                                    clipRect.center().y);
    if (clipRect.width() <= 0 || clipRect.height() <= 0 ||
        !m.invert(&m_stack.back().pixelToNormalizedClipRect))
    {
        // If the width or height went zero or negative, or if m is non-invertible, force the
        // clipRect coverage to 0. (The shader uses tx and ty as uniform clipRect coverage if the
        // matrix is singular.)
        m_stack.back().pixelToNormalizedClipRect = {0, 0, 0, 0, 0, 0};
    }
}

void PLSRenderer::clipPath(PLSPath* path)
{
    // Only write a new clip element if this path isn't already on the stack from before. e.g.:
    //
    //     clipPath(samePath);
    //     restore();
    //     save();
    //     clipPath(samePath); // <-- reuse the ClipElement (and clipID!) already in m_clipStack.
    //
    const size_t clipStackHeight = m_stack.back().clipStackHeight;
    assert(m_clipStack.size() >= clipStackHeight);
    if (m_clipStack.size() == clipStackHeight ||
        !m_clipStack[clipStackHeight].isEquivalent(m_stack.back().matrix, path))
    {
        m_clipStack.resize(clipStackHeight);
        m_clipStack.emplace_back(m_stack.back().matrix, path);
    }
    m_stack.back().clipStackHeight = clipStackHeight + 1;
}

void PLSRenderer::drawImage(const RenderImage* renderImage, BlendMode blendMode, float opacity)
{
    // Implement drawImage() as drawPath() with a rectangle path and an image paint.
    save();

    auto image = static_cast<const PLSImage*>(renderImage);
    scale(image->width(), image->height());

    PLSPath path;
    path.line({1, 0});
    path.line({1, 1});
    path.line({0, 1});

    PLSPaint paint;
    paint.image(image->refTexture(m_context->impl()), opacity);
    paint.blendMode(blendMode);

    drawPath(&path, &paint);

    restore();
}

void PLSRenderer::drawImageMesh(const RenderImage*,
                                rcp<RenderBuffer> vertices_f32,
                                rcp<RenderBuffer> uvCoords_f32,
                                rcp<RenderBuffer> indices_u16,
                                BlendMode,
                                float opacity)
{}

namespace
{
constexpr static int kStrokeStyleFlag = 8;
constexpr static int kRoundJoinStyleFlag = kStrokeStyleFlag << 1;
RIVE_ALWAYS_INLINE constexpr int style_flags(bool stroked, bool roundJoinStroked)
{
    int styleFlags = (stroked << 3) | (roundJoinStroked << 4);
    assert(bool(styleFlags & kStrokeStyleFlag) == stroked);
    assert(bool(styleFlags & kRoundJoinStyleFlag) == roundJoinStroked);
    return styleFlags;
}

// Switching on a StyledVerb reduces "if (stroked)" branching and makes the code cleaner.
enum class StyledVerb
{
    filledMove = static_cast<int>(PathVerb::move),
    strokedMove = kStrokeStyleFlag | static_cast<int>(PathVerb::move),
    roundJoinStrokedMove =
        kStrokeStyleFlag | kRoundJoinStyleFlag | static_cast<int>(PathVerb::move),

    filledLine = static_cast<int>(PathVerb::line),
    strokedLine = kStrokeStyleFlag | static_cast<int>(PathVerb::line),
    roundJoinStrokedLine =
        kStrokeStyleFlag | kRoundJoinStyleFlag | static_cast<int>(PathVerb::line),

    filledQuad = static_cast<int>(PathVerb::quad),
    strokedQuad = kStrokeStyleFlag | static_cast<int>(PathVerb::quad),
    roundJoinStrokedQuad =
        kStrokeStyleFlag | kRoundJoinStyleFlag | static_cast<int>(PathVerb::quad),

    filledCubic = static_cast<int>(PathVerb::cubic),
    strokedCubic = kStrokeStyleFlag | static_cast<int>(PathVerb::cubic),
    roundJoinStrokedCubic =
        kStrokeStyleFlag | kRoundJoinStyleFlag | static_cast<int>(PathVerb::cubic),

    filledClose = static_cast<int>(PathVerb::close),
    strokedClose = kStrokeStyleFlag | static_cast<int>(PathVerb::close),
    roundJoinStrokedClose =
        kStrokeStyleFlag | kRoundJoinStyleFlag | static_cast<int>(PathVerb::close),
};
RIVE_ALWAYS_INLINE constexpr StyledVerb styled_verb(PathVerb verb, int styleFlags)
{
    return static_cast<StyledVerb>(styleFlags | static_cast<int>(verb));
}

// When chopping strokes, switching on a "chop_key" reduces "if (areCusps)" branching and makes the
// code cleaner.
RIVE_ALWAYS_INLINE constexpr uint8_t chop_key(bool areCusps, uint8_t numChops)
{
    return (numChops << 1) | static_cast<uint8_t>(areCusps);
}
RIVE_ALWAYS_INLINE constexpr uint8_t cusp_chop_key(uint8_t n) { return chop_key(true, n); }
RIVE_ALWAYS_INLINE constexpr uint8_t simple_chop_key(uint8_t n) { return chop_key(false, n); }

// Produces a cubic equivalent to the given line, for which Wang's formula also returns 1.
RIVE_ALWAYS_INLINE std::array<Vec2D, 4> convert_line_to_cubic(const Vec2D line[2])
{
    float4 endPts = simd::load4f(line);
    float4 controlPts = simd::mix(endPts, endPts.zwxy, float4(1 / 3.f));
    std::array<Vec2D, 4> cubic;
    cubic[0] = line[0];
    simd::store(&cubic[1], controlPts);
    cubic[3] = line[1];
    return cubic;
}
RIVE_ALWAYS_INLINE std::array<Vec2D, 4> convert_line_to_cubic(Vec2D p0, Vec2D p1)
{
    Vec2D line[2] = {p0, p1};
    return convert_line_to_cubic(line);
}

// Finds the tangents of the curve at T=0 and T=1 respectively.
RIVE_ALWAYS_INLINE Vec2D find_cubic_tan0(const Vec2D p[4])
{
    Vec2D tan0 = (p[0] != p[1] ? p[1] : p[1] != p[2] ? p[2] : p[3]) - p[0];
    // RawPath should have discarded empty cubics, and FindCubicConvex180Chops should have enough
    // slop to not produce empty chops.
    assert((tan0 != Vec2D{0, 0}));
    return tan0;
}
RIVE_ALWAYS_INLINE Vec2D find_cubic_tan1(const Vec2D p[4])
{
    Vec2D tan1 = p[3] - (p[3] != p[2] ? p[2] : p[2] != p[1] ? p[1] : p[0]);
    // RawPath should have discarded empty cubics, and FindCubicConvex180Chops should have enough
    // slop to not produce empty chops.
    assert((tan1 != Vec2D{0, 0}));
    return tan1;
}
RIVE_ALWAYS_INLINE void find_cubic_tangents(const Vec2D p[4], Vec2D tangents[2])
{
    tangents[0] = find_cubic_tan0(p);
    tangents[1] = find_cubic_tan1(p);
}

// Chops a cubic into 2 * n + 1 segments, surrounding each cusp. The resulting cubics will be
// visually equivalent to the original when stroked, but the cusp won't have artifacts when rendered
// using the parametric/polar sorting algorithm.
//
// The size of dst[] must be 6 * n + 4 Vec2Ds.
static void chop_cubic_around_cusps(const Vec2D p[4],
                                    Vec2D dst[/*6 * n + 4*/],
                                    const float cuspT[],
                                    int n,
                                    float matrixMaxScale)
{
    float t[4];
    assert(n * 2 <= std::size(t));
    // Generate chop points straddling each cusp with padding. This creates buffer space around the
    // cusp that protects against fp32 precision issues.
    for (int i = 0; i < n; ++i)
    {
        // If the cusps are extremely close together, don't allow the straddle points to cross.
        float minT = i == 0 ? 0.f : (cuspT[i - 1] + cuspT[i]) * .5f;
        float maxT = i + 1 == n ? 1.f : (cuspT[i + 1] + cuspT[i]) * .5f;
        t[i * 2 + 0] = std::max(cuspT[i] - math::EPSILON, minT);
        t[i * 2 + 1] = std::min(cuspT[i] + math::EPSILON, maxT);
    }
    pathutils::ChopCubicAt(p, dst, t, n * 2);
    for (int i = 0; i < n; ++i)
    {
        // Find the three chops at this cusp.
        Vec2D* chops = dst + i * 6;
        // Correct the chops to fall on the actual cusp point.
        Vec2D cusp = pathutils::EvalCubicAt(p, cuspT[i]);
        chops[3] = chops[6] = cusp;
        // The only purpose of the middle cubic is to capture the cusp's 180-degree rotation.
        // Implement it as a sub-pixel 180-degree pivot.
        Vec2D pivot = (chops[2] + chops[7]) * .5f;
        pivot = (cusp - pivot).normalized() / (matrixMaxScale * kPolarPrecision * 2) + cusp;
        chops[4] = chops[5] = pivot;
    }
}

// Finds the starting tangent in a contour composed of the points [pts, end). If all points are
// equal, generates a tangent pointing horizontally to the right.
static Vec2D find_starting_tangent(const Vec2D pts[], const Vec2D* end)
{
    assert(end > pts);
    const Vec2D p0 = pts[0];
    while (++pts < end)
    {
        Vec2D p = *pts;
        if (p != p0)
        {
            return p - p0;
        }
    }
    return {1, 0};
}

// Finds the ending tangent in a contour composed of the points [pts, end). If all points are equal,
// generates a tangent pointing horizontally to the left.
static Vec2D find_ending_tangent(const Vec2D pts[], const Vec2D* end)
{
    assert(end > pts);
    const Vec2D endpoint = end[-1];
    while (--end > pts)
    {
        Vec2D p = end[-1];
        if (p != endpoint)
        {
            return endpoint - p;
        }
    }
    return {-1, 0};
}

static Vec2D find_join_tangent_full_impl(const Vec2D* joinPoint,
                                         const Vec2D* end,
                                         bool closed,
                                         const Vec2D* p0)
{
    // Find the first point in the contour not equal to *joinPoint and return the difference.
    // RawPath should have discarded empty verbs, so this should be a fast operation.
    for (const Vec2D* p = joinPoint + 1; p != end; ++p)
    {
        if (*p != *joinPoint)
        {
            return *p - *joinPoint;
        }
    }
    if (closed)
    {
        for (const Vec2D* p = p0; p != joinPoint; ++p)
        {
            if (*p != *joinPoint)
            {
                return *p - *joinPoint;
            }
        }
    }
    // This should never be reached because RawPath discards empty verbs.
    RIVE_UNREACHABLE();
}

RIVE_ALWAYS_INLINE Vec2D find_join_tangent(const Vec2D* joinPoint,
                                           const Vec2D* end,
                                           bool closed,
                                           const Vec2D* p0)
{
    // Quick early out for inlining and branch prediction: The next point in the contour is almost
    // always the point that determines the join tangent.
    const Vec2D* nextPoint = joinPoint + 1;
    nextPoint = nextPoint != end ? nextPoint : p0;
    Vec2D tangent = *nextPoint - *joinPoint;
    return tangent != Vec2D{0, 0} ? tangent
                                  : find_join_tangent_full_impl(joinPoint, end, closed, p0);
}

// Should an empty stroke emit round caps, square caps, or none?
//
// Just pick the cap type that makes the most sense for a contour that animates from non-empty to
// empty:
//
//   * A non-closed contour with round caps and a CLOSED contour with round JOINS both converge to a
//     circle when animated to empty.
//         => round caps on the empty contour.
//
//   * A non-closed contour with square caps converges to a square (albeit with potential rotation
//     that is lost when the contour becomes empty).
//         => square caps on the empty contour.
//
//   * A closed contour with miter JOINS converges to... some sort of polygon with pointy corners.
//         ~=> square caps on the empty contour.
//
//   * All other contours converge to nothing.
//         => butt caps on the empty contour, which are ignored.
//
static StrokeCap empty_stroke_cap(const PLSPaint* paint, bool closed)
{
    if (closed)
    {
        switch (paint->getJoin())
        {
            case StrokeJoin::round:
                return StrokeCap::round;
            case StrokeJoin::miter:
                return StrokeCap::square;
            case StrokeJoin::bevel:
                return StrokeCap::butt;
        }
    }
    return paint->getCap();
}

RIVE_ALWAYS_INLINE bool is_final_verb_of_contour(const RawPath::Iter& iter,
                                                 const RawPath::Iter& end)
{
    return iter.rawVerbsPtr() + 1 == end.rawVerbsPtr();
}
} // namespace

// Helps count required resources for, and submit data to the render context that will be used to
// render paths with the "interior triangulation" algorithm.
class PLSRenderer::InteriorTriangulationHelper
{
public:
    size_t patchCount() const { return m_patchCount; }
    bool empty() const { return m_patchCount == 0; }

    enum class PathOp : bool
    {
        countDataAndTriangulate,
        submitOuterCubics
    };

    // For now, we just iterate and subdivide the path twice (once for each enum in PathOp). Since
    // we only do this for large paths, and since we're triangulating the path interior anyway,
    // adding complexity to only run Wang's formula and chop once would save about ~5% of the total
    // CPU time. (And large paths are GPU-bound anyway.)
    //
    // Returns the number of contours processed.
    size_t processPath(PathOp op,
                       PLSRenderContext* context,
                       PathDraw* path,
                       RawPath* scratchPath = nullptr)
    {
        Vec2D chops[kMaxCurveSubdivisions * 3 + 1];
        const RawPath& rawPath = *path->rawPath;
        assert(!rawPath.empty());
        wangs_formula::VectorXform vectorXform(*path->matrix);
        size_t patchCount = 0;
        size_t contourCount = 0;
        Vec2D p0 = {0, 0};
        if (op == PathOp::countDataAndTriangulate)
        {
            scratchPath->rewind();
        }
        for (const auto [verb, pts] : rawPath)
        {
            switch (verb)
            {
                case PathVerb::move:
                    if (contourCount != 0 && pts[-1] != p0)
                    {
                        if (op == PathOp::submitOuterCubics)
                        {
                            context->pushCubic(convert_line_to_cubic(pts[-1], p0).data(),
                                               {0, 0},
                                               flags::kCullExcessTessellationSegments,
                                               kPatchSegmentCountExcludingJoin,
                                               1,
                                               kJoinSegmentCount);
                        }
                        ++patchCount;
                    }
                    if (op == PathOp::countDataAndTriangulate)
                    {
                        scratchPath->move(pts[0]);
                    }
                    else
                    {
                        context->pushContour({0, 0}, true, 0);
                    }
                    p0 = pts[0];
                    ++contourCount;
                    break;
                case PathVerb::line:
                    if (op == PathOp::countDataAndTriangulate)
                    {
                        scratchPath->line(pts[1]);
                    }
                    else
                    {
                        context->pushCubic(convert_line_to_cubic(pts).data(),
                                           {0, 0},
                                           flags::kCullExcessTessellationSegments,
                                           kPatchSegmentCountExcludingJoin,
                                           1,
                                           kJoinSegmentCount);
                    }
                    ++patchCount;
                    break;
                case PathVerb::quad:
                    RIVE_UNREACHABLE();
                case PathVerb::cubic:
                {
                    size_t numSubdivisions = FindSubdivisionCount(pts, vectorXform);
                    if (numSubdivisions == 1)
                    {
                        if (op == PathOp::countDataAndTriangulate)
                        {
                            scratchPath->line(pts[3]);
                        }
                        else
                        {
                            context->pushCubic(pts,
                                               {0, 0},
                                               flags::kCullExcessTessellationSegments,
                                               kPatchSegmentCountExcludingJoin,
                                               1,
                                               kJoinSegmentCount);
                        }
                    }
                    else
                    {
                        // Passing nullptr for the 'tValues' causes it to chop the cubic uniformly
                        // in T.
                        pathutils::ChopCubicAt(pts, chops, nullptr, numSubdivisions - 1);
                        const Vec2D* chop = chops;
                        for (size_t i = 0; i < numSubdivisions; ++i)
                        {
                            if (op == PathOp::countDataAndTriangulate)
                            {
                                scratchPath->line(chop[3]);
                            }
                            else
                            {
                                context->pushCubic(chop,
                                                   {0, 0},
                                                   flags::kCullExcessTessellationSegments,
                                                   kPatchSegmentCountExcludingJoin,
                                                   1,
                                                   kJoinSegmentCount);
                            }
                            chop += 3;
                        }
                    }
                    patchCount += numSubdivisions;
                    break;
                }
                case PathVerb::close:
                    break;
            }
        }
        Vec2D lastPt = rawPath.points().back();
        if (contourCount != 0 && lastPt != p0)
        {
            if (op == PathOp::submitOuterCubics)
            {
                context->pushCubic(convert_line_to_cubic(lastPt, p0).data(),
                                   {0, 0},
                                   flags::kCullExcessTessellationSegments,
                                   kPatchSegmentCountExcludingJoin,
                                   1,
                                   kJoinSegmentCount);
            }
            ++patchCount;
        }

        if (op == PathOp::countDataAndTriangulate)
        {
            assert(!path->triangulator);
            path->triangulator =
                context->make<GrInnerFanTriangulator>(*scratchPath,
                                                      *path->matrix,
                                                      path->pathBounds,
                                                      path->fillRule,
                                                      context->trivialPerFlushAllocator());
            // We also draw each "grout" triangle using an outerCubic patch.
            patchCount += path->triangulator->groutList().count();
            path->tessVertexCount = patchCount * kOuterCurvePatchSegmentSpan;
            m_patchCount += patchCount;
        }
        else
        {
            // Submit grout triangles, retrofitted into outerCubic patches.
            for (auto* node = path->triangulator->groutList().head(); node; node = node->fNext)
            {
                Vec2D triangleAsCubic[4] = {node->fPts[0], node->fPts[1], {0, 0}, node->fPts[2]};
                context->pushCubic(triangleAsCubic,
                                   {0, 0},
                                   flags::kRetrofittedTriangle,
                                   kPatchSegmentCountExcludingJoin,
                                   1,
                                   kJoinSegmentCount);
                ++patchCount;
            }
            assert(contourCount == path->contourCount);
            assert(path->paddingVertexCount + patchCount * kOuterCurvePatchSegmentSpan ==
                   path->tessVertexCount);
            RIVE_DEBUG_CODE(m_writtenPatchCount += patchCount;)
            RIVE_DEBUG_CODE(m_writtenTessVertexCount += patchCount * kOuterCurvePatchSegmentSpan;)
        }

        return contourCount;
    }

#ifdef DEBUG
    bool didSubmitAllData()
    {
        return m_writtenPatchCount == m_patchCount &&
               m_writtenTessVertexCount == m_patchCount * kOuterCurvePatchSegmentSpan;
    }
#endif

private:
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

    size_t m_patchCount = 0;
    RIVE_DEBUG_CODE(size_t m_writtenPatchCount = 0;)
    RIVE_DEBUG_CODE(size_t m_writtenTessVertexCount = 0;)
};

bool PLSRenderer::pushInternalPathBatch(PLSPaint* finalPathPaint)
{
    // Only the final path in the batch uses 'finalPathPaint', which may or may not be stroked.
    size_t strokeIdx = finalPathPaint->getIsStroked() ? m_pathBatch.size() - 1
                                                      : std::numeric_limits<size_t>::max();
    float strokeMatrixMaxScale =
        finalPathPaint->getIsStroked() ? m_pathBatch.back().matrix->findMaxScale() : 0;
    float strokeRadius = finalPathPaint->getIsStroked() ? finalPathPaint->getThickness() * .5f : 0;

    // Count up how much temporary storage this function will need to reserve in CPU buffers.
    size_t maxStrokedCurvesBeforeChops = 0;
    size_t maxStrokedCurvesAfterChops = 0;
    size_t maxTotalCurvesAfterChops = 0;
    PLSPaint clipPaint;
    for (size_t i = 0; i < m_pathBatch.size(); ++i)
    {
        const RawPath* rawPath = m_pathBatch[i].rawPath;
        if (rawPath->empty())
        {
            continue;
        }
        bool stroked = i == strokeIdx; // (Will never be true if finalPathPaint is not stroked.)
        // Reserve enough space to record all the info we might need for this path.
        assert(rawPath->verbs()[0] == PathVerb::move);
        // Every path has at least 1 (non-curve) move.
        size_t maxCurves = rawPath->verbs().size() - 1;
        // Stroked cubics can be chopped into a maximum of 5 segments.
        size_t maxCurvesAfterChops = stroked ? maxCurves * 5 : maxCurves;
        if (stroked)
        {
            maxStrokedCurvesBeforeChops += maxCurves;
            maxStrokedCurvesAfterChops += maxCurvesAfterChops;
        }
        maxTotalCurvesAfterChops += maxCurvesAfterChops;
    }

    // Reserve temporary CPU storage for the loops that follow.
    // (+3 because we process these values in SIMD batches of 4, an may begin at n - 1.)
    m_parametricSegmentCounts_pow4.resize(
        std::max(maxTotalCurvesAfterChops + 3, m_parametricSegmentCounts_pow4.capacity()));
    m_parametricSegmentCounts.resize(
        std::max(maxTotalCurvesAfterChops + 3, m_parametricSegmentCounts.capacity()));
    size_t maxTangentPairs = 0;
    if (maxStrokedCurvesAfterChops != 0)
    {
        assert(finalPathPaint->getIsStroked());
        // Each stroked curve will record the number of chops it requires (either 0, 1, or 2).
        m_numChops.resizeAndRewind(std::max(maxStrokedCurvesBeforeChops, m_numChops.capacity()));
        // We only chop into this queue if a cubic has one chop. More chops in a single cubic
        // are rare and require a lot of memory, so if a cubic needs more chops we just re-chop
        // the second time around. The maximum size this queue would need is therefore enough to
        // chop each cubic once, or 7 points per.
        m_chops.resizeAndRewind(std::max(maxStrokedCurvesBeforeChops * 7, m_chops.capacity()));
        // After chopping, each stroked curve will also record its beginning and ending tangents
        // (4 floats) so we can measure its rotation.
        maxTangentPairs += maxStrokedCurvesAfterChops;
    }
    if (finalPathPaint->getIsStroked())
    {
        // If the stroke has round joins, we also record the tangents between (pre-chopped) joins in
        // order to calculate how many vertices are in each round join.
        if (finalPathPaint->getJoin() == StrokeJoin::round)
        {
            maxTangentPairs += maxStrokedCurvesBeforeChops;
        }
        // Reserve temporary CPU storage for the loops that follow.
        // (+3 because we process these values in SIMD batches of 4, an may begin at n - 1.)
        m_tangentPairs.resize(std::max(maxTangentPairs + 3, m_tangentPairs.capacity()));
        m_polarSegmentCounts.resize(
            std::max(maxStrokedCurvesAfterChops + 3, m_polarSegmentCounts.capacity()));
    }

    InteriorTriangulationHelper interiorTriHelper;

    // Iteration pass 1: Collect information on contour and curves counts for every path in the
    // batch, and begin counting tessellated vertices.
    m_contourBatch.clear();
    size_t contourCount = 0;
    size_t lineCount = 0;
    size_t curveCount = 0;
    size_t rotationCount = 0; // We measure rotations on both curves and round joins.
    for (size_t i = 0; i < m_pathBatch.size(); ++i)
    {
        PathDraw& path = m_pathBatch[i];
        if (path.rawPath->empty())
        {
            continue;
        }

        size_t pathContourCount = 0;
        bool stroked = i == strokeIdx; // (Will never be true if finalPathPaint is not stroked.)
        assert(path.triangulator == nullptr);
        if (!stroked && FindTransformedArea(path.pathBounds, *path.matrix) > 512 * 512)
        {
            // This path is a sufficiently-large fill. Use interior triangulation!
            pathContourCount = interiorTriHelper.processPath(
                InteriorTriangulationHelper::PathOp::countDataAndTriangulate,
                m_context,
                &path,
                &m_scratchPath);
        }
        else
        {
            bool roundJoinStroked = stroked && finalPathPaint->getJoin() == StrokeJoin::round;
            wangs_formula::VectorXform vectorXform(*path.matrix);
            RawPath::Iter startOfContour = path.rawPath->begin();
            RawPath::Iter end = path.rawPath->end();
            int preChopVerbCount = 0; // Original number of lines and curves, before chopping.
            Vec2D endpointsSum{};
            bool closed = !stroked;
            Vec2D lastTangent = {0, 1};
            Vec2D firstTangent = {0, 1};
            size_t roundJoinCount = 0;
            path.firstContourIdx = m_contourBatch.size();
            auto finishAndAppendContour = [&](RawPath::Iter iter) {
                if (closed)
                {
                    Vec2D finalPtInContour = iter.rawPtsPtr()[-1];
                    if (startOfContour.movePt() != finalPtInContour)
                    {
                        assert(preChopVerbCount > 0);
                        if (roundJoinStroked)
                        {
                            // Round join before implicit closing line.
                            Vec2D tangent = startOfContour.movePt() - finalPtInContour;
                            assert(rotationCount < m_tangentPairs.capacity());
                            m_tangentPairs[rotationCount++] = {lastTangent, tangent};
                            lastTangent = tangent;
                            ++roundJoinCount;
                        }
                        ++lineCount; // Implicit closing line.
                        // The first point in the contour hasn't gotten counted yet.
                        ++preChopVerbCount;
                        endpointsSum += startOfContour.movePt();
                    }
                    if (roundJoinStroked && preChopVerbCount != 0)
                    {
                        // Round join back to the beginning of the contour.
                        assert(rotationCount < m_tangentPairs.capacity());
                        m_tangentPairs[rotationCount++] = {lastTangent, firstTangent};
                        ++roundJoinCount;
                    }
                }
                size_t strokeJoinCount = preChopVerbCount;
                if (!closed)
                {
                    strokeJoinCount = std::max<size_t>(strokeJoinCount, 1) - 1;
                }
                m_contourBatch.emplace_back(iter,
                                            lineCount,
                                            curveCount,
                                            rotationCount,
                                            stroked ? Vec2D()
                                                    : endpointsSum * (1.f / preChopVerbCount),
                                            closed,
                                            strokeJoinCount);
                ++pathContourCount;
            };
            const int styleFlags = style_flags(stroked, roundJoinStroked);
            for (RawPath::Iter iter = startOfContour; iter != end; ++iter)
            {
                switch (styled_verb(iter.verb(), styleFlags))
                {
                    case StyledVerb::roundJoinStrokedMove:
                    case StyledVerb::strokedMove:
                    case StyledVerb::filledMove:
                        if (iter != startOfContour)
                        {
                            finishAndAppendContour(iter);
                            startOfContour = iter;
                        }
                        preChopVerbCount = 0;
                        endpointsSum = {0, 0};
                        closed = !stroked;
                        lastTangent = {0, 1};
                        firstTangent = {0, 1};
                        roundJoinCount = 0;
                        break;
                    case StyledVerb::roundJoinStrokedClose:
                    case StyledVerb::strokedClose:
                    case StyledVerb::filledClose:
                        assert(iter != startOfContour);
                        closed = true;
                        break;
                    case StyledVerb::roundJoinStrokedLine:
                    {
                        const Vec2D* p = iter.linePts();
                        Vec2D tangent = p[1] - p[0];
                        if (preChopVerbCount == 0)
                        {
                            firstTangent = tangent;
                        }
                        else
                        {
                            assert(rotationCount < m_tangentPairs.capacity());
                            m_tangentPairs[rotationCount++] = {lastTangent, tangent};
                            ++roundJoinCount;
                        }
                        lastTangent = tangent;
                        [[fallthrough]];
                    }
                    case StyledVerb::strokedLine:
                    case StyledVerb::filledLine:
                    {
                        const Vec2D* p = iter.linePts();
                        ++preChopVerbCount;
                        endpointsSum += p[1];
                        ++lineCount;
                        break;
                    }
                    case StyledVerb::roundJoinStrokedQuad:
                    case StyledVerb::strokedQuad:
                    case StyledVerb::filledQuad:
                        RIVE_UNREACHABLE();
                        break;
                    case StyledVerb::roundJoinStrokedCubic:
                    {
                        const Vec2D* p = iter.cubicPts();
                        Vec2D unchoppedTangents[2];
                        find_cubic_tangents(p, unchoppedTangents);
                        if (preChopVerbCount == 0)
                        {
                            firstTangent = unchoppedTangents[0];
                        }
                        else
                        {
                            assert(rotationCount < m_tangentPairs.capacity());
                            m_tangentPairs[rotationCount++] = {lastTangent, unchoppedTangents[0]};
                            ++roundJoinCount;
                        }
                        lastTangent = unchoppedTangents[1];
                        [[fallthrough]];
                    }
                    case StyledVerb::strokedCubic:
                    {
                        const Vec2D* p = iter.cubicPts();
                        ++preChopVerbCount;
                        endpointsSum += p[3];
                        // Chop strokes into sections that do not inflect (i.e, are convex), and do
                        // not rotate more than 180 degrees. This is required by the GPU
                        // parametric/polar sorter.
                        float t[2];
                        bool areCusps;
                        uint8_t numChops = pathutils::FindCubicConvex180Chops(p, t, &areCusps);
                        uint8_t chopKey = chop_key(areCusps, numChops);
                        m_numChops.push_back(chopKey);
                        Vec2D localChopBuffer[16];
                        switch (chopKey)
                        {
                            case cusp_chop_key(2): // 2 cusps
                            case cusp_chop_key(1): // 1 cusp
                                // We have to chop carefully around stroked cusps in order to avoid
                                // rendering artifacts. Luckily, cusps are extremely rare in
                                // real-world content.
                                m_chops.push_back() = {t[0], t[1]};
                                chop_cubic_around_cusps(p,
                                                        localChopBuffer,
                                                        t,
                                                        numChops,
                                                        strokeMatrixMaxScale);
                                p = localChopBuffer;
                                numChops *= 2;
                                break;
                            case simple_chop_key(2): // 2 non-cusp chops
                                m_chops.push_back() = {t[0], t[1]};
                                pathutils::ChopCubicAt(p, localChopBuffer, t[0], t[1]);
                                p = localChopBuffer;
                                break;
                            case simple_chop_key(1): // 1 non-cusp chop
                            {
                                Vec2D* buff = m_chops.push_back_n(7);
                                pathutils::ChopCubicAt(p, buff, t[0]);
                                p = buff;
                                break;
                            }
                        }
                        // Calculate segment counts for each chopped section independently.
                        for (const Vec2D* end = p + numChops * 3 + 3; p != end;
                             p += 3, ++curveCount, ++rotationCount)
                        {
                            float n4 =
                                wangs_formula::cubic_pow4(p, kParametricPrecision, vectorXform);
                            m_parametricSegmentCounts_pow4[curveCount] = n4;
                            assert(rotationCount < m_tangentPairs.capacity());
                            find_cubic_tangents(p, m_tangentPairs[rotationCount].data());
                        }
                        break;
                    }
                    case StyledVerb::filledCubic:
                    {
                        const Vec2D* p = iter.cubicPts();
                        ++preChopVerbCount;
                        endpointsSum += p[3];
                        float n4 = wangs_formula::cubic_pow4(p, kParametricPrecision, vectorXform);
                        m_parametricSegmentCounts_pow4[curveCount++] = n4;
                        break;
                    }
                }
            }
            if (startOfContour != end)
            {
                finishAndAppendContour(end);
            }
        }
        path.contourCount = pathContourCount;
        contourCount += pathContourCount;
    }

    if (contourCount == 0)
    {
        // The entire batch is empty.
        return true;
    }

    // Iteration pass 2: Finish calculating the numbers of tessellation segments in each contour,
    // using SIMD.
    size_t contourFirstLineIdx = 0;
    size_t contourFirstCurveIdx = 0;
    size_t contourFirstRotationIdx = 0;
    size_t emptyStrokeCountForCaps = 0;
    PLSRenderContext::TessVertexCounter tessVertexCounter(m_context);
    for (size_t currentPathIdx = 0; currentPathIdx < m_pathBatch.size(); ++currentPathIdx)
    {
        PathDraw& path = m_pathBatch[currentPathIdx];
        if (path.rawPath->empty())
        {
            continue;
        }

        bool stroked = currentPathIdx == strokeIdx;

        // (If we used interior triangulation, interiorTriHelper already counted the path's vertices
        // for us.)
        if (path.triangulator != nullptr)
        {
            assert(path.paddingVertexCount == 0);
            // If the path has a nonzero number of tessellation vertices, pad them so they align on
            // a multiple of the patch size.
            if (path.tessVertexCount > 0)
            {
                assert(!stroked);
                path.paddingVertexCount =
                    tessVertexCounter.countPath<kOuterCurvePatchSegmentSpan>(path.tessVertexCount,
                                                                             false);
                path.tessVertexCount += path.paddingVertexCount;
            }
        }
        else
        {
            assert(path.tessVertexCount == 0);
            for (size_t i = 0; i < path.contourCount; ++i)
            {
                ContourData* contour = &m_contourBatch[path.firstContourIdx + i];
                size_t contourLineCount = contour->endLineIdx - contourFirstLineIdx;
                uint32_t contourVertexCount =
                    contourLineCount * 2; // Each line tessellates to 2 vertices.
                uint4 mergedTessVertexSums4 = 0;

                // Finish calculating and counting parametric segments for each curve.
                size_t j;
                for (j = contourFirstCurveIdx; j < contour->endCurveIdx; j += 4)
                {
                    assert(j + 4 <= m_parametricSegmentCounts_pow4.capacity());
                    // Curves recorded their segment counts raised to the 4th power. Now find their
                    // roots and convert to integers in batches of 4.
                    float4 n = simd::load4f(m_parametricSegmentCounts_pow4.get() + j);
                    n = simd::ceil(simd::sqrt(simd::sqrt(n)));
                    n = simd::clamp(n, float4(1), float4(kMaxParametricSegments));
                    uint4 n_ = simd::cast<uint32_t>(n);
                    assert(j + 4 <= m_parametricSegmentCounts.capacity());
                    simd::store(m_parametricSegmentCounts.get() + j, n_);
                    mergedTessVertexSums4 += n_;
                }
                // We counted in batches of 4. Undo the values we counted from beyond the end of the
                // path.
                while (j-- > contour->endCurveIdx)
                {
                    contourVertexCount -= m_parametricSegmentCounts[j];
                }

                if (stroked)
                {
                    // Finish calculating and counting polar segments for each stroked curve and
                    // round join.
                    const float r_ = strokeRadius * strokeMatrixMaxScale;
                    const float polarSegmentsPerRad =
                        pathutils::CalcPolarSegmentsPerRadian<kPolarPrecision>(r_);
                    for (j = contourFirstRotationIdx; j < contour->endRotationIdx; j += 4)
                    {
                        // Measure the rotations of curves in batches of 4.
                        assert(j + 4 <= m_tangentPairs.capacity());
                        auto [tx0, ty0, tx1, ty1] = simd::load4x4f(&m_tangentPairs[j][0].x);
                        float4 numer = tx0 * tx1 + ty0 * ty1;
                        float4 denom_pow2 = (tx0 * tx0 + ty0 * ty0) * (tx1 * tx1 + ty1 * ty1);
                        float4 cosTheta = numer / simd::sqrt(denom_pow2);
                        cosTheta = simd::clamp(cosTheta, float4(-1), float4(1));
                        float4 theta = simd::fast_acos(cosTheta);
                        // Find polar segment counts from the rotation angles.
                        float4 n = simd::ceil(theta * polarSegmentsPerRad);
                        n = simd::clamp(n, float4(1), float4(kMaxPolarSegments));
                        uint4 n_ = simd::cast<uint32_t>(n);
                        assert(j + 4 <= m_polarSegmentCounts.capacity());
                        simd::store(m_polarSegmentCounts.get() + j, n_);
                        // Polar and parametric segments share the first and final vertices.
                        // Therefore:
                        //
                        //   parametricVertexCount = parametricSegmentCount + 1
                        //
                        //   polarVertexCount = polarVertexCount + 1
                        //
                        //   mergedVertexCount = parametricVertexCount + polarVertexCount - 2
                        //                     = parametricSegmentCount + 1 + polarSegmentCount + 1
                        //                     - 2 = parametricSegmentCount + polarSegmentCount
                        //
                        mergedTessVertexSums4 += n_;
                    }

                    // We counted in batches of 4. Undo the values we counted from beyond the end of
                    // the path.
                    while (j-- > contour->endRotationIdx)
                    {
                        contourVertexCount -= m_polarSegmentCounts[j];
                    }

                    // Count joins.
                    if (finalPathPaint->getJoin() == StrokeJoin::round)
                    {
                        // Round joins share their beginning and ending vertices with the curve on
                        // either side. Therefore, the number of vertices we need to allocate for a
                        // round join is "joinSegmentCount - 1". Do all the -1's here.
                        contourVertexCount -= contour->strokeJoinCount;
                    }
                    else
                    {
                        // The shader needs 3 segments for each miter and bevel join (which
                        // translates to two interior vertices, since joins share their beginning
                        // and ending vertices with the curve on either side).
                        contourVertexCount +=
                            contour->strokeJoinCount * (kNumSegmentsInMiterOrBevelJoin - 1);
                    }

                    // Count stroke caps, if any.
                    bool empty = contour->endLineIdx == contourFirstLineIdx &&
                                 contour->endCurveIdx == contourFirstCurveIdx;
                    StrokeCap cap;
                    bool needsCaps;
                    if (!empty)
                    {
                        cap = finalPathPaint->getCap();
                        needsCaps = !contour->closed;
                    }
                    else
                    {
                        cap = empty_stroke_cap(finalPathPaint, contour->closed);
                        needsCaps =
                            cap != StrokeCap::butt; // Ignore butt caps when the contour is empty.
                    }
                    if (needsCaps)
                    {
                        // We emulate stroke caps as 180-degree joins.
                        if (cap == StrokeCap::round)
                        {
                            // Round caps rotate 180 degrees.
                            contour->strokeCapSegmentCount = ceilf(polarSegmentsPerRad * math::PI);
                            // +2 because round caps emulated as joins need to emit vertices at T=0
                            // and T=1, unlike normal round joins.
                            contour->strokeCapSegmentCount += 2;
                            // Make sure not to exceed kMaxPolarSegments.
                            contour->strokeCapSegmentCount =
                                std::min(contour->strokeCapSegmentCount, kMaxPolarSegments);
                        }
                        else
                        {
                            contour->strokeCapSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                        }
                        // pushContour() uses "strokeCapSegmentCount != 0" to tell if it needs
                        // stroke caps.
                        assert(contour->strokeCapSegmentCount != 0);
                        // As long as a contour isn't empty, we can tack the end cap onto the join
                        // section of the final curve in the stroke. Otherwise, we need to introduce
                        // 0-tessellation-segment curves with non-empty joins to carry the caps.
                        emptyStrokeCountForCaps += empty ? 2 : 1;
                        contourVertexCount += (contour->strokeCapSegmentCount - 1) * 2;
                    }
                }
                else
                {
                    // Fills don't have polar segments:
                    //
                    //   mergedVertexCount = parametricVertexCount = parametricSegmentCount + 1
                    //
                    // Just collect the +1 for each non-stroked curve.
                    size_t contourCurveCount = contour->endCurveIdx - contourFirstCurveIdx;
                    contourVertexCount += contourCurveCount;
                }
                contourVertexCount += simd::sum(mergedTessVertexSums4);

                // Add padding vertices until the number of tessellation vertices in the contour is
                // an exact multiple of kMidpointFanPatchSegmentSpan. This ensures that patch
                // boundaries align with contour boundaries.
                contour->paddingVertexCount =
                    PaddingToAlignUp<kMidpointFanPatchSegmentSpan>(contourVertexCount);
                contourVertexCount += contour->paddingVertexCount;
                assert(contourVertexCount % kMidpointFanPatchSegmentSpan == 0);
                RIVE_DEBUG_CODE(contour->tessVertexCount = contourVertexCount;)

                path.tessVertexCount += contourVertexCount;
                contourFirstLineIdx = contour->endLineIdx;
                contourFirstCurveIdx = contour->endCurveIdx;
                contourFirstRotationIdx = contour->endRotationIdx;
            }
            assert(path.paddingVertexCount == 0);
            // If the path has a nonzero number of tessellation vertices, pad them so they align on
            // a multiple of the patch size.
            if (path.tessVertexCount > 0)
            {
                path.paddingVertexCount =
                    tessVertexCounter.countPath<kMidpointFanPatchSegmentSpan>(path.tessVertexCount,
                                                                              stroked);
                path.tessVertexCount += path.paddingVertexCount;
            }
        }
    }
    assert(contourFirstLineIdx == lineCount);
    assert(contourFirstCurveIdx == curveCount);
    assert(contourFirstRotationIdx == rotationCount);

    // Attempt to reserve space on the GPU for our entire batch of paths.
    size_t curveReserveCount =
        curveCount + lineCount + emptyStrokeCountForCaps + interiorTriHelper.patchCount();
    if (!m_context->reservePathData(m_pathBatch.size(),
                                    contourCount,
                                    curveReserveCount,
                                    tessVertexCounter))
    {
        // The paths don't fit. Give up and let the caller flush and try again.
        return false;
    }

    // Set up rendering for 'finalPathPaint', including allocating span in the gradient texture, if
    // needed.
    PaintData finalPathPaintData;
    switch (finalPathPaint->getType())
    {
        case PaintType::solidColor:
            finalPathPaintData = PaintData::MakeColor(finalPathPaint->getColor());
            break;
        case PaintType::linearGradient:
        case PaintType::radialGradient:
            if (!m_context->pushGradient(finalPathPaint->getGradient(), &finalPathPaintData))
            {
                // The gradient doesn't fit. Give up and let the caller flush and try again.
                return false;
            }
            break;
        case PaintType::image:
            finalPathPaintData = PaintData::MakeImage(finalPathPaint->getImageOpacity());
            break;
        case PaintType::clipUpdate:
            RIVE_UNREACHABLE();
    }

    // Iteration pass 3: Now that we have space reserved, push the whole batch of paths to the GPU.
    RIVE_DEBUG_CODE(size_t pushedPathCount = 0;)
    RIVE_DEBUG_CODE(size_t skippedPathCount = 0;)
    RIVE_DEBUG_CODE(size_t pushedContourCount = 0;)
    RIVE_DEBUG_CODE(size_t skippedContourCount = 0;)
    RIVE_DEBUG_CODE(m_pushedLineCount = 0;)
    RIVE_DEBUG_CODE(m_pushedCurveCount = 0;)
    RIVE_DEBUG_CODE(m_pushedRotationCount = 0;)
    RIVE_DEBUG_CODE(m_pushedEmptyStrokeCountForCaps = 0;)
    size_t curveIdx = 0;
    size_t rotationIdx = 0;
    RawPath::Iter startOfContour;
    size_t finalPathIdx = m_pathBatch.size() - 1; // All paths are clips except the final one.
    for (size_t currentPathIdx = 0; currentPathIdx < m_pathBatch.size(); ++currentPathIdx)
    {
        PathDraw& path = m_pathBatch[currentPathIdx];
        if (path.tessVertexCount == 0)
        {
            RIVE_DEBUG_CODE(skippedContourCount += path.contourCount;)
            RIVE_DEBUG_CODE(++skippedPathCount;)
            continue;
        }
        assert(!path.rawPath->empty());

        // Push a path record.
        bool isClipPath = currentPathIdx != finalPathIdx;
        PaintType currentPaintType = isClipPath ? PaintType::clipUpdate : finalPathPaint->getType();
        const PaintData& currentPaintData =
            isClipPath ? PaintData::MakeClipUpdate(path.outerClipID) : finalPathPaintData;
        const Mat2D* pixelToNormalizedClipRect =
            m_stack.back().hasClipRect ? &m_stack.back().pixelToNormalizedClipRect : nullptr;
        m_context->pushPath(path.triangulator ? PatchType::outerCurves : PatchType::midpointFan,
                            *path.matrix,
                            isClipPath ? 0 : strokeRadius, // Clips are never stroked.
                            path.fillRule,
                            currentPaintType,
                            currentPaintData,
                            finalPathPaint->getImageTexture(), // Ignored by clip updates.
                            path.clipID,
                            pixelToNormalizedClipRect,
                            finalPathPaint->getBlendMode(), // Ignored by clip updates.
                            path.tessVertexCount,
                            path.paddingVertexCount);
        RIVE_DEBUG_CODE(++pushedPathCount;)

        if (path.triangulator != nullptr)
        {
            // This path is drawn with the interior triangulation algorithm instead.
            size_t processedContourCount RIVE_MAYBE_UNUSED = interiorTriHelper.processPath(
                InteriorTriangulationHelper::PathOp::submitOuterCubics,
                m_context,
                &path);
            RIVE_DEBUG_CODE(pushedContourCount += processedContourCount;)
            m_context->pushInteriorTriangulation(path.triangulator,
                                                 currentPaintType,
                                                 currentPaintData,
                                                 finalPathPaint->getImageTexture(),
                                                 path.clipID,
                                                 pixelToNormalizedClipRect,
                                                 finalPathPaint->getBlendMode());
        }
        else
        {
            startOfContour = path.rawPath->begin();
            for (size_t i = 0; i < path.contourCount; ++i)
            {
                // Push a contour and curve records.
                const ContourData& contour = m_contourBatch[path.firstContourIdx + i];
                RIVE_DEBUG_CODE(m_pushedStrokeJoinCount = 0;)
                RIVE_DEBUG_CODE(m_pushedStrokeCapCount = 0;)
                pushContour(startOfContour,
                            contour,
                            curveIdx,
                            rotationIdx,
                            strokeMatrixMaxScale,
                            currentPathIdx == strokeIdx ? finalPathPaint : nullptr);
                assert(m_pushedCurveCount == contour.endCurveIdx);
                assert(m_pushedRotationCount == contour.endRotationIdx);
                assert(m_pushedStrokeJoinCount ==
                       (currentPathIdx == strokeIdx ? contour.strokeJoinCount : 0));
                assert(m_pushedStrokeCapCount == (contour.strokeCapSegmentCount != 0 ? 2 : 0));
                curveIdx = contour.endCurveIdx;
                rotationIdx = contour.endRotationIdx;
                startOfContour = contour.endOfContour;
                RIVE_DEBUG_CODE(++pushedContourCount);
            }
        }
    }

    // Make sure we only pushed the amount of data we reserved.
    assert(pushedPathCount + skippedPathCount == m_pathBatch.size());
    assert(pushedContourCount + skippedContourCount == contourCount);
    assert(m_pushedLineCount == lineCount);
    assert(m_pushedCurveCount == curveCount);
    assert(m_pushedRotationCount == rotationCount);
    assert(m_pushedEmptyStrokeCountForCaps == emptyStrokeCountForCaps);
    assert(interiorTriHelper.didSubmitAllData());
    assert(m_pushedLineCount + m_pushedCurveCount + m_pushedEmptyStrokeCountForCaps +
               interiorTriHelper.patchCount() ==
           curveReserveCount);
    return true;
}

void PLSRenderer::pushContour(RawPath::Iter iter,
                              const ContourData& contour,
                              size_t curveIdx,
                              size_t rotationIdx,
                              float matrixMaxScale,
                              const PLSPaint* strokePaint)
{
    assert(iter.verb() == PathVerb::move);
    assert(strokePaint != nullptr || contour.closed); // Fills are always closed.
    RIVE_DEBUG_CODE(const size_t startingCurveIdx = curveIdx;)
    RIVE_DEBUG_CODE(const size_t startingRotationIdx = rotationIdx;)

    const Vec2D* pts = iter.rawPtsPtr();
    const RawPath::Iter end = contour.endOfContour;
    uint32_t joinTypeFlags = 0;
    bool roundJoinStroked = false;
    bool needsFirstEmulatedCapAsJoin = false; // Emit a starting cap before the next cubic?
    uint32_t emulatedCapAsJoinFlags = 0;
    if (strokePaint != nullptr)
    {
        joinTypeFlags = flags::JoinTypeFlags(strokePaint->getJoin());
        roundJoinStroked = joinTypeFlags == 0;
        if (contour.strokeCapSegmentCount != 0)
        {
            StrokeCap cap = !contour.closed ? strokePaint->getCap()
                                            : empty_stroke_cap(strokePaint, contour.closed);
            emulatedCapAsJoinFlags = flags::kEmulatedStrokeCap;
            if (cap == StrokeCap::square)
            {
                emulatedCapAsJoinFlags |= flags::kMiterClipJoin;
            }
            else if (cap == StrokeCap::butt)
            {
                emulatedCapAsJoinFlags |= flags::kBevelJoin;
            }
            needsFirstEmulatedCapAsJoin = true;
        }
    }

    // Make a data record for this current contour on the GPU.
    m_context->pushContour(contour.midpoint, contour.closed, contour.paddingVertexCount);

    // Convert all curves in the contour to cubics and push them to the GPU.
    const int styleFlags = style_flags(strokePaint != nullptr, roundJoinStroked);
    Vec2D joinTangent = {0, 1};
    int joinSegmentCount = 1;
    Vec2D implicitClose[2]; // In case we need an implicit closing line.
    for (; iter != end; ++iter)
    {
        StyledVerb styledVerb = styled_verb(iter.verb(), styleFlags);
        switch (styledVerb)
        {
            case StyledVerb::filledMove:
            case StyledVerb::strokedMove:
            case StyledVerb::roundJoinStrokedMove:
                implicitClose[1] = iter.movePt(); // In case we need an implicit closing line.
                break;
            case StyledVerb::filledClose:
            case StyledVerb::strokedClose:
            case StyledVerb::roundJoinStrokedClose:
                assert(contour.closed);
                break;
            case StyledVerb::roundJoinStrokedLine:
            {
                if (contour.closed || !is_final_verb_of_contour(iter, end))
                {
                    joinTangent = m_tangentPairs[rotationIdx][1];
                    joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                    ++rotationIdx;
                    RIVE_DEBUG_CODE(++m_pushedStrokeJoinCount;)
                }
                else
                {
                    // End with a 180-degree join that looks like the stroke cap.
                    joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                    joinTypeFlags = emulatedCapAsJoinFlags;
                    joinSegmentCount = contour.strokeCapSegmentCount;
                    RIVE_DEBUG_CODE(++m_pushedStrokeCapCount;)
                }
                goto line_common;
            }
            case StyledVerb::strokedLine:
                if (contour.closed || !is_final_verb_of_contour(iter, end))
                {
                    joinTangent =
                        find_join_tangent(iter.linePts() + 1, end.rawPtsPtr(), contour.closed, pts);
                    joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                    RIVE_DEBUG_CODE(++m_pushedStrokeJoinCount;)
                }
                else
                {
                    // End with a 180-degree join that looks like the stroke cap.
                    joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                    joinTypeFlags = emulatedCapAsJoinFlags;
                    joinSegmentCount = contour.strokeCapSegmentCount;
                    RIVE_DEBUG_CODE(++m_pushedStrokeCapCount;)
                }
                [[fallthrough]];
            case StyledVerb::filledLine:
            line_common:
            {
                std::array<Vec2D, 4> cubic = convert_line_to_cubic(iter.linePts());
                if (needsFirstEmulatedCapAsJoin)
                {
                    // Emulate the start cap as a 180-degree join before the first stroke.
                    pushEmulatedStrokeCapAsJoinBeforeCubic(cubic.data(),
                                                           emulatedCapAsJoinFlags,
                                                           contour.strokeCapSegmentCount);
                    needsFirstEmulatedCapAsJoin = false;
                }
                m_context
                    ->pushCubic(cubic.data(), joinTangent, joinTypeFlags, 1, 1, joinSegmentCount);
                RIVE_DEBUG_CODE(++m_pushedLineCount;)
                break;
            }
            case StyledVerb::roundJoinStrokedQuad:
            case StyledVerb::strokedQuad:
            case StyledVerb::filledQuad:
                RIVE_UNREACHABLE();
                break;
            case StyledVerb::roundJoinStrokedCubic:
            case StyledVerb::strokedCubic:
            {
                const Vec2D* p = iter.cubicPts();
                uint8_t chopKey = m_numChops.pop_front();
                uint8_t numChops = 0;
                Vec2D localChopBuffer[16];
                switch (chopKey)
                {
                    case cusp_chop_key(2): // 2 cusps
                    case cusp_chop_key(1): // 1 cusp
                        // We have to chop carefully around stroked cusps in order to avoid
                        // rendering artifacts. Luckily, cusps are extremely rare in real-world
                        // content.
                        chop_cubic_around_cusps(p,
                                                localChopBuffer,
                                                &m_chops.pop_front().x,
                                                chopKey >> 1,
                                                matrixMaxScale);
                        p = localChopBuffer;
                        // The bottom bit of chopKey is 1, meaning "areCusps". Clearing the bottom
                        // bit leaves "numChops * 2", which is the number of chops a cusp needs!
                        numChops = chopKey ^ 1;
                        break;

                    case simple_chop_key(2): // 2 non-cusp chops
                    {
                        // Curves that need 2 chops are rare in real-world content. Just re-chop the
                        // curve this time around as well.
                        auto [t0, t1] = m_chops.pop_front();
                        pathutils::ChopCubicAt(p, localChopBuffer, t0, t1);
                        p = localChopBuffer;
                        numChops = 2;
                        break;
                    }
                    case simple_chop_key(1): // 1 non-cusp chop
                        // Single-chop curves were saved in the m_chops queue.
                        p = m_chops.pop_front_n(7);
                        numChops = 1;
                        break;
                }
                if (needsFirstEmulatedCapAsJoin)
                {
                    // Emulate the start cap as a 180-degree join before the first stroke.
                    pushEmulatedStrokeCapAsJoinBeforeCubic(p,
                                                           emulatedCapAsJoinFlags,
                                                           contour.strokeCapSegmentCount);
                    needsFirstEmulatedCapAsJoin = false;
                }
                // Push chops before the final one.
                for (size_t end = curveIdx + numChops; curveIdx != end;
                     ++curveIdx, ++rotationIdx, p += 3)
                {
                    uint32_t parametricSegmentCount = m_parametricSegmentCounts[curveIdx];
                    uint32_t polarSegmentCount = m_polarSegmentCounts[rotationIdx];
                    m_context->pushCubic(p,
                                         joinTangent,
                                         joinTypeFlags,
                                         parametricSegmentCount,
                                         polarSegmentCount,
                                         1);
                }
                // Push the final chop, with a join.
                uint32_t parametricSegmentCount = m_parametricSegmentCounts[curveIdx++];
                uint32_t polarSegmentCount = m_polarSegmentCounts[rotationIdx++];
                if (contour.closed || !is_final_verb_of_contour(iter, end))
                {
                    if (styledVerb == StyledVerb::roundJoinStrokedCubic)
                    {
                        joinTangent = m_tangentPairs[rotationIdx][1];
                        joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                        ++rotationIdx;
                    }
                    else
                    {
                        joinTangent = find_join_tangent(iter.cubicPts() + 3,
                                                        end.rawPtsPtr(),
                                                        contour.closed,
                                                        pts);
                        joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                    }
                    RIVE_DEBUG_CODE(++m_pushedStrokeJoinCount;)
                }
                else
                {
                    // End with a 180-degree join that looks like the stroke cap.
                    joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                    joinTypeFlags = emulatedCapAsJoinFlags;
                    joinSegmentCount = contour.strokeCapSegmentCount;
                    RIVE_DEBUG_CODE(++m_pushedStrokeCapCount;)
                }
                m_context->pushCubic(p,
                                     joinTangent,
                                     joinTypeFlags,
                                     parametricSegmentCount,
                                     polarSegmentCount,
                                     joinSegmentCount);
                break;
            }
            case StyledVerb::filledCubic:
            {
                uint32_t parametricSegmentCount = m_parametricSegmentCounts[curveIdx++];
                m_context->pushCubic(iter.cubicPts(), Vec2D{}, 0, parametricSegmentCount, 1, 1);
                break;
            }
        }
    }

    if (needsFirstEmulatedCapAsJoin)
    {
        // The contour was empty. Emit both caps on p0.
        Vec2D p0 = pts[0], left = {p0.x - 1, p0.y}, right = {p0.x + 1, p0.y};
        pushEmulatedStrokeCapAsJoinBeforeCubic(std::array{p0, right, right, right}.data(),
                                               emulatedCapAsJoinFlags,
                                               contour.strokeCapSegmentCount);
        pushEmulatedStrokeCapAsJoinBeforeCubic(std::array{p0, left, left, left}.data(),
                                               emulatedCapAsJoinFlags,
                                               contour.strokeCapSegmentCount);
    }
    else if (contour.closed)
    {
        implicitClose[0] = iter.rawPtsPtr()[-1];
        if (implicitClose[0] != implicitClose[1])
        {
            // Draw a line back to the beginning of the contour.
            std::array<Vec2D, 4> cubic = convert_line_to_cubic(implicitClose);
            // Closing join back to the beginning of the contour.
            if (roundJoinStroked)
            {
                joinTangent = m_tangentPairs[rotationIdx][1];
                joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                ++rotationIdx;
                RIVE_DEBUG_CODE(++m_pushedStrokeJoinCount;)
            }
            else if (strokePaint != nullptr)
            {
                joinTangent = find_starting_tangent(pts, end.rawPtsPtr());
                joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                RIVE_DEBUG_CODE(++m_pushedStrokeJoinCount;)
            }
            m_context->pushCubic(cubic.data(), joinTangent, joinTypeFlags, 1, 1, joinSegmentCount);
            RIVE_DEBUG_CODE(++m_pushedLineCount;)
        }
    }

    RIVE_DEBUG_CODE(m_pushedCurveCount += curveIdx - startingCurveIdx;)
    RIVE_DEBUG_CODE(m_pushedRotationCount += rotationIdx - startingRotationIdx;)
}

void PLSRenderer::pushEmulatedStrokeCapAsJoinBeforeCubic(const Vec2D cubic[],
                                                         uint32_t emulatedCapAsJoinFlags,
                                                         uint32_t strokeCapSegmentCount)
{
    // Reverse the cubic and push it with zero parametric and polar segments, and a 180-degree join
    // tangent. This results in a solitary join, positioned immediately before the provided cubic,
    // that looks like the desired stroke cap.
    m_context->pushCubic(std::array{cubic[3], cubic[2], cubic[1], cubic[0]}.data(),
                         find_cubic_tan0(cubic),
                         emulatedCapAsJoinFlags,
                         0,
                         0,
                         strokeCapSegmentCount);
    RIVE_DEBUG_CODE(++m_pushedStrokeCapCount;)
    RIVE_DEBUG_CODE(++m_pushedEmptyStrokeCountForCaps;)
}

bool PLSRenderer::IsAABB(const RawPath& path, AABB* result)
{
    // Any quadrilateral begins with a move plus 3 lines.
    constexpr static size_t kAABBVerbCount = 4;
    constexpr static PathVerb aabbVerbs[kAABBVerbCount] = {PathVerb::move,
                                                           PathVerb::line,
                                                           PathVerb::line,
                                                           PathVerb::line};
    Span<const PathVerb> verbs = path.verbs();
    if (verbs.count() < kAABBVerbCount || memcmp(verbs.data(), aabbVerbs, sizeof(aabbVerbs)) != 0)
    {
        return false;
    }

    // Only accept extra verbs and points if every point after the quadrilateral is equal to p0.
    Span<const Vec2D> pts = path.points();
    for (size_t i = 4; i < pts.count(); ++i)
    {
        if (pts[i] != pts[0])
        {
            return false;
        }
    }

    // We have a quadrilateral! Now check if it is an axis-aligned rectangle.
    float4 corners = {pts[0].x, pts[0].y, pts[2].x, pts[2].y};
    float4 oppositeCorners = {pts[1].x, pts[1].y, pts[3].x, pts[3].y};
    if (simd::all(corners == oppositeCorners.zyxw) || simd::all(corners == oppositeCorners.xwzy))
    {
        float4 r = simd::join(simd::min(corners.xy, corners.zw), simd::max(corners.xy, corners.zw));
        simd::store(result, r);
        return true;
    }
    return false;
}
} // namespace rive::pls
