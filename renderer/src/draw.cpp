/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/draw.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "path_utils.hpp"
#include "rive_render_path.hpp"
#include "rive_render_paint.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/renderer/image.hpp"
#include "shaders/constants.glsl"

namespace rive::gpu
{
namespace
{
constexpr static int kNumSegmentsInMiterOrBevelJoin = 5;
constexpr static int kStrokeStyleFlag = 8;
constexpr static int kRoundJoinStyleFlag = kStrokeStyleFlag << 1;
RIVE_ALWAYS_INLINE constexpr int style_flags(bool isStroked, bool roundJoinStroked)
{
    int styleFlags = (isStroked << 3) | (roundJoinStroked << 4);
    assert(bool(styleFlags & kStrokeStyleFlag) == isStroked);
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
    return tan0;
}
RIVE_ALWAYS_INLINE Vec2D find_cubic_tan1(const Vec2D p[4])
{
    Vec2D tan1 = p[3] - (p[3] != p[2] ? p[2] : p[2] != p[1] ? p[1] : p[0]);
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
        t[i * 2 + 0] = fmaxf(cuspT[i] - math::EPSILON, minT);
        t[i * 2 + 1] = fminf(cuspT[i] + math::EPSILON, maxT);
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
static StrokeCap empty_stroke_cap(bool closed, StrokeJoin join, StrokeCap cap)
{
    if (closed)
    {
        switch (join)
        {
            case StrokeJoin::round:
                return StrokeCap::round;
            case StrokeJoin::miter:
                return StrokeCap::square;
            case StrokeJoin::bevel:
                return StrokeCap::butt;
        }
    }
    return cap;
}

RIVE_ALWAYS_INLINE bool is_final_verb_of_contour(const RawPath::Iter& iter,
                                                 const RawPath::Iter& end)
{
    return iter.rawVerbsPtr() + 1 == end.rawVerbsPtr();
}

RIVE_ALWAYS_INLINE uint32_t join_type_flags(StrokeJoin join)
{
    switch (join)
    {
        case StrokeJoin::miter:
            return MITER_REVERT_JOIN_CONTOUR_FLAG;
        case StrokeJoin::round:
            return 0;
        case StrokeJoin::bevel:
            return BEVEL_JOIN_CONTOUR_FLAG;
    }
    RIVE_UNREACHABLE();
}
} // namespace

PLSDraw::PLSDraw(IAABB pixelBounds,
                 const Mat2D& matrix,
                 BlendMode blendMode,
                 rcp<const PLSTexture> imageTexture,
                 Type type) :
    m_imageTextureRef(imageTexture.release()),
    m_pixelBounds(pixelBounds),
    m_matrix(matrix),
    m_blendMode(blendMode),
    m_type(type)
{
    if (m_blendMode != BlendMode::srcOver)
    {
        m_drawContents |= gpu::DrawContents::advancedBlend;
    }
}

void PLSDraw::setClipID(uint32_t clipID)
{
    m_clipID = clipID;

    // For clipUpdates, m_clipID refers to the ID we are writing to the stencil buffer (NOT the ID
    // we are clipping against). It therefore doesn't affect the activeClip flag in that case.
    if (!(m_drawContents & gpu::DrawContents::clipUpdate))
    {
        if (m_clipID != 0)
        {
            m_drawContents |= gpu::DrawContents::activeClip;
        }
        else
        {
            m_drawContents &= ~gpu::DrawContents::activeClip;
        }
    }
}

bool PLSDraw::allocateGradientIfNeeded(PLSRenderContext::LogicalFlush* flush,
                                       ResourceCounters* counters)
{
    return m_gradientRef == nullptr ||
           flush->allocateGradient(m_gradientRef, counters, &m_simplePaintValue.colorRampLocation);
}

void PLSDraw::releaseRefs()
{
    safe_unref(m_imageTextureRef);
    safe_unref(m_gradientRef);
}

PLSDrawUniquePtr RiveRenderPathDraw::Make(PLSRenderContext* context,
                                          const Mat2D& matrix,
                                          rcp<const RiveRenderPath> path,
                                          FillRule fillRule,
                                          const RiveRenderPaint* paint,
                                          RawPath* scratchPath)
{
    assert(path != nullptr);
    assert(paint != nullptr);
    AABB mappedBounds;
    if (context->frameInterlockMode() == gpu::InterlockMode::atomics)
    {
        // In atomic mode, find a tighter bounding box in order to maximize reordering.
        mappedBounds = matrix.mapBoundingBox(path->getRawPath().points().data(),
                                             path->getRawPath().points().count());
    }
    else
    {
        // Otherwise we can get away with just mapping the path's bounding box.
        mappedBounds = matrix.mapBoundingBox(path->getBounds());
    }
    assert(mappedBounds.width() >= 0);
    assert(mappedBounds.height() >= 0);
    if (paint->getIsStroked())
    {
        // Outset the path's bounding box to account for stroking.
        float strokeOutset = paint->getThickness() * .5f;
        if (paint->getJoin() == StrokeJoin::miter)
        {
            strokeOutset *= 4;
        }
        else if (paint->getCap() == StrokeCap::square)
        {
            strokeOutset *= math::SQRT2;
        }
        AABB strokePixelOutset = matrix.mapBoundingBox({0, 0, strokeOutset, strokeOutset});
        mappedBounds = mappedBounds.inset(-strokePixelOutset.width(), -strokePixelOutset.height());
    }
    IAABB pixelBounds = mappedBounds.roundOut();
    if (!paint->getIsStroked())
    {
        // Use interior triangulation to draw filled paths if they're large enough to benefit from
        // it.
        const AABB& localBounds = path->getBounds();
        // FIXME! Implement interior triangulation in depthStencil mode.
        if (context->frameInterlockMode() != gpu::InterlockMode::depthStencil &&
            path->getRawPath().verbs().count() < 1000 &&
            gpu::FindTransformedArea(localBounds, matrix) > 512 * 512)
        {
            return PLSDrawUniquePtr(context->make<InteriorTriangulationDraw>(
                context,
                pixelBounds,
                matrix,
                std::move(path),
                fillRule,
                paint,
                scratchPath,
                localBounds.width() > localBounds.height()
                    ? InteriorTriangulationDraw::TriangulatorAxis::horizontal
                    : InteriorTriangulationDraw::TriangulatorAxis::vertical));
        }
    }
    return PLSDrawUniquePtr(context->make<MidpointFanPathDraw>(context,
                                                               pixelBounds,
                                                               matrix,
                                                               std::move(path),
                                                               fillRule,
                                                               paint));
}

RiveRenderPathDraw::RiveRenderPathDraw(IAABB pixelBounds,
                                       const Mat2D& matrix,
                                       rcp<const RiveRenderPath> path,
                                       FillRule fillRule,
                                       const RiveRenderPaint* paint,
                                       Type type,
                                       gpu::InterlockMode frameInterlockMode) :
    PLSDraw(pixelBounds, matrix, paint->getBlendMode(), ref_rcp(paint->getImageTexture()), type),
    m_pathRef(path.release()),
    m_fillRule(paint->getIsStroked() ? FillRule::nonZero : fillRule),
    m_paintType(paint->getType())
{
    assert(m_pathRef != nullptr);
    assert(!m_pathRef->getRawPath().empty());
    assert(paint != nullptr);
    if (m_blendMode == BlendMode::srcOver && paint->getIsOpaque())
    {
        m_drawContents |= gpu::DrawContents::opaquePaint;
    }
    if (paint->getIsStroked())
    {
        m_drawContents |= gpu::DrawContents::stroke;
        m_strokeRadius = paint->getThickness() * .5f;
        // Ensure stroke radius is nonzero. (In PLS, zero radius means the path is filled.)
        m_strokeRadius = fmaxf(m_strokeRadius, std::numeric_limits<float>::min());
        assert(!std::isnan(m_strokeRadius)); // These should get culled in RiveRenderer::drawPath().
        assert(m_strokeRadius > 0);
    }
    else if (m_fillRule == FillRule::evenOdd)
    {
        m_drawContents |= gpu::DrawContents::evenOddFill;
    }
    if (paint->getType() == gpu::PaintType::clipUpdate)
    {
        m_drawContents |= gpu::DrawContents::clipUpdate;
        if (paint->getSimpleValue().outerClipID != 0)
        {
            m_drawContents |= gpu::DrawContents::activeClip;
        }
    }

    if (isStroked())
    {
        // Stroke triangles are always forward.
        m_contourDirections = gpu::ContourDirections::forward;
    }
    else if (frameInterlockMode != gpu::InterlockMode::depthStencil)
    {
        // atomic and rasterOrdering fills need reverse AND forward triangles.
        m_contourDirections = gpu::ContourDirections::reverseAndForward;
    }
    else if (m_fillRule != FillRule::evenOdd)
    {
        // Emit "nonZero" depthStencil fills in a direction such that the dominant triangle winding
        // area is always clockwise. This maximizes pixel throughput since we will draw
        // counterclockwise triangles twice and clockwise only once.
        float matrixDeterminant = matrix[0] * matrix[3] - matrix[2] * matrix[1];
        m_contourDirections = m_pathRef->getCoarseArea() * matrixDeterminant >= 0
                                  ? gpu::ContourDirections::forward
                                  : gpu::ContourDirections::reverse;
    }
    else
    {
        // "evenOdd" depthStencil fils just get drawn twice, so any direction is fine.
        m_contourDirections = gpu::ContourDirections::forward;
    }

    m_simplePaintValue = paint->getSimpleValue();
    m_gradientRef = safe_ref(paint->getGradient());
    RIVE_DEBUG_CODE(m_pathRef->lockRawPathMutations();)
    RIVE_DEBUG_CODE(m_rawPathMutationID = m_pathRef->getRawPathMutationID();)
    assert(isStroked() == (strokeRadius() > 0));
}

void RiveRenderPathDraw::pushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    // Make sure the rawPath in our path reference hasn't changed since we began holding!
    assert(m_rawPathMutationID == m_pathRef->getRawPathMutationID());
    assert(!m_pathRef->getRawPath().empty());

    size_t tessVertexCount = m_type == Type::midpointFanPath
                                 ? m_resourceCounts.midpointFanTessVertexCount
                                 : m_resourceCounts.outerCubicTessVertexCount;
    if (tessVertexCount > 0)
    {
        // Push a path record.
        flush->pushPath(this,
                        m_type == Type::midpointFanPath ? PatchType::midpointFan
                                                        : PatchType::outerCurves,
                        math::lossless_numeric_cast<uint32_t>(tessVertexCount));

        onPushToRenderContext(flush);
    }
}

void RiveRenderPathDraw::releaseRefs()
{
    PLSDraw::releaseRefs();
    RIVE_DEBUG_CODE(m_pathRef->unlockRawPathMutations();)
    m_pathRef->unref();
}

MidpointFanPathDraw::MidpointFanPathDraw(PLSRenderContext* context,
                                         IAABB pixelBounds,
                                         const Mat2D& matrix,
                                         rcp<const RiveRenderPath> path,
                                         FillRule fillRule,
                                         const RiveRenderPaint* paint) :
    RiveRenderPathDraw(pixelBounds,
                       matrix,
                       std::move(path),
                       fillRule,
                       paint,
                       Type::midpointFanPath,
                       context->frameInterlockMode())
{
    if (isStroked())
    {
        m_strokeMatrixMaxScale = m_matrix.findMaxScale();
        m_strokeJoin = paint->getJoin();
        m_strokeCap = paint->getCap();
    }

    // Count up how much temporary storage this function will need to reserve in CPU buffers.
    const RawPath& rawPath = m_pathRef->getRawPath();
    size_t contourCount = rawPath.countMoveTos();
    assert(contourCount != 0);

    m_contours = reinterpret_cast<ContourInfo*>(
        context->perFrameAllocator().alloc(sizeof(ContourInfo) * contourCount));

    size_t maxStrokedCurvesBeforeChops = 0;
    size_t maxCurves = 0;
    size_t maxRotations = 0;
    // Reserve enough space to record all the info we might need for this path.
    assert(rawPath.verbs()[0] == PathVerb::move);
    // Every path has at least 1 (non-curve) move.
    size_t pathMaxLinesOrCurvesBeforeChops = rawPath.verbs().size() - 1;
    // Stroked cubics can be chopped into a maximum of 5 segments.
    size_t pathMaxLinesOrCurvesAfterChops =
        isStroked() ? pathMaxLinesOrCurvesBeforeChops * 5 : pathMaxLinesOrCurvesBeforeChops;
    maxCurves += pathMaxLinesOrCurvesAfterChops;
    if (isStroked())
    {
        maxStrokedCurvesBeforeChops += pathMaxLinesOrCurvesBeforeChops;
        maxRotations += pathMaxLinesOrCurvesAfterChops;
        if (m_strokeJoin == StrokeJoin::round)
        {
            // If the stroke has round joins, we also record the rotations between (pre-chopped)
            // joins in order to calculate how many vertices are in each round join.
            maxRotations += pathMaxLinesOrCurvesBeforeChops;
        }
    }

    // Each stroked curve will record the number of chops it requires (either 0, 1, or 2).
    size_t maxChops = maxStrokedCurvesBeforeChops;
    // We only chop into this queue if a cubic has one chop. More chops in a single cubic
    // are rare and require a lot of memory, so if a cubic needs more chops we just re-chop
    // the second time around. The maximum size this queue would need is therefore enough to
    // chop each cubic once, or 5 internal points per chop.
    size_t maxChopVertices = maxStrokedCurvesBeforeChops * 5;
    // +3 for each contour because we align each contour's curves and rotations on multiples of 4.
    size_t maxPaddedRotations = isStroked() ? maxRotations + contourCount * 3 : 0;
    size_t maxPaddedCurves = maxCurves + contourCount * 3;

    // Reserve intermediate space for the polar segment counts of each curve and round join.
    if (isStroked())
    {
        m_numChops.reset(context->numChopsAllocator(), maxChops);
        m_chopVertices.reset(context->chopVerticesAllocator(), maxChopVertices);
        m_tangentPairs = context->tangentPairsAllocator().alloc(maxPaddedRotations);
        m_polarSegmentCounts = context->polarSegmentCountsAllocator().alloc(maxPaddedRotations);
    }
    m_parametricSegmentCounts = context->parametricSegmentCountsAllocator().alloc(maxPaddedCurves);

    size_t lineCount = 0;
    size_t unpaddedCurveCount = 0;
    size_t unpaddedRotationCount = 0;
    size_t emptyStrokeCountForCaps = 0;

    // Iteration pass 1: Collect information on contour and curves counts for every path in the
    // batch, and begin counting tessellated vertices.
    size_t contourIdx = 0;
    size_t curveIdx = 0;
    size_t rotationIdx = 0; // We measure rotations on both curves and round joins.
    bool roundJoinStroked = isStroked() && m_strokeJoin == StrokeJoin::round;
    wangs_formula::VectorXform vectorXform(m_matrix);
    RawPath::Iter startOfContour = rawPath.begin();
    RawPath::Iter end = rawPath.end();
    int preChopVerbCount = 0; // Original number of lines and curves, before chopping.
    Vec2D endpointsSum{};
    bool closed = !isStroked();
    Vec2D lastTangent = {0, 1};
    Vec2D firstTangent = {0, 1};
    size_t roundJoinCount = 0;
    size_t contourFirstCurveIdx = curveIdx;
    assert(contourFirstCurveIdx % 4 == 0);
    size_t contourFirstRotationIdx = rotationIdx;
    assert(contourFirstRotationIdx % 4 == 0);
    auto finishAndAppendContour = [&](RawPath::Iter iter) {
        if (closed)
        {
            Vec2D finalPtInContour = iter.rawPtsPtr()[-1];
            // Bit-cast to uint64_t because we don't want the special equality rules for NaN. If
            // we're empty or otherwise return back to p0, we want to detect this, regardless of
            // whether there are NaN values.
            if (math::bit_cast<uint64_t>(startOfContour.movePt()) !=
                math::bit_cast<uint64_t>(finalPtInContour))
            {
                assert(preChopVerbCount > 0);
                if (roundJoinStroked)
                {
                    // Round join before implicit closing line.
                    Vec2D tangent = startOfContour.movePt() - finalPtInContour;
                    assert(rotationIdx < maxPaddedRotations);
                    m_tangentPairs[rotationIdx++] = {lastTangent, tangent};
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
                assert(rotationIdx < maxPaddedRotations);
                m_tangentPairs[rotationIdx++] = {lastTangent, firstTangent};
                ++roundJoinCount;
            }
        }
        size_t strokeJoinCount = preChopVerbCount;
        if (!closed)
        {
            strokeJoinCount = std::max<size_t>(strokeJoinCount, 1) - 1;
        }
        assert(contourIdx < contourCount);
        m_contours[contourIdx++] = {
            iter,
            lineCount,
            contourFirstCurveIdx,
            curveIdx,
            contourFirstRotationIdx,
            rotationIdx,
            isStroked() ? Vec2D() : endpointsSum * (1.f / preChopVerbCount),
            closed,
            strokeJoinCount,
            0,                 // strokeCapSegmentCount
            0,                 // paddingVertexCount
            RIVE_DEBUG_CODE(0) // tessVertexCount
        };
        unpaddedCurveCount += curveIdx - contourFirstCurveIdx;
        contourFirstCurveIdx = curveIdx = math::round_up_to_multiple_of<4>(curveIdx);
        unpaddedRotationCount += rotationIdx - contourFirstRotationIdx;
        contourFirstRotationIdx = rotationIdx = math::round_up_to_multiple_of<4>(rotationIdx);
    };
    const int styleFlags = style_flags(isStroked(), roundJoinStroked);
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
                closed = !isStroked();
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
                    assert(rotationIdx < maxPaddedRotations);
                    m_tangentPairs[rotationIdx++] = {lastTangent, tangent};
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
                    assert(rotationIdx < maxPaddedRotations);
                    m_tangentPairs[rotationIdx++] = {lastTangent, unchoppedTangents[0]};
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
                                           // We have to chop carefully around stroked cusps in
                                           // order to avoid rendering artifacts. Luckily, cusps
                                           // are extremely rare in real-world content.
                        m_chopVertices.push_back() = {t[0], t[1]};
                        chop_cubic_around_cusps(p,
                                                localChopBuffer,
                                                t,
                                                numChops,
                                                m_strokeMatrixMaxScale);
                        p = localChopBuffer;
                        numChops *= 2;
                        break;
                    case simple_chop_key(2): // 2 non-cusp chops
                        m_chopVertices.push_back() = {t[0], t[1]};
                        pathutils::ChopCubicAt(p, localChopBuffer, t[0], t[1]);
                        p = localChopBuffer;
                        break;
                    case simple_chop_key(1): // 1 non-cusp chop
                    {
                        pathutils::ChopCubicAt(p, localChopBuffer, t[0]);
                        p = localChopBuffer;
                        memcpy(m_chopVertices.push_back_n(5), p + 1, sizeof(Vec2D) * 5);
                        break;
                    }
                }
                // Calculate segment counts for each chopped section independently.
                for (const Vec2D* end = p + numChops * 3 + 3; p != end;
                     p += 3, ++curveIdx, ++rotationIdx)
                {
                    float n4 = wangs_formula::cubic_pow4(p, kParametricPrecision, vectorXform);
                    // Record n^4 for now. This will get resolved later.
                    assert(curveIdx < maxPaddedCurves);
                    RIVE_INLINE_MEMCPY(m_parametricSegmentCounts + curveIdx, &n4, sizeof(uint32_t));
                    assert(rotationIdx < maxPaddedRotations);
                    find_cubic_tangents(p, m_tangentPairs[rotationIdx].data());
                }
                break;
            }
            case StyledVerb::filledCubic:
            {
                const Vec2D* p = iter.cubicPts();
                ++preChopVerbCount;
                endpointsSum += p[3];
                float n4 = wangs_formula::cubic_pow4(p, kParametricPrecision, vectorXform);
                // Record n^4 for now. This will get resolved later.
                assert(curveIdx < maxPaddedCurves);
                RIVE_INLINE_MEMCPY(m_parametricSegmentCounts + curveIdx++, &n4, sizeof(uint32_t));
                break;
            }
        }
    }
    if (startOfContour != end)
    {
        finishAndAppendContour(end);
    }
    assert(contourIdx == contourCount);
    assert(contourCount > 0);
    assert(curveIdx <= maxPaddedCurves);
    assert(rotationIdx <= maxPaddedRotations);
    assert(curveIdx % 4 == 0);    // Because we write parametric segment counts in batches of 4.
    assert(rotationIdx % 4 == 0); // Because we write polar segment counts in batches of 4.
    assert(isStroked() || maxPaddedRotations == 0);
    assert(isStroked() || rotationIdx == 0);

    // Return any data we conservatively allocated but did not use.
    if (isStroked())
    {
        m_numChops.shrinkToFit(context->numChopsAllocator(), maxChops);
        m_chopVertices.shrinkToFit(context->chopVerticesAllocator(), maxChopVertices);
        context->tangentPairsAllocator().rewindLastAllocation(maxPaddedRotations - rotationIdx);
        context->polarSegmentCountsAllocator().rewindLastAllocation(maxPaddedRotations -
                                                                    rotationIdx);
    }
    context->parametricSegmentCountsAllocator().rewindLastAllocation(maxPaddedCurves - curveIdx);

    // Iteration pass 2: Finish calculating the numbers of tessellation segments in each contour,
    // using SIMD.
    size_t contourFirstLineIdx = 0;
    size_t tessVertexCount = 0;
    for (size_t i = 0; i < contourCount; ++i)
    {
        ContourInfo* contour = &m_contours[i];
        size_t contourLineCount = contour->endLineIdx - contourFirstLineIdx;
        uint32_t contourVertexCount = math::lossless_numeric_cast<uint32_t>(
            contourLineCount * 2); // Each line tessellates to 2 vertices.
        uint4 mergedTessVertexSums4 = 0;

        // Finish calculating and counting parametric segments for each curve.
        size_t j;
        for (j = contour->firstCurveIdx; j < contour->endCurveIdx; j += 4)
        {
            // Curves recorded their segment counts raised to the 4th power. Now find their
            // roots and convert to integers in batches of 4.
            assert(j + 4 <= curveIdx);
            float4 n = simd::load4f(m_parametricSegmentCounts + j);
            n = simd::ceil(simd::sqrt(simd::sqrt(n)));
            n = simd::clamp(n, float4(1), float4(kMaxParametricSegments));
            uint4 n_ = simd::cast<uint32_t>(n);
            assert(j + 4 <= curveIdx);
            simd::store(m_parametricSegmentCounts + j, n_);
            mergedTessVertexSums4 += n_;
        }
        // We counted in batches of 4. Undo the values we counted from beyond the end of the
        // path.
        while (j-- > contour->endCurveIdx)
        {
            contourVertexCount -= m_parametricSegmentCounts[j];
        }

        if (isStroked())
        {
            // Finish calculating and counting polar segments for each stroked curve and
            // round join.
            const float r_ = m_strokeRadius * m_strokeMatrixMaxScale;
            const float polarSegmentsPerRad =
                pathutils::CalcPolarSegmentsPerRadian<kPolarPrecision>(r_);
            for (j = contour->firstRotationIdx; j < contour->endRotationIdx; j += 4)
            {
                // Measure the rotations of curves in batches of 4.
                assert(j + 4 <= rotationIdx);

                float4 tx0, ty0, tx1, ty1;
                std::tie(tx0, ty0, tx1, ty1) = simd::load4x4f(&m_tangentPairs[j][0].x);

                float4 numer = tx0 * tx1 + ty0 * ty1;
                float4 denom_pow2 = (tx0 * tx0 + ty0 * ty0) * (tx1 * tx1 + ty1 * ty1);
                float4 cosTheta = numer / simd::sqrt(denom_pow2);
                cosTheta = simd::clamp(cosTheta, float4(-1), float4(1));
                float4 theta = simd::fast_acos(cosTheta);
                // Find polar segment counts from the rotation angles.
                float4 n = simd::ceil(theta * polarSegmentsPerRad);
                n = simd::clamp(n, float4(1), float4(kMaxPolarSegments));
                uint4 n_ = simd::cast<uint32_t>(n);
                assert(j + 4 <= rotationIdx);
                simd::store(m_polarSegmentCounts + j, n_);
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
            if (m_strokeJoin == StrokeJoin::round)
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
                         contour->endCurveIdx == contour->firstCurveIdx;
            StrokeCap cap;
            bool needsCaps;
            if (!empty)
            {
                cap = m_strokeCap;
                needsCaps = !contour->closed;
            }
            else
            {
                cap = empty_stroke_cap(contour->closed, m_strokeJoin, m_strokeCap);
                needsCaps = cap != StrokeCap::butt; // Ignore butt caps when the contour is empty.
            }
            if (needsCaps)
            {
                // We emulate stroke caps as 180-degree joins.
                if (cap == StrokeCap::round)
                {
                    // Round caps rotate 180 degrees.
                    float strokeCapSegmentCount = ceilf(polarSegmentsPerRad * math::PI);
                    // +2 because round caps emulated as joins need to emit vertices at T=0
                    // and T=1, unlike normal round joins.
                    strokeCapSegmentCount += 2;
                    // Make sure not to exceed kMaxPolarSegments.
                    strokeCapSegmentCount = fminf(strokeCapSegmentCount, kMaxPolarSegments);
                    contour->strokeCapSegmentCount = static_cast<uint32_t>(strokeCapSegmentCount);
                }
                else
                {
                    contour->strokeCapSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                }
                // PLS expects all patches to have >0 tessellation vertices, so for the case of an
                // empty patch with a stroke cap, contour->strokeCapSegmentCount can't be zero.
                // Also, pushContourToRenderContext() uses "strokeCapSegmentCount != 0" to tell if
                // it needs stroke caps.
                assert(contour->strokeCapSegmentCount >= 2);
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
            size_t contourCurveCount = contour->endCurveIdx - contour->firstCurveIdx;
            contourVertexCount += contourCurveCount;
        }
        contourVertexCount += simd::reduce_add(mergedTessVertexSums4);

        // Add padding vertices until the number of tessellation vertices in the contour is
        // an exact multiple of kMidpointFanPatchSegmentSpan. This ensures that patch
        // boundaries align with contour boundaries.
        contour->paddingVertexCount =
            PaddingToAlignUp<kMidpointFanPatchSegmentSpan>(contourVertexCount);
        contourVertexCount += contour->paddingVertexCount;
        assert(contourVertexCount % kMidpointFanPatchSegmentSpan == 0);
        RIVE_DEBUG_CODE(contour->tessVertexCount = contourVertexCount;)

        tessVertexCount += contourVertexCount;
        contourFirstLineIdx = contour->endLineIdx;
    }

    assert(contourFirstLineIdx == lineCount);
    RIVE_DEBUG_CODE(m_pendingLineCount = lineCount);
    RIVE_DEBUG_CODE(m_pendingCurveCount = unpaddedCurveCount);
    RIVE_DEBUG_CODE(m_pendingRotationCount = unpaddedRotationCount);
    RIVE_DEBUG_CODE(m_pendingEmptyStrokeCountForCaps = emptyStrokeCountForCaps);

    if (tessVertexCount > 0)
    {
        m_resourceCounts.pathCount = 1;
        m_resourceCounts.contourCount = contourCount;
        // maxTessellatedSegmentCount does not get doubled when we emit both forward and mirrored
        // contours because the forward and mirrored pair both get packed into a single
        // gpu::TessVertexSpan.
        m_resourceCounts.maxTessellatedSegmentCount =
            lineCount + unpaddedCurveCount + emptyStrokeCountForCaps;
        m_resourceCounts.midpointFanTessVertexCount =
            m_contourDirections == gpu::ContourDirections::reverseAndForward ? tessVertexCount * 2
                                                                             : tessVertexCount;
    }
}

void MidpointFanPathDraw::onPushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    const RawPath& rawPath = m_pathRef->getRawPath();
    RawPath::Iter startOfContour = rawPath.begin();
    for (size_t i = 0; i < m_resourceCounts.contourCount; ++i)
    {
        // Push a contour and curve records.
        const ContourInfo& contour = m_contours[i];
        assert(startOfContour.verb() == PathVerb::move);
        assert(isStroked() || contour.closed); // Fills are always closed.
        RIVE_DEBUG_CODE(m_pendingStrokeJoinCount = isStroked() ? contour.strokeJoinCount : 0;)
        RIVE_DEBUG_CODE(m_pendingStrokeCapCount = contour.strokeCapSegmentCount != 0 ? 2 : 0;)

        const Vec2D* pts = startOfContour.rawPtsPtr();
        size_t curveIdx = contour.firstCurveIdx;
        size_t rotationIdx = contour.firstRotationIdx;
        const RawPath::Iter end = contour.endOfContour;
        uint32_t joinTypeFlags = 0;
        bool roundJoinStroked = false;
        bool needsFirstEmulatedCapAsJoin = false; // Emit a starting cap before the next cubic?
        uint32_t emulatedCapAsJoinFlags = 0;
        if (isStroked())
        {
            joinTypeFlags = join_type_flags(m_strokeJoin);
            roundJoinStroked = joinTypeFlags == 0;
            if (contour.strokeCapSegmentCount != 0)
            {
                StrokeCap cap = !contour.closed ? m_strokeCap
                                                : empty_stroke_cap(true, m_strokeJoin, m_strokeCap);
                emulatedCapAsJoinFlags = EMULATED_STROKE_CAP_CONTOUR_FLAG;
                if (cap == StrokeCap::square)
                {
                    emulatedCapAsJoinFlags |= MITER_CLIP_JOIN_CONTOUR_FLAG;
                }
                else if (cap == StrokeCap::butt)
                {
                    emulatedCapAsJoinFlags |= BEVEL_JOIN_CONTOUR_FLAG;
                }
                needsFirstEmulatedCapAsJoin = true;
            }
        }

        // Make a data record for this current contour on the GPU.
        flush->pushContour(contour.midpoint, contour.closed, contour.paddingVertexCount);

        // Convert all curves in the contour to cubics and push them to the GPU.
        const int styleFlags = style_flags(isStroked(), roundJoinStroked);
        Vec2D joinTangent = {0, 1};
        int joinSegmentCount = 1;
        Vec2D implicitClose[2]; // In case we need an implicit closing line.
        for (auto iter = startOfContour; iter != end; ++iter)
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
                        RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                        RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                    }
                    else
                    {
                        // End with a 180-degree join that looks like the stroke cap.
                        joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                        joinTypeFlags = emulatedCapAsJoinFlags;
                        joinSegmentCount = contour.strokeCapSegmentCount;
                        RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
                    }
                    goto line_common;
                }
                case StyledVerb::strokedLine:
                    if (contour.closed || !is_final_verb_of_contour(iter, end))
                    {
                        joinTangent = find_join_tangent(iter.linePts() + 1,
                                                        end.rawPtsPtr(),
                                                        contour.closed,
                                                        pts);
                        joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                        RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                    }
                    else
                    {
                        // End with a 180-degree join that looks like the stroke cap.
                        joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                        joinTypeFlags = emulatedCapAsJoinFlags;
                        joinSegmentCount = contour.strokeCapSegmentCount;
                        RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
                    }
                    [[fallthrough]];
                case StyledVerb::filledLine:
                line_common:
                {
                    std::array<Vec2D, 4> cubic = convert_line_to_cubic(iter.linePts());
                    if (needsFirstEmulatedCapAsJoin)
                    {
                        // Emulate the start cap as a 180-degree join before the first stroke.
                        pushEmulatedStrokeCapAsJoinBeforeCubic(flush,
                                                               cubic.data(),
                                                               emulatedCapAsJoinFlags,
                                                               contour.strokeCapSegmentCount);
                        needsFirstEmulatedCapAsJoin = false;
                    }
                    flush->pushCubic(cubic.data(),
                                     joinTangent,
                                     joinTypeFlags,
                                     1,
                                     1,
                                     joinSegmentCount);
                    RIVE_DEBUG_CODE(--m_pendingLineCount;)
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
                                                    &m_chopVertices.pop_front().x,
                                                    chopKey >> 1,
                                                    m_strokeMatrixMaxScale);
                            p = localChopBuffer;
                            // The bottom bit of chopKey is 1, meaning "areCusps". Clearing the
                            // bottom bit leaves "numChops * 2", which is the number of chops a cusp
                            // needs!
                            numChops = chopKey ^ 1;
                            break;

                        case simple_chop_key(2): // 2 non-cusp chops
                        {
                            // Curves that need 2 chops are rare in real-world content. Just re-chop
                            // the curve this time around as well.
                            auto [t0, t1] = m_chopVertices.pop_front();
                            pathutils::ChopCubicAt(p, localChopBuffer, t0, t1);
                            p = localChopBuffer;
                            numChops = 2;
                            break;
                        }
                        case simple_chop_key(1): // 1 non-cusp chop
                            // Single-chop curves were saved in the m_chopVertices queue.
                            localChopBuffer[0] = p[0];
                            memcpy(localChopBuffer + 1,
                                   m_chopVertices.pop_front_n(5),
                                   sizeof(Vec2D) * 5);
                            localChopBuffer[6] = p[3];
                            p = localChopBuffer;
                            numChops = 1;
                            break;
                    }
                    if (needsFirstEmulatedCapAsJoin)
                    {
                        // Emulate the start cap as a 180-degree join before the first stroke.
                        pushEmulatedStrokeCapAsJoinBeforeCubic(flush,
                                                               p,
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
                        flush->pushCubic(p,
                                         joinTangent,
                                         joinTypeFlags,
                                         parametricSegmentCount,
                                         polarSegmentCount,
                                         1);
                        RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                        RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    }
                    // Push the final chop, with a join.
                    uint32_t parametricSegmentCount = m_parametricSegmentCounts[curveIdx++];
                    uint32_t polarSegmentCount = m_polarSegmentCounts[rotationIdx++];
                    RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    if (contour.closed || !is_final_verb_of_contour(iter, end))
                    {
                        if (styledVerb == StyledVerb::roundJoinStrokedCubic)
                        {
                            joinTangent = m_tangentPairs[rotationIdx][1];
                            joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                            ++rotationIdx;
                            RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                        }
                        else
                        {
                            joinTangent = find_join_tangent(iter.cubicPts() + 3,
                                                            end.rawPtsPtr(),
                                                            contour.closed,
                                                            pts);
                            joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                        }
                        RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                    }
                    else
                    {
                        // End with a 180-degree join that looks like the stroke cap.
                        joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
                        joinTypeFlags = emulatedCapAsJoinFlags;
                        joinSegmentCount = contour.strokeCapSegmentCount;
                        RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
                    }
                    flush->pushCubic(p,
                                     joinTangent,
                                     joinTypeFlags,
                                     parametricSegmentCount,
                                     polarSegmentCount,
                                     joinSegmentCount);
                    RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                    break;
                }
                case StyledVerb::filledCubic:
                {
                    uint32_t parametricSegmentCount = m_parametricSegmentCounts[curveIdx++];
                    flush->pushCubic(iter.cubicPts(), Vec2D{}, 0, parametricSegmentCount, 1, 1);
                    RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                    break;
                }
            }
        }

        if (needsFirstEmulatedCapAsJoin)
        {
            // The contour was empty. Emit both caps on p0.
            Vec2D p0 = pts[0], left = {p0.x - 1, p0.y}, right = {p0.x + 1, p0.y};
            pushEmulatedStrokeCapAsJoinBeforeCubic(flush,
                                                   std::array{p0, right, right, right}.data(),
                                                   emulatedCapAsJoinFlags,
                                                   contour.strokeCapSegmentCount);
            pushEmulatedStrokeCapAsJoinBeforeCubic(flush,
                                                   std::array{p0, left, left, left}.data(),
                                                   emulatedCapAsJoinFlags,
                                                   contour.strokeCapSegmentCount);
        }
        else if (contour.closed)
        {
            implicitClose[0] = end.rawPtsPtr()[-1];
            // Bit-cast to uint64_t because we don't want the special equality rules for NaN. If
            // we're empty or otherwise return back to p0, we want to detect this, regardless of
            // whether there are NaN values.
            if (math::bit_cast<uint64_t>(implicitClose[0]) !=
                math::bit_cast<uint64_t>(implicitClose[1]))
            {
                // Draw a line back to the beginning of the contour.
                std::array<Vec2D, 4> cubic = convert_line_to_cubic(implicitClose);
                // Closing join back to the beginning of the contour.
                if (roundJoinStroked)
                {
                    joinTangent = m_tangentPairs[rotationIdx][1];
                    joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                    ++rotationIdx;
                    RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                }
                else if (isStroked())
                {
                    joinTangent = find_starting_tangent(pts, end.rawPtsPtr());
                    joinSegmentCount = kNumSegmentsInMiterOrBevelJoin;
                    RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                }
                flush->pushCubic(cubic.data(), joinTangent, joinTypeFlags, 1, 1, joinSegmentCount);
                RIVE_DEBUG_CODE(--m_pendingLineCount;)
            }
        }

        assert(curveIdx == contour.endCurveIdx);
        assert(rotationIdx == contour.endRotationIdx);

        assert(m_pendingStrokeJoinCount == 0);
        assert(m_pendingStrokeCapCount == 0);
        startOfContour = contour.endOfContour;
    }

    // Make sure we only pushed the amount of data we reserved.
    assert(m_pendingLineCount == 0);
    assert(m_pendingCurveCount == 0);
    assert(m_pendingRotationCount == 0);
    assert(m_pendingEmptyStrokeCountForCaps == 0);
}

void MidpointFanPathDraw::pushEmulatedStrokeCapAsJoinBeforeCubic(
    PLSRenderContext::LogicalFlush* flush,
    const Vec2D cubic[],
    uint32_t emulatedCapAsJoinFlags,
    uint32_t strokeCapSegmentCount)
{
    // Reverse the cubic and push it with zero parametric and polar segments, and a 180-degree join
    // tangent. This results in a solitary join, positioned immediately before the provided cubic,
    // that looks like the desired stroke cap.
    assert(strokeCapSegmentCount >= 2);
    flush->pushCubic(std::array{cubic[3], cubic[2], cubic[1], cubic[0]}.data(),
                     find_cubic_tan0(cubic),
                     emulatedCapAsJoinFlags,
                     0,
                     0,
                     strokeCapSegmentCount);
    RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
    RIVE_DEBUG_CODE(--m_pendingEmptyStrokeCountForCaps;)
}

InteriorTriangulationDraw::InteriorTriangulationDraw(PLSRenderContext* context,
                                                     IAABB pixelBounds,
                                                     const Mat2D& matrix,
                                                     rcp<const RiveRenderPath> path,
                                                     FillRule fillRule,
                                                     const RiveRenderPaint* paint,
                                                     RawPath* scratchPath,
                                                     TriangulatorAxis triangulatorAxis) :
    RiveRenderPathDraw(pixelBounds,
                       matrix,
                       std::move(path),
                       fillRule,
                       paint,
                       Type::interiorTriangulationPath,
                       context->frameInterlockMode())
{
    assert(!isStroked());
    assert(m_strokeRadius == 0);
    processPath(PathOp::countDataAndTriangulate,
                &context->perFrameAllocator(),
                scratchPath,
                triangulatorAxis,
                nullptr);
}

void InteriorTriangulationDraw::onPushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    processPath(PathOp::submitOuterCubics, nullptr, nullptr, TriangulatorAxis::dontCare, flush);
    if (flush->desc().interlockMode == gpu::InterlockMode::atomics)
    {
        // We need a barrier between the outer cubics and interior triangles in atomic mode.
        flush->pushBarrier();
    }
    flush->pushInteriorTriangulation(this);
}

void InteriorTriangulationDraw::processPath(PathOp op,
                                            TrivialBlockAllocator* allocator,
                                            RawPath* scratchPath,
                                            TriangulatorAxis triangulatorAxis,
                                            PLSRenderContext::LogicalFlush* flush)
{
    Vec2D chops[kMaxCurveSubdivisions * 3 + 1];
    const RawPath& rawPath = m_pathRef->getRawPath();
    assert(!rawPath.empty());
    wangs_formula::VectorXform vectorXform(m_matrix);
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
                        flush->pushCubic(convert_line_to_cubic(pts[-1], p0).data(),
                                         {0, 0},
                                         CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG,
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
                    flush->pushContour({0, 0}, true, 0);
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
                    flush->pushCubic(convert_line_to_cubic(pts).data(),
                                     {0, 0},
                                     CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG,
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
                uint32_t numSubdivisions = FindSubdivisionCount(pts, vectorXform);
                if (numSubdivisions == 1)
                {
                    if (op == PathOp::countDataAndTriangulate)
                    {
                        scratchPath->line(pts[3]);
                    }
                    else
                    {
                        flush->pushCubic(pts,
                                         {0, 0},
                                         CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG,
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
                            flush->pushCubic(chop,
                                             {0, 0},
                                             CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG,
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
            flush->pushCubic(convert_line_to_cubic(lastPt, p0).data(),
                             {0, 0},
                             CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG,
                             kPatchSegmentCountExcludingJoin,
                             1,
                             kJoinSegmentCount);
        }
        ++patchCount;
    }

    if (op == PathOp::countDataAndTriangulate)
    {
        assert(m_triangulator == nullptr);
        assert(triangulatorAxis != TriangulatorAxis::dontCare);
        m_triangulator = allocator->make<GrInnerFanTriangulator>(
            *scratchPath,
            m_matrix,
            triangulatorAxis == TriangulatorAxis::horizontal
                ? GrTriangulator::Comparator::Direction::kHorizontal
                : GrTriangulator::Comparator::Direction::kVertical,
            m_fillRule,
            allocator);
        // We also draw each "grout" triangle using an outerCubic patch.
        patchCount += m_triangulator->groutList().count();

        if (patchCount > 0)
        {
            m_resourceCounts.pathCount = 1;
            m_resourceCounts.contourCount = contourCount;
            // maxTessellatedSegmentCount does not get doubled when we emit both forward and
            // mirrored contours because the forward and mirrored pair both get packed into a single
            // gpu::TessVertexSpan.
            m_resourceCounts.maxTessellatedSegmentCount = patchCount;
            // outerCubic patches emit their tessellated geometry twice: once forward and once
            // mirrored.
            m_resourceCounts.outerCubicTessVertexCount =
                m_contourDirections == gpu::ContourDirections::reverseAndForward
                    ? patchCount * kOuterCurvePatchSegmentSpan * 2
                    : patchCount * kOuterCurvePatchSegmentSpan;
            m_resourceCounts.maxTriangleVertexCount = m_triangulator->maxVertexCount();
        }
    }
    else
    {
        assert(m_triangulator != nullptr);
        // Submit grout triangles, retrofitted into outerCubic patches.
        for (auto* node = m_triangulator->groutList().head(); node; node = node->fNext)
        {
            Vec2D triangleAsCubic[4] = {node->fPts[0], node->fPts[1], {0, 0}, node->fPts[2]};
            flush->pushCubic(triangleAsCubic,
                             {0, 0},
                             RETROFITTED_TRIANGLE_CONTOUR_FLAG,
                             kPatchSegmentCountExcludingJoin,
                             1,
                             kJoinSegmentCount);
            ++patchCount;
        }
        assert(contourCount == m_resourceCounts.contourCount);
        assert(patchCount == m_resourceCounts.maxTessellatedSegmentCount);
        assert(patchCount * kOuterCurvePatchSegmentSpan * 2 ==
                   m_resourceCounts.outerCubicTessVertexCount ||
               patchCount * kOuterCurvePatchSegmentSpan ==
                   m_resourceCounts.outerCubicTessVertexCount);
    }
}

ImageRectDraw::ImageRectDraw(PLSRenderContext* context,
                             IAABB pixelBounds,
                             const Mat2D& matrix,
                             BlendMode blendMode,
                             rcp<const PLSTexture> imageTexture,
                             float opacity) :
    PLSDraw(pixelBounds, matrix, blendMode, std::move(imageTexture), Type::imageRect),
    m_opacity(opacity)
{
    // If we support image paints for paths, the client should draw a rectangular path with an
    // image paint instead of using this draw.
    assert(!context->frameSupportsImagePaintForPaths());
    m_resourceCounts.imageDrawCount = 1;
}

void ImageRectDraw::pushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    flush->pushImageRect(this);
}

ImageMeshDraw::ImageMeshDraw(IAABB pixelBounds,
                             const Mat2D& matrix,
                             BlendMode blendMode,
                             rcp<const PLSTexture> imageTexture,
                             rcp<const RenderBuffer> vertexBuffer,
                             rcp<const RenderBuffer> uvBuffer,
                             rcp<const RenderBuffer> indexBuffer,
                             uint32_t indexCount,
                             float opacity) :
    PLSDraw(pixelBounds, matrix, blendMode, std::move(imageTexture), Type::imageMesh),
    m_vertexBufferRef(vertexBuffer.release()),
    m_uvBufferRef(uvBuffer.release()),
    m_indexBufferRef(indexBuffer.release()),
    m_indexCount(indexCount),
    m_opacity(opacity)
{
    assert(m_vertexBufferRef != nullptr);
    assert(m_uvBufferRef != nullptr);
    assert(m_indexBufferRef != nullptr);
    m_resourceCounts.imageDrawCount = 1;
}

void ImageMeshDraw::pushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    flush->pushImageMesh(this);
}

void ImageMeshDraw::releaseRefs()
{
    PLSDraw::releaseRefs();
    m_vertexBufferRef->unref();
    m_uvBufferRef->unref();
    m_indexBufferRef->unref();
}

StencilClipReset::StencilClipReset(PLSRenderContext* context,
                                   uint32_t previousClipID,
                                   ResetAction resetAction) :
    PLSDraw(context->getClipContentBounds(previousClipID),
            Mat2D(),
            BlendMode::srcOver,
            nullptr,
            Type::stencilClipReset),
    m_previousClipID(previousClipID)
{
    switch (resetAction)
    {
        case ResetAction::intersectPreviousClip:
            m_drawContents |= gpu::DrawContents::activeClip;
            [[fallthrough]];
        case ResetAction::clearPreviousClip:
            m_drawContents |= gpu::DrawContents::clipUpdate;
            break;
    }
    m_resourceCounts.maxTriangleVertexCount = 6;
}

void StencilClipReset::pushToRenderContext(PLSRenderContext::LogicalFlush* flush)
{
    flush->pushStencilClipReset(this);
}
} // namespace rive::gpu
