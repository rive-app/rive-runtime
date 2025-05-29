/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/draw.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "rive_render_path.hpp"
#include "rive_render_paint.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/renderer/texture.hpp"
#include "gradient.hpp"
#include "shaders/constants.glsl"

namespace rive::gpu
{
namespace
{
// The final segment in an outerCurve patch is a bowtie join.
constexpr static size_t kJoinSegmentCount = 1;
constexpr static size_t kPatchSegmentCountExcludingJoin =
    kOuterCurvePatchSegmentSpan - kJoinSegmentCount;

// Maximum # of outerCurve patches a curve on the path can be subdivided into.
constexpr static size_t kMaxCurveSubdivisions =
    (kMaxParametricSegments + kPatchSegmentCountExcludingJoin - 1) /
    kPatchSegmentCountExcludingJoin;

static uint32_t find_outer_cubic_subdivision_count(
    const Vec2D pts[],
    const wangs_formula::VectorXform& vectorXform)
{
    float numSubdivisions =
        ceilf(wangs_formula::cubic(pts, kParametricPrecision, vectorXform) *
              (1.f / kPatchSegmentCountExcludingJoin));
    return static_cast<uint32_t>(
        math::clamp(numSubdivisions, 1, kMaxCurveSubdivisions));
}

constexpr static int NUM_SEGMENTS_IN_MITER_OR_BEVEL_JOIN = 5;
constexpr static int STROKE_OR_FEATHER_STYLE_FLAG = 8;
constexpr static int ROUND_JOIN_STYLE_FLAG = STROKE_OR_FEATHER_STYLE_FLAG << 1;
RIVE_ALWAYS_INLINE constexpr int style_flags(bool isStrokeOrFeather,
                                             bool roundJoinStroked)
{
    int styleFlags = (isStrokeOrFeather << 3) | (roundJoinStroked << 4);
    assert(bool(styleFlags & STROKE_OR_FEATHER_STYLE_FLAG) ==
           isStrokeOrFeather);
    assert(bool(styleFlags & ROUND_JOIN_STYLE_FLAG) == roundJoinStroked);
    return styleFlags;
}

// Switching on a StyledVerb reduces "if (stroked)" branching and makes the code
// cleaner.
enum class StyledVerb
{
    filledMove = static_cast<int>(PathVerb::move),
    strokedMove =
        STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::move),
    roundJoinStrokedMove = STROKE_OR_FEATHER_STYLE_FLAG |
                           ROUND_JOIN_STYLE_FLAG |
                           static_cast<int>(PathVerb::move),

    filledLine = static_cast<int>(PathVerb::line),
    strokedLine =
        STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::line),
    roundJoinStrokedLine = STROKE_OR_FEATHER_STYLE_FLAG |
                           ROUND_JOIN_STYLE_FLAG |
                           static_cast<int>(PathVerb::line),

    filledQuad = static_cast<int>(PathVerb::quad),
    strokedQuad =
        STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::quad),
    roundJoinStrokedQuad = STROKE_OR_FEATHER_STYLE_FLAG |
                           ROUND_JOIN_STYLE_FLAG |
                           static_cast<int>(PathVerb::quad),

    filledCubic = static_cast<int>(PathVerb::cubic),
    strokedCubic =
        STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::cubic),
    roundJoinStrokedCubic = STROKE_OR_FEATHER_STYLE_FLAG |
                            ROUND_JOIN_STYLE_FLAG |
                            static_cast<int>(PathVerb::cubic),

    filledClose = static_cast<int>(PathVerb::close),
    strokedClose =
        STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::close),
    roundJoinStrokedClose = STROKE_OR_FEATHER_STYLE_FLAG |
                            ROUND_JOIN_STYLE_FLAG |
                            static_cast<int>(PathVerb::close),
};
RIVE_ALWAYS_INLINE constexpr StyledVerb styled_verb(PathVerb verb,
                                                    int styleFlags)
{
    return static_cast<StyledVerb>(styleFlags | static_cast<int>(verb));
}

// When chopping strokes, switching on a "chop_key" reduces "if (areCusps)"
// branching and makes the code cleaner.
RIVE_ALWAYS_INLINE constexpr uint8_t chop_key(bool areCusps, uint8_t numChops)
{
    return (numChops << 1) | static_cast<uint8_t>(areCusps);
}
RIVE_ALWAYS_INLINE constexpr uint8_t cusp_chop_key(uint8_t n)
{
    return chop_key(true, n);
}
RIVE_ALWAYS_INLINE constexpr uint8_t simple_chop_key(uint8_t n)
{
    return chop_key(false, n);
}

// Produces a cubic equivalent to the given line, for which Wang's formula also
// returns 1.
RIVE_ALWAYS_INLINE std::array<Vec2D, 4> convert_line_to_cubic(
    const Vec2D line[2])
{
    float4 endPts = simd::load4f(line);
    float4 controlPts = simd::mix(endPts, endPts.zwxy, float4(1 / 3.f));
    std::array<Vec2D, 4> cubic;
    cubic[0] = line[0];
    simd::store(&cubic[1], controlPts);
    cubic[3] = line[1];
    return cubic;
}
RIVE_ALWAYS_INLINE std::array<Vec2D, 4> convert_line_to_cubic(Vec2D p0,
                                                              Vec2D p1)
{
    Vec2D line[2] = {p0, p1};
    return convert_line_to_cubic(line);
}

// Chops a cubic into 2 * n + 1 segments, surrounding each cusp. The resulting
// cubics will be visually equivalent to the original when stroked, but the cusp
// won't have artifacts when rendered using the parametric/polar sorting
// algorithm.
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
    // Generate chop points straddling each cusp with padding. This creates
    // buffer space around the cusp that protects against fp32 precision issues.
    for (int i = 0; i < n; ++i)
    {
        // If the cusps are extremely close together, don't allow the straddle
        // points to cross.
        float minT = i == 0 ? 0.f : (cuspT[i - 1] + cuspT[i]) * .5f;
        float maxT = i + 1 == n ? 1.f : (cuspT[i + 1] + cuspT[i]) * .5f;
        t[i * 2 + 0] = fmaxf(cuspT[i] - math::EPSILON, minT);
        t[i * 2 + 1] = fminf(cuspT[i] + math::EPSILON, maxT);
    }
    math::chop_cubic_at(p, dst, t, n * 2);
    for (int i = 0; i < n; ++i)
    {
        // Find the three chops at this cusp.
        Vec2D* chops = dst + i * 6;
        // Correct the chops to fall on the actual cusp point.
        Vec2D cusp = math::eval_cubic_at(p, cuspT[i]);
        chops[3] = chops[6] = cusp;
        // The only purpose of the middle cubic is to capture the cusp's
        // 180-degree rotation. Implement it as a sub-pixel 180-degree pivot.
        Vec2D pivot = (chops[2] + chops[7]) * .5f;
        pivot = (cusp - pivot).normalized() /
                    (matrixMaxScale * kPolarPrecision * 2) +
                cusp;
        chops[4] = chops[5] = pivot;
    }
}

// Finds the starting tangent in a contour composed of the points [pts, end). If
// all points are equal, generates a tangent pointing horizontally to the right.
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

// Finds the ending tangent in a contour composed of the points [pts, end). If
// all points are equal, generates a tangent pointing horizontally to the left.
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
    // Find the first point in the contour not equal to *joinPoint and return
    // the difference. RawPath should have discarded empty verbs, so this should
    // be a fast operation.
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
    // Quick early out for inlining and branch prediction: The next point in the
    // contour is almost always the point that determines the join tangent.
    const Vec2D* nextPoint = joinPoint + 1;
    nextPoint = nextPoint != end ? nextPoint : p0;
    Vec2D tangent = *nextPoint - *joinPoint;
    return tangent != Vec2D{0, 0}
               ? tangent
               : find_join_tangent_full_impl(joinPoint, end, closed, p0);
}

// Should an empty stroke emit round caps, square caps, or none?
//
// Just pick the cap type that makes the most sense for a contour that animates
// from non-empty to empty:
//
//   * A non-closed contour with round caps and a CLOSED contour with round
//   JOINS both converge to a
//     circle when animated to empty.
//         => round caps on the empty contour.
//
//   * A non-closed contour with square caps converges to a square (albeit with
//   potential rotation
//     that is lost when the contour becomes empty).
//         => square caps on the empty contour.
//
//   * A closed contour with miter JOINS converges to... some sort of polygon
//   with pointy corners.
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
            return ROUND_JOIN_CONTOUR_FLAG;
        case StrokeJoin::bevel:
            return BEVEL_JOIN_CONTOUR_FLAG;
    }
    RIVE_UNREACHABLE();
}

inline float find_feather_radius(float paintFeather)
{
    // Blur magnitudes in design tools are customarily the width of two standard
    // deviations, or, the length of the range -1stddev .. +1stddev.
    return paintFeather * (FEATHER_TEXTURE_STDDEVS / 2);
}

inline float find_atlas_feather_scale_factor(float featherRadius,
                                             float matrixMaxScale)
{
    // In practice, and with "gaussian -> linear -> gaussian" filtering, we
    // never need to render a blur with a radius larger than 16 pixels. After
    // this point, we can just scale it up to whatever resolution it needs to be
    // displayed at.
    return 16.f / fmaxf(featherRadius * matrixMaxScale, 16);
}

static uint32_t feather_join_segment_count(float polarSegmentsPerRadian)
{
    uint32_t n =
        static_cast<uint32_t>(ceilf(polarSegmentsPerRadian * math::PI)) +
        FEATHER_JOIN_HELPER_SEGMENT_COUNT;
    n = std::max(n, FEATHER_JOIN_MIN_SEGMENT_COUNT);
    // FEATHER_POLAR_SEGMENT_MIN_ANGLE should limit n long before we reach
    // kMaxPolarSegments.
    assert(n < gpu::kMaxPolarSegments);
    return n;
}
} // namespace

Draw::Draw(IAABB pixelBounds,
           const Mat2D& matrix,
           BlendMode blendMode,
           rcp<const Texture> imageTexture,
           ImageSampler imageSampler,
           Type type) :
    m_imageTextureRef(imageTexture.release()),
    m_imageSampler(imageSampler),
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

void Draw::setClipID(uint32_t clipID)
{
    m_clipID = clipID;

    // For clipUpdates, m_clipID refers to the ID we are writing to the stencil
    // buffer (NOT the ID we are clipping against). It therefore doesn't affect
    // the activeClip flag in that case.
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

void Draw::releaseRefs() { safe_unref(m_imageTextureRef); }

PathDraw::CoverageType PathDraw::SelectCoverageType(
    const RiveRenderPaint* paint,
    float matrixMaxScale,
    const gpu::PlatformFeatures& platformFeatures,
    gpu::InterlockMode interlockMode)
{
    if (paint->getFeather() != 0)
    {
        if (platformFeatures.alwaysFeatherToAtlas ||
            interlockMode == gpu::InterlockMode::msaa ||
            // Always switch to the atlas once we can render quarter-resultion.
            find_atlas_feather_scale_factor(
                find_feather_radius(paint->getFeather()),
                matrixMaxScale) <= .5f)
        {
            return CoverageType::atlas;
        }
    }
    if (interlockMode == gpu::InterlockMode::msaa)
    {
        return CoverageType::msaa;
    }
    if (interlockMode == gpu::InterlockMode::clockwiseAtomic)
    {
        return CoverageType::clockwiseAtomic;
    }
    return CoverageType::pixelLocalStorage;
}

DrawUniquePtr PathDraw::Make(RenderContext* context,
                             const Mat2D& matrix,
                             rcp<const RiveRenderPath> path,
                             FillRule fillRule,
                             const RiveRenderPaint* paint,
                             RawPath* scratchPath)
{
    assert(path != nullptr);
    assert(paint != nullptr);

    CoverageType coverageType =
        SelectCoverageType(paint,
                           matrix.findMaxScale(),
                           context->platformFeatures(),
                           context->frameInterlockMode());

    // Compute the screen-space bounding box.
    AABB mappedBounds;
    if (context->frameInterlockMode() == gpu::InterlockMode::rasterOrdering &&
        coverageType != CoverageType::atlas)
    {
        // In rasterOrdering mode we can use a looser bounding box since we
        // don't do reordering.
        mappedBounds = matrix.mapBoundingBox(path->getBounds());
    }
    else
    {
        // Otherwise find a tight bounding box in order to maximize reordering.
        mappedBounds =
            matrix.mapBoundingBox(path->getRawPath().points().data(),
                                  path->getRawPath().points().count());
    }
    assert(mappedBounds.width() >= 0);
    assert(mappedBounds.height() >= 0);
    if (paint->getIsStroked() || paint->getFeather() != 0)
    {
        // Outset the path's bounding box to account for stroking & feathering.
        float outset = 0;
        if (paint->getIsStroked())
        {
            outset = paint->getThickness() * .5f;
            if (paint->getJoin() == StrokeJoin::miter)
            {
                // Miter joins may be longer than the stroke radius.
                outset *= RIVE_MITER_LIMIT;
            }
            else if (paint->getCap() == StrokeCap::square)
            {
                // The diagonal of a square cap is longer than the stroke
                // radius.
                outset *= math::SQRT2;
            }
        }
        if (paint->getFeather() != 0)
        {
            outset += find_feather_radius(paint->getFeather());
        }
        AABB strokePixelOutset = matrix.mapBoundingBox({0, 0, outset, outset});
        // Add an extra pixel to the stroke outset radius to account for:
        //   * Butt caps and bevel joins bleed out 1/2 AA width.
        //   * With Manhattan sytle AA, an AA width can be as large as sqrt(2).
        //   * The diagonal of that sqrt(2)/2 bleed is 1px in length.
        mappedBounds = mappedBounds.outset(strokePixelOutset.width() + 1,
                                           strokePixelOutset.height() + 1);
    }

    IAABB pixelBounds = mappedBounds.roundOut();
    bool doTriangulation = false;
    const AABB& localBounds = path->getBounds();
    if (context->isOutsideCurrentFrame(pixelBounds))
    {
        return DrawUniquePtr();
    }
    if (!paint->getIsStroked() && paint->getFeather() == 0)
    {
        // Use interior triangulation to draw filled paths if they're large
        // enough to benefit from it.
        //
        // FIXME! Implement interior triangulation for feathers.
        //
        // FIXME! Implement interior triangulation in msaa mode.
        if (context->frameInterlockMode() != gpu::InterlockMode::msaa &&
            path->getRawPath().verbs().count() < 1000 &&
            gpu::find_transformed_area(localBounds, matrix) > 512.f * 512.f)
        {
            doTriangulation = true;
        }
    }

    auto draw = context->make<PathDraw>(pixelBounds,
                                        matrix,
                                        std::move(path),
                                        fillRule,
                                        paint,
                                        coverageType,
                                        context->frameDescriptor());
    if (doTriangulation)
    {
        draw->initForInteriorTriangulation(
            context,
            scratchPath,
            localBounds.width() > localBounds.height()
                ? PathDraw::TriangulatorAxis::horizontal
                : PathDraw::TriangulatorAxis::vertical);
    }
    else
    {
        draw->initForMidpointFan(context, paint);
    }

    return DrawUniquePtr(draw);
}

PathDraw::PathDraw(IAABB pixelBounds,
                   const Mat2D& matrix,
                   rcp<const RiveRenderPath> path,
                   FillRule initialFillRule,
                   const RiveRenderPaint* paint,
                   CoverageType coverageType,
                   const RenderContext::FrameDescriptor& frameDesc) :
    Draw(pixelBounds,
         matrix,
         paint->getBlendMode(),
         ref_rcp(paint->getImageTexture()),
         paint->getImageSampler(),
         Type::path),
    m_pathRef(path.release()),
    m_pathFillRule(frameDesc.clockwiseFillOverride ? FillRule::clockwise
                                                   : initialFillRule),
    m_gradientRef(safe_ref(paint->getGradient())),
    m_paintType(paint->getType()),
    m_coverageType(coverageType)
{
    assert(m_pathRef != nullptr);
    assert(!m_pathRef->getRawPath().empty());
    assert(paint != nullptr);

    if (paint->getIsOpaque())
    {
        m_drawContents |= gpu::DrawContents::opaquePaint;
    }

    if (paint->getFeather() != 0)
    {
        m_featherRadius = find_feather_radius(paint->getFeather());
        assert(!std::isnan(m_featherRadius)); // These should get culled in
                                              // RiveRenderer::drawPath().
        assert(m_featherRadius > 0);
    }

    if (paint->getIsStroked())
    {
        m_strokeRadius = paint->getThickness() * .5f;
        // Ensure stroke radius is nonzero. (In PLS, zero radius means the path
        // is filled.)
        m_strokeRadius =
            fmaxf(m_strokeRadius, std::numeric_limits<float>::min());
        assert(!std::isnan(m_strokeRadius)); // These should get culled in
                                             // RiveRenderer::drawPath().
        assert(m_strokeRadius > 0);
    }

    // For atlased paths, m_drawContents refers to the rectangle being drawn
    // into the main render target, not the step that generates the atlas mask.
    if (m_coverageType != CoverageType::atlas)
    {
        if (isStroke())
        {
            m_drawContents |= gpu::DrawContents::stroke;
        }
        else
        {
            if (m_featherRadius)
            {
                m_drawContents |= gpu::DrawContents::featheredFill;
            }
            if (initialFillRule == FillRule::clockwise ||
                frameDesc.clockwiseFillOverride)
            {
                m_drawContents |= gpu::DrawContents::clockwiseFill;
            }
            else if (initialFillRule == FillRule::nonZero)
            {
                m_drawContents |= gpu::DrawContents::nonZeroFill;
            }
            else if (initialFillRule == FillRule::evenOdd)
            {
                m_drawContents |= gpu::DrawContents::evenOddFill;
            }
        }
    }

    if (paint->getType() == gpu::PaintType::clipUpdate)
    {
        m_drawContents |= gpu::DrawContents::clipUpdate;
        if (paint->getSimpleValue().outerClipID != 0)
        {
            m_drawContents |= gpu::DrawContents::activeClip;
        }
    }

    if (isStroke())
    {
        // Stroke triangles are always forward.
        m_contourDirections = gpu::ContourDirections::forward;
    }
    else if (initialFillRule == FillRule::clockwise)
    {
        // Clockwise paths need to be reversed when the matrix is left-handed,
        // so that the intended forward triangles remain clockwise.
        float det = matrix.xx() * matrix.yy() - matrix.yx() * matrix.xy();
        if (det < 0)
        {
            m_contourDirections =
                m_coverageType == CoverageType::msaa
                    ? gpu::ContourDirections::reverse
                    : gpu::ContourDirections::forwardThenReverse;
            m_contourFlags |= NEGATE_PATH_FILL_COVERAGE_FLAG; // ignored by msaa
        }
        else
        {
            m_contourDirections =
                m_coverageType == CoverageType::msaa
                    ? gpu::ContourDirections::forward
                    : gpu::ContourDirections::reverseThenForward;
        }
    }
    else if (m_coverageType != CoverageType::msaa)
    {
        // atomic and rasterOrdering fills need reverse AND forward triangles.
        if (frameDesc.clockwiseFillOverride &&
            !m_pathRef->isClockwiseDominant(matrix))
        {
            // For clockwiseFill, this is also our opportunity to logically
            // reverse the winding of the path, if it is predominantly
            // counterclockwise.
            m_contourDirections = gpu::ContourDirections::forwardThenReverse;
            m_contourFlags |= NEGATE_PATH_FILL_COVERAGE_FLAG;
        }
        else
        {
            m_contourDirections = gpu::ContourDirections::reverseThenForward;
        }
    }
    else
    {
        if (initialFillRule == FillRule::nonZero ||
            frameDesc.clockwiseFillOverride)
        {
            // Emit "nonZero" msaa fills in a direction such that the dominant
            // triangle winding area is always clockwise. This maximizes pixel
            // throughput since we will draw counterclockwise triangles twice
            // and clockwise only once.
            m_contourDirections = m_pathRef->isClockwiseDominant(matrix)
                                      ? gpu::ContourDirections::forward
                                      : gpu::ContourDirections::reverse;
        }
        else
        {
            // "evenOdd" msaa fills just get drawn twice, so any direction is
            // fine.
            m_contourDirections = gpu::ContourDirections::forward;
        }
    }

    m_simplePaintValue = paint->getSimpleValue();

    if (m_coverageType == CoverageType::atlas)
    {
        // Reserve two triangles for our on-screen rectangle that reads coverage
        // from the atlas.
        m_resourceCounts.maxTriangleVertexCount = 6;
    }

    RIVE_DEBUG_CODE(m_pathRef->lockRawPathMutations();)
    RIVE_DEBUG_CODE(m_rawPathMutationID = m_pathRef->getRawPathMutationID();)
    assert(isStroke() == (strokeRadius() > 0));
    assert(isFeatheredFill() == (!isStroke() && featherRadius() > 0));
    assert(!isFeatheredFill() || featherRadius() > 0);
}

void PathDraw::releaseRefs()
{
    Draw::releaseRefs();
    RIVE_DEBUG_CODE(m_pathRef->unlockRawPathMutations();)
    m_pathRef->unref();
    safe_unref(m_gradientRef);
}

void PathDraw::initForMidpointFan(RenderContext* context,
                                  const RiveRenderPaint* paint)
{
    // Only call init() once.
    assert((m_resourceCounts.midpointFanTessVertexCount |
            m_resourceCounts.outerCubicTessVertexCount) == 0);

    if (isStrokeOrFeather())
    {
        m_strokeMatrixMaxScale = m_matrix.findMaxScale();

        float r_ = 0;
        if (m_featherRadius != 0)
        {
            r_ = m_featherRadius * m_strokeMatrixMaxScale;

            // Inverse of 1/calc_polar_segments_per_radian at
            // FEATHER_MIN_POLAR_SEGMENT_ANGLE.
            //
            // i.e., 1 / calc_polar_segments_per_radian<kPolarPrecision>(
            //           FEATHER_MIN_POLAR_SEGMENT_ANGLE) ==
            //       FEATHER_MAX_SCREEN_SPACE_RADIUS
            //
            constexpr static float FEATHER_MAX_SCREEN_SPACE_RADIUS =
                1 / (kPolarPrecision *
                     (1 - COS_FEATHER_POLAR_SEGMENT_MIN_ANGLE_OVER_2));
            assert(math::nearly_equal(
                1 / math::calc_polar_segments_per_radian<kPolarPrecision>(
                        FEATHER_MAX_SCREEN_SPACE_RADIUS),
                gpu::FEATHER_POLAR_SEGMENT_MIN_ANGLE));

            // r_ is used to calculate how many polar segments are needed. Limit
            // r_ for large feathers. (See FEATHER_POLAR_SEGMENT_MIN_ANGLE.)
            r_ = std::min(r_, FEATHER_MAX_SCREEN_SPACE_RADIUS);
        }
        if (isStroke())
        {
            r_ += m_strokeRadius * m_strokeMatrixMaxScale;
        }
        m_polarSegmentsPerRadian =
            math::calc_polar_segments_per_radian<kPolarPrecision>(r_);

        m_strokeJoin = paint->getJoin();
        m_strokeCap = paint->getCap();
    }

    // Count up how much temporary storage this function will need to reserve in
    // CPU buffers.
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
    // Every path has at least 1 (non-cubic) move.
    size_t pathMaxLinesOrCurvesBeforeChops = rawPath.verbs().size() - 1;
    // Stroked cubics can be chopped into a maximum of 5 segments.
    size_t pathMaxLinesOrCurvesAfterChops =
        isStrokeOrFeather() ? pathMaxLinesOrCurvesBeforeChops * 5
                            : pathMaxLinesOrCurvesBeforeChops;
    maxCurves += pathMaxLinesOrCurvesAfterChops;
    if (isStrokeOrFeather())
    {
        maxStrokedCurvesBeforeChops += pathMaxLinesOrCurvesBeforeChops;
        maxRotations += pathMaxLinesOrCurvesAfterChops;
        if (m_strokeJoin == StrokeJoin::round)
        {
            // If the stroke has round joins, we also record the rotations
            // between (pre-chopped) joins in order to calculate how many
            // vertices are in each round join.
            maxRotations += pathMaxLinesOrCurvesBeforeChops;
        }
    }

    // Each stroked curve will record the number of chops it requires (either 0,
    // 1, or 2).
    size_t maxChops = maxStrokedCurvesBeforeChops;
    // We only chop into this queue if a cubic has one chop. More chops in a
    // single cubic are rare and require a lot of memory, so if a cubic needs
    // more chops we just re-chop the second time around. The maximum size this
    // queue would need is therefore enough to chop each cubic once, or 5
    // internal points per chop.
    size_t maxChopVertices = maxStrokedCurvesBeforeChops * 5;
    // +3 for each contour because we align each contour's curves and rotations
    // on multiples of 4.
    size_t maxPaddedRotations =
        isStrokeOrFeather() ? maxRotations + contourCount * 3 : 0;
    size_t maxPaddedCurves = maxCurves + contourCount * 3;

    // Reserve intermediate space for the polar segment counts of each curve and
    // round join.
    if (isStrokeOrFeather())
    {
        m_numChops.reset(context->numChopsAllocator(), maxChops);
        m_chopVertices.reset(context->chopVerticesAllocator(), maxChopVertices);
        m_tangentPairs =
            context->tangentPairsAllocator().alloc(maxPaddedRotations);
        m_polarSegmentCounts =
            context->polarSegmentCountsAllocator().alloc(maxPaddedRotations);
    }
    m_parametricSegmentCounts =
        context->parametricSegmentCountsAllocator().alloc(maxPaddedCurves);

    float parametricPrecision = gpu::kParametricPrecision;
    if (m_featherRadius > 1)
    {
        // Once the blur radius is above ~50 pixels, we don't have to tessellate
        // within 1/4px of the edge anymore.
        // At this point, tessellate within strokeRadius/200 pixels of the edge.
        // (parametricPrecision == 1/tolerance.)
        parametricPrecision =
            std::min(parametricPrecision * 100.f /
                         (m_featherRadius * m_strokeMatrixMaxScale),
                     parametricPrecision);
    }

    size_t lineCount = 0;
    size_t unpaddedCurveCount = 0;
    size_t unpaddedRotationCount = 0;
    size_t emptyStrokeCountForCaps = 0;

    // Iteration pass 1: Collect information on contour and curves counts for
    // every path in the batch, and begin counting tessellated vertices.
    size_t contourIdx = 0;
    size_t curveIdx = 0;
    // We measure rotations on both curves and round joins.
    size_t rotationIdx = 0;
    bool roundJoinStroked = isStroke() && m_strokeJoin == StrokeJoin::round;
    wangs_formula::VectorXform vectorXform(m_matrix);
    RawPath::Iter startOfContour = rawPath.begin();
    RawPath::Iter end = rawPath.end();
    // Original number of lines and curves, before chopping.
    int preChopVerbCount = 0;
    Vec2D endpointsSum{};
    bool closed = !isStroke();
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
            // Bit-cast to uint64_t because we don't want the special equality
            // rules for NaN. If we're empty or otherwise return back to p0, we
            // want to detect this, regardless of whether there are NaN values.
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
                             // The first point in the contour hasn't gotten
                             // counted yet.
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
            isStroke() ? Vec2D() : endpointsSum * (1.f / preChopVerbCount),
            closed,
            strokeJoinCount,
            0,                 // strokeCapSegmentCount
            0,                 // paddingVertexCount
            RIVE_DEBUG_CODE(0) // tessVertexCount
        };
        unpaddedCurveCount += curveIdx - contourFirstCurveIdx;
        contourFirstCurveIdx = curveIdx =
            math::round_up_to_multiple_of<4>(curveIdx);
        unpaddedRotationCount += rotationIdx - contourFirstRotationIdx;
        contourFirstRotationIdx = rotationIdx =
            math::round_up_to_multiple_of<4>(rotationIdx);
    };
    const int styleFlags = style_flags(isStrokeOrFeather(), roundJoinStroked);
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
                closed = !isStroke();
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
                math::find_cubic_tangents(p, unchoppedTangents);
                if (preChopVerbCount == 0)
                {
                    firstTangent = unchoppedTangents[0];
                }
                else
                {
                    assert(rotationIdx < maxPaddedRotations);
                    m_tangentPairs[rotationIdx++] = {lastTangent,
                                                     unchoppedTangents[0]};
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
                // Chop strokes into sections that do not inflect (i.e, are
                // convex), and do not rotate more than 180 degrees. This is
                // required by the GPU parametric/polar sorter.
                float t[2];
                bool areCusps = false;
                uint8_t numChops =
                    isStroke()
                        ? math::find_cubic_convex_180_chops(p, t, &areCusps)
                        : 0; // Feathers already got chopped.
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
                        math::chop_cubic_at(p, localChopBuffer, t[0], t[1]);
                        p = localChopBuffer;
                        break;
                    case simple_chop_key(1): // 1 non-cusp chop
                    {
                        math::chop_cubic_at(p, localChopBuffer, t[0]);
                        p = localChopBuffer;
                        memcpy(m_chopVertices.push_back_n(5),
                               p + 1,
                               sizeof(Vec2D) * 5);
                        break;
                    }
                }
                // Calculate segment counts for each chopped section
                // independently.
                for (const Vec2D* end = p + numChops * 3 + 3; p != end;
                     p += 3, ++curveIdx, ++rotationIdx)
                {
                    float n4 = wangs_formula::cubic_pow4(p,
                                                         parametricPrecision,
                                                         vectorXform);
                    // Record n^4 for now. This will get resolved later.
                    assert(curveIdx < maxPaddedCurves);
                    RIVE_INLINE_MEMCPY(m_parametricSegmentCounts + curveIdx,
                                       &n4,
                                       sizeof(uint32_t));
                    assert(rotationIdx < maxPaddedRotations);
                    if (isStroke())
                    {
                        math::find_cubic_tangents(
                            p,
                            m_tangentPairs[rotationIdx].data());
                    }
                    else
                    {
                        // FIXME: Feathered fills don't have polar segments for
                        // now, but we're leaving space for them in
                        // m_tangentPairs because we will convert them to use a
                        // similar concept of "polar joins" once we move them
                        // onto the GPU.
                        m_tangentPairs[rotationIdx] = {Vec2D{0, 1},
                                                       Vec2D{0, 1}};
                    }
                }
                break;
            }
            case StyledVerb::filledCubic:
            {
                const Vec2D* p = iter.cubicPts();
                ++preChopVerbCount;
                endpointsSum += p[3];
                float n4 = wangs_formula::cubic_pow4(p,
                                                     parametricPrecision,
                                                     vectorXform);
                // Record n^4 for now. This will get resolved later.
                assert(curveIdx < maxPaddedCurves);
                RIVE_INLINE_MEMCPY(m_parametricSegmentCounts + curveIdx++,
                                   &n4,
                                   sizeof(uint32_t));
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
    // Because we write parametric segment counts in batches of 4.
    assert(curveIdx % 4 == 0);
    // Because we write polar segment counts in batches of 4.
    assert(rotationIdx % 4 == 0);
    assert(isStrokeOrFeather() || maxPaddedRotations == 0);
    assert(isStrokeOrFeather() || rotationIdx == 0);

    // Return any data we conservatively allocated but did not use.
    if (isStrokeOrFeather())
    {
        m_numChops.shrinkToFit(context->numChopsAllocator(), maxChops);
        m_chopVertices.shrinkToFit(context->chopVerticesAllocator(),
                                   maxChopVertices);
        context->tangentPairsAllocator().rewindLastAllocation(
            maxPaddedRotations - rotationIdx);
        context->polarSegmentCountsAllocator().rewindLastAllocation(
            maxPaddedRotations - rotationIdx);
    }
    context->parametricSegmentCountsAllocator().rewindLastAllocation(
        maxPaddedCurves - curveIdx);

    // Iteration pass 2: Finish calculating the numbers of tessellation segments
    // in each contour, using SIMD.
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
            // Curves recorded their segment counts raised to the 4th power. Now
            // find their roots and convert to integers in batches of 4.
            assert(j + 4 <= curveIdx);
            float4 n = simd::load4f(m_parametricSegmentCounts + j);
            n = simd::ceil(simd::sqrt(simd::sqrt(n)));
            n = simd::clamp(n, float4(1), float4(kMaxParametricSegments));
            uint4 n_ = simd::cast<uint32_t>(n);
            assert(j + 4 <= curveIdx);
            simd::store(m_parametricSegmentCounts + j, n_);
            mergedTessVertexSums4 += n_;
        }
        // We counted in batches of 4. Undo the values we counted from beyond
        // the end of the path.
        while (j-- > contour->endCurveIdx)
        {
            contourVertexCount -= m_parametricSegmentCounts[j];
        }

        if (isStrokeOrFeather())
        {
            // Finish calculating and counting polar segments for each stroked
            // curve and round join.
            for (j = contour->firstRotationIdx; j < contour->endRotationIdx;
                 j += 4)
            {
                // Measure the rotations of curves in batches of 4.
                assert(j + 4 <= rotationIdx);

                float4 tx0, ty0, tx1, ty1;
                std::tie(tx0, ty0, tx1, ty1) =
                    simd::load4x4f(&m_tangentPairs[j][0].x);

                float4 numer = tx0 * tx1 + ty0 * ty1;
                float4 denom_pow2 =
                    (tx0 * tx0 + ty0 * ty0) * (tx1 * tx1 + ty1 * ty1);
                float4 cosTheta = numer / simd::sqrt(denom_pow2);
                cosTheta = simd::clamp(cosTheta, float4(-1), float4(1));
                float4 theta = simd::fast_acos(cosTheta);
                // Find polar segment counts from the rotation angles.
                float4 n = simd::ceil(theta * m_polarSegmentsPerRadian);
                n = simd::clamp(n, float4(1), float4(kMaxPolarSegments));
                uint4 n_ = simd::cast<uint32_t>(n);
                assert(j + 4 <= rotationIdx);
                simd::store(m_polarSegmentCounts + j, n_);
                // Polar and parametric segments share the first and final
                // vertices. Therefore:
                //
                //   parametricVertexCount = parametricSegmentCount + 1
                //
                //   polarVertexCount = polarVertexCount + 1
                //
                //   mergedVertexCount
                //     = parametricVertexCount + polarVertexCount - 2
                //     = parametricSegmentCount + 1 + polarSegmentCount + 1 - 2
                //     = parametricSegmentCount + polarSegmentCount
                //
                mergedTessVertexSums4 += n_;
            }

            // We counted in batches of 4. Undo the values we counted from
            // beyond the end of the path.
            while (j-- > contour->endRotationIdx)
            {
                contourVertexCount -= m_polarSegmentCounts[j];
            }

            // Count joins.
            if (!isStroke())
            {
                assert(isFeatheredFill());
                uint32_t numSegmentsInFeatherJoin =
                    feather_join_segment_count(m_polarSegmentsPerRadian);
                contourVertexCount +=
                    contour->strokeJoinCount * (numSegmentsInFeatherJoin - 1);
            }
            else if (m_strokeJoin == StrokeJoin::round)
            {
                // Round joins share their beginning and ending vertices with
                // the curve on either side. Therefore, the number of vertices
                // we need to allocate for a round join is "joinSegmentCount -
                // 1". Do all the -1's here.
                contourVertexCount -= contour->strokeJoinCount;
            }
            else
            {
                // The shader needs 3 segments for each miter and bevel join
                // (which translates to two interior vertices, since joins share
                // their beginning and ending vertices with the curve on either
                // side).
                contourVertexCount += contour->strokeJoinCount *
                                      (NUM_SEGMENTS_IN_MITER_OR_BEVEL_JOIN - 1);
            }

            // Count stroke caps, if any.
            bool empty = contour->endLineIdx == contourFirstLineIdx &&
                         contour->endCurveIdx == contour->firstCurveIdx;
            StrokeCap cap;
            bool needsCaps = false;
            if (!empty)
            {
                cap = m_strokeCap;
                needsCaps = !contour->closed;
            }
            else if (isStroke())
            {
                cap = empty_stroke_cap(contour->closed,
                                       m_strokeJoin,
                                       m_strokeCap);
                needsCaps = cap != StrokeCap::butt; // Ignore butt caps when the
                                                    // contour is empty.
            }
            if (needsCaps)
            {
                // We emulate stroke caps as 180-degree joins.
                if (cap == StrokeCap::round)
                {
                    // Round caps rotate 180 degrees.
                    float strokeCapSegmentCount =
                        ceilf(m_polarSegmentsPerRadian * math::PI);
                    // +2 because round caps emulated as joins need to emit
                    // vertices at T=0 and T=1, unlike normal round joins.
                    strokeCapSegmentCount += 2;
                    // Make sure not to exceed kMaxPolarSegments.
                    strokeCapSegmentCount =
                        fminf(strokeCapSegmentCount, kMaxPolarSegments);
                    contour->strokeCapSegmentCount =
                        static_cast<uint32_t>(strokeCapSegmentCount);
                }
                else
                {
                    contour->strokeCapSegmentCount =
                        NUM_SEGMENTS_IN_MITER_OR_BEVEL_JOIN;
                }
                // PLS expects all patches to have >0 tessellation vertices, so
                // for the case of an empty patch with a stroke cap,
                // contour->strokeCapSegmentCount can't be zero. Also,
                // pushContourToRenderContext() uses "strokeCapSegmentCount !=
                // 0" to tell if it needs stroke caps.
                assert(contour->strokeCapSegmentCount >= 2);
                // As long as a contour isn't empty, we can tack the end cap
                // onto the join section of the final curve in the stroke.
                // Otherwise, we need to introduce 0-tessellation-segment curves
                // with non-empty joins to carry the caps.
                emptyStrokeCountForCaps += empty ? 2 : 1;
                contourVertexCount += (contour->strokeCapSegmentCount - 1) * 2;
            }
        }
        else
        {
            // Fills don't have polar segments:
            //
            //   mergedVertexCount = parametricVertexCount =
            //   parametricSegmentCount + 1
            //
            // Just collect the +1 for each non-stroked curve.
            size_t contourCurveCount =
                contour->endCurveIdx - contour->firstCurveIdx;
            contourVertexCount += contourCurveCount;
        }
        contourVertexCount += simd::reduce_add(mergedTessVertexSums4);

        // Add padding vertices until the number of tessellation vertices in the
        // contour is an exact multiple of kMidpointFanPatchSegmentSpan. This
        // ensures that patch boundaries align with contour boundaries.
        contour->paddingVertexCount =
            math::padding_to_align_up<kMidpointFanPatchSegmentSpan>(
                contourVertexCount);
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
        // maxTessellatedSegmentCount does not get doubled when we emit both
        // forward and mirrored contours because the forward and mirrored pair
        // both get packed into a single gpu::TessVertexSpan.
        m_resourceCounts.maxTessellatedSegmentCount =
            lineCount + unpaddedCurveCount + emptyStrokeCountForCaps;
        m_resourceCounts.midpointFanTessVertexCount =
            gpu::ContourDirectionsAreDoubleSided(m_contourDirections)
                ? tessVertexCount * 2
                : tessVertexCount;
    }
}

void PathDraw::initForInteriorTriangulation(RenderContext* context,
                                            RawPath* scratchPath,
                                            TriangulatorAxis triangulatorAxis)
{
    assert(simd::all(m_resourceCounts.toVec() == 0)); // Only call init() once.
    assert(!isStrokeOrFeather());
    assert(m_strokeRadius == 0);

    // Every path has at least 1 (non-cubic) move.
    size_t originalNumChopsSize = m_pathRef->getRawPath().verbs().size() - 1;
    m_numChops.reset(context->numChopsAllocator(), originalNumChopsSize);
    iterateInteriorTriangulation(
        InteriorTriangulationOp::countDataAndTriangulate,
        &context->perFrameAllocator(),
        scratchPath,
        triangulatorAxis,
        nullptr);
    m_numChops.shrinkToFit(context->numChopsAllocator(), originalNumChopsSize);
}

bool PathDraw::allocateResources(RenderContext::LogicalFlush* flush)
{
    const RenderContext::FrameDescriptor& frameDesc = flush->frameDescriptor();

    // Allocate a gradient if needed. Do this first since it's more expensive to
    // fail after setting up an atlas draw than a gradient draw.
    if (m_gradientRef != nullptr &&
        !flush->allocateGradient(m_gradientRef,
                                 &m_simplePaintValue.colorRampLocation))
    {
        return false;
    }

    // Allocate a coverage buffer range or atlas region if needed.
    if (m_coverageType == CoverageType::atlas ||
        m_coverageType == CoverageType::clockwiseAtomic)
    {
        constexpr static int PADDING = 2;

        // We don't need any coverage space for areas outside the viewport.
        // TODO: Account for active clips as well once we have the info.
        IAABB renderTargetBounds = {
            0,
            0,
            static_cast<int32_t>(frameDesc.renderTargetWidth),
            static_cast<int32_t>(frameDesc.renderTargetHeight),
        };
        IAABB visibleBounds = renderTargetBounds.intersect(m_pixelBounds);

        if (m_coverageType == CoverageType::atlas)
        {
            const float scaleFactor =
                find_atlas_feather_scale_factor(m_featherRadius,
                                                m_strokeMatrixMaxScale);
            auto w = static_cast<uint16_t>(
                ceilf(visibleBounds.width() * scaleFactor));
            auto h = static_cast<uint16_t>(
                ceilf(visibleBounds.height() * scaleFactor));
            uint16_t x, y;
            if (!flush->allocateAtlasDraw(this,
                                          w,
                                          h,
                                          PADDING,
                                          &x,
                                          &y,
                                          &m_atlasScissor))
            {
                return false; // There wasn't room for our path in the atlas.
            }
            m_atlasTransform.scaleFactor = scaleFactor;
            m_atlasTransform.translateX = x - visibleBounds.left * scaleFactor;
            m_atlasTransform.translateY = y - visibleBounds.top * scaleFactor;
            m_atlasScissorEnabled = visibleBounds != m_pixelBounds;
        }
        else
        {
            // Round up width and height to multiples of 32 for tiling.
            uint32_t coverageWidth = math::round_up_to_multiple_of<32>(
                visibleBounds.width() + PADDING * 2);
            uint32_t coverageHeight = math::round_up_to_multiple_of<32>(
                visibleBounds.height() + PADDING * 2);

            // Get our coverage allocation.
            size_t offset = flush->allocateCoverageBufferRange(coverageHeight *
                                                               coverageWidth);
            if (offset == -1)
            {
                return false; // There wasn't room for our coverage buffer.
            }
            m_coverageBufferRange.offset =
                math::lossless_numeric_cast<uint32_t>(offset);
            m_coverageBufferRange.pitch = coverageWidth;
            m_coverageBufferRange.offsetX = -visibleBounds.left + PADDING;
            m_coverageBufferRange.offsetY = -visibleBounds.top + PADDING;
        }
    }
    return true;
}

void PathDraw::countSubpasses()
{
    m_subpassCount = 1;
    m_prepassCount = 0;

    switch (m_coverageType)
    {
        case CoverageType::pixelLocalStorage:
        case CoverageType::atlas:
            m_subpassCount = 1;
            break;

        case CoverageType::clockwiseAtomic:
            if (!isStroke())
            {
                m_prepassCount = 1; // Borrowed coverage.
            }
            m_subpassCount = 1;
            break;

        case CoverageType::msaa:
        {
            if (isStroke())
            {
                m_subpassCount = 1; // Strokes can be rendered in a single pass.
            }
            else if ((m_drawContents & gpu::kNestedClipUpdateMask) ==
                     gpu::kNestedClipUpdateMask)
            {
                // Nested clip updates only have a stencil pass. (The reset is
                // handled by a separate msaaStencilClipReset draw.)
                m_subpassCount = 1;
            }
            else if (m_drawContents & gpu::DrawContents::evenOddFill)
            {
                m_subpassCount = 2; // MSAA "slow" path: stencil-then-cover.
            }
            else
            {
                // MSAA "fast" path: (effectively) single pass rendering.
                m_subpassCount = 3;
            }
            if (isOpaque())
            {
                const bool usesClipping =
                    m_drawContents & (gpu::DrawContents::activeClip |
                                      gpu::DrawContents::clipUpdate);
                if (!usesClipping)
                {
                    // Render this path front-to-back instead of back-to-front.
                    assert(m_prepassCount == 0);
                    m_prepassCount = m_subpassCount;
                    m_subpassCount = 0;
                }
            }
        }
    }

    if (m_triangulator != nullptr)
    {
        // Each tessellation draw has a corresponding interior triangles draw.
        m_prepassCount *= 2;
        m_subpassCount *= 2;
    }
}

void PathDraw::pushToRenderContext(RenderContext::LogicalFlush* flush,
                                   int subpassIndex)
{
    // Make sure the rawPath in our path reference hasn't changed since we began
    // holding!
    assert(m_rawPathMutationID == m_pathRef->getRawPathMutationID());
    assert(!m_pathRef->getRawPath().empty());

    assert(m_resourceCounts.outerCubicTessVertexCount == 0 ||
           m_resourceCounts.midpointFanTessVertexCount == 0);
    uint32_t tessVertexCount = math::lossless_numeric_cast<uint32_t>(
        m_resourceCounts.outerCubicTessVertexCount |
        m_resourceCounts.midpointFanTessVertexCount);
    if (tessVertexCount == 0)
    {
        return;
    }

    if (m_pathID == 0)
    {
        // Reserve our pathID and write out a path record.
        m_pathID = flush->pushPath(this);
    }

    switch (m_coverageType)
    {
        case CoverageType::pixelLocalStorage:
        {
            if (subpassIndex == 0)
            {
                // Tessellation (midpoint fan or outer cubic).
                uint32_t tessLocation =
                    allocateTessellationVertices(flush, tessVertexCount);
                pushTessellationData(flush, tessVertexCount, tessLocation);
                pushTessellationDraw(flush, tessVertexCount, tessLocation);
            }
            else
            {
                // Interior triangles.
                assert(m_triangulator != nullptr);
                assert(subpassIndex == 1);
                RIVE_DEBUG_CODE(m_numInteriorTriangleVerticesPushed +=)
                flush->pushInteriorTriangulationDraw(this,
                                                     m_pathID,
                                                     gpu::WindingFaces::all);
                assert(m_numInteriorTriangleVerticesPushed <=
                       m_triangulator->maxVertexCount());
            }
            break;
        }

        case CoverageType::clockwiseAtomic:
            if (!isStroke())
            {
                // The subpass and prepass each emit half the vertices.
                assert(m_prepassCount == m_subpassCount);
                assert(tessVertexCount % 2 == 0);
                tessVertexCount /= 2;
            }
            switch (subpassIndex)
            {
                case -1: // Tessellation (borrowed, midpointFan or outerCubic).
                    assert(!isStroke());
                    m_prepassTessLocation =
                        allocateTessellationVertices(flush, tessVertexCount);
                    pushTessellationDraw(
                        flush,
                        tessVertexCount,
                        m_prepassTessLocation,
                        gpu::ShaderMiscFlags::borrowedCoveragePrepass);
                    break;

                case 0: // Tessellation (midpointFan or outerCubic).
                {
                    uint32_t tessLocation =
                        allocateTessellationVertices(flush, tessVertexCount);
                    pushTessellationData(flush, tessVertexCount, tessLocation);
                    pushTessellationDraw(flush, tessVertexCount, tessLocation);
                    break;
                }

                case -2: // Interior triangles (borrowed).
                case 1:  // Interior triangles.
                    assert(!isStroke());
                    assert(m_triangulator != nullptr);
                    RIVE_DEBUG_CODE(m_numInteriorTriangleVerticesPushed +=)
                    flush->pushInteriorTriangulationDraw(
                        this,
                        m_pathID,
                        subpassIndex < 0 ? gpu::WindingFaces::negative
                                         : gpu::WindingFaces::positive,
                        subpassIndex < 0
                            ? gpu::ShaderMiscFlags::borrowedCoveragePrepass
                            : gpu::ShaderMiscFlags::none);
                    assert(m_numInteriorTriangleVerticesPushed <=
                           m_triangulator->maxVertexCount());
                    break;

                default:
                    RIVE_UNREACHABLE();
            }
            break;

        case CoverageType::msaa:
        {
            assert(m_prepassCount == 0 || m_subpassCount == 0);
            int passCount = m_prepassCount | m_subpassCount;
            int passIdx = subpassIndex + m_prepassCount;
            if (passIdx == 0)
            {
                m_msaaTessLocation =
                    allocateTessellationVertices(flush, tessVertexCount);
                pushTessellationData(flush,
                                     tessVertexCount,
                                     m_msaaTessLocation);
            }
            constexpr static gpu::DrawType MSAA_FILL_TYPES[][3] = {
                // Nested clip update (passCount == 1; the reset is handled by a
                // separate msaaStencilClipReset draw.)
                {
                    gpu::DrawType::msaaMidpointFanPathsStencil,
                },

                // Slow path (passCount == 2): stencil-then-cover
                {
                    gpu::DrawType::msaaMidpointFanPathsStencil,
                    gpu::DrawType::msaaMidpointFanPathsCover,
                },

                // Fast path (passCount == 3): (mostly) single pass rendering.
                {
                    gpu::DrawType::msaaMidpointFanBorrowedCoverage,
                    gpu::DrawType::msaaMidpointFans,
                    gpu::DrawType::msaaMidpointFanStencilReset,
                },
            };
            assert(passCount <= 3);
            assert(passIdx < passCount);
            gpu::DrawType msaaDrawType =
                isStroke() ? gpu::DrawType::msaaStrokes
                           : MSAA_FILL_TYPES[passCount - 1][passIdx];
            flush->pushMidpointFanDraw(this,
                                       msaaDrawType,
                                       tessVertexCount,
                                       m_msaaTessLocation);
            break;
        }

        case CoverageType::atlas:
            // Atlas draws only have one subpass -- the rectangular blit from
            // the atlas to the screen. The step that renders coverage to the
            // offscreen atlas is handled separately, outside the subpass
            // system.
            assert(subpassIndex == 0);
            flush->pushAtlasBlit(this, m_pathID);
            break;
    }
}

void PathDraw::pushTessellationDraw(RenderContext::LogicalFlush* flush,
                                    uint32_t tessVertexCount,
                                    uint32_t tessLocation,
                                    gpu::ShaderMiscFlags shaderMiscFlags)
{
    if (m_triangulator != nullptr)
    {
        assert(!isStroke());
        flush->pushOuterCubicsDraw(this,
                                   gpu::DrawType::outerCurvePatches,
                                   tessVertexCount,
                                   tessLocation,
                                   shaderMiscFlags);
    }
    else
    {
        flush->pushMidpointFanDraw(
            this,
            isFeatheredFill() ? gpu::DrawType::midpointFanCenterAAPatches
                              : gpu::DrawType::midpointFanPatches,
            tessVertexCount,
            tessLocation,
            shaderMiscFlags);
    }
}

void PathDraw::pushAtlasTessellation(RenderContext::LogicalFlush* flush,
                                     uint32_t* tessVertexCount,
                                     uint32_t* tessBaseVertex)
{
    assert(m_coverageType == CoverageType::atlas);
    assert(m_resourceCounts.outerCubicTessVertexCount == 0 ||
           m_resourceCounts.midpointFanTessVertexCount == 0);

    *tessVertexCount = math::lossless_numeric_cast<uint32_t>(
        m_resourceCounts.outerCubicTessVertexCount |
        m_resourceCounts.midpointFanTessVertexCount);

    if (*tessVertexCount == 0)
    {
        assert(m_pathID == 0);
        return;
    }

    *tessBaseVertex = allocateTessellationVertices(flush, *tessVertexCount);
    pushTessellationData(flush, *tessVertexCount, *tessBaseVertex);
}

void PathDraw::pushTessellationData(RenderContext::LogicalFlush* flush,
                                    uint32_t tessVertexCount,
                                    uint32_t tessLocation)
{
    // Determine where to fill in forward and mirrored tessellations.
    uint32_t forwardTessVertexCount, forwardTessLocation,
        mirroredTessVertexCount, mirroredTessLocation;
    switch (m_contourDirections)
    {
        case gpu::ContourDirections::forward:
            forwardTessVertexCount = tessVertexCount;
            forwardTessLocation = tessLocation;
            mirroredTessLocation = mirroredTessVertexCount = 0;
            break;
        case gpu::ContourDirections::reverse:
            forwardTessVertexCount = forwardTessLocation = 0;
            mirroredTessVertexCount = tessVertexCount;
            mirroredTessLocation = tessLocation + tessVertexCount;
            break;
        case gpu::ContourDirections::reverseThenForward:
            if (m_coverageType == CoverageType::clockwiseAtomic && !isStroke())
            {
                // The tessellation for borrowed coverage was allocated at a
                // different location than the forward tessellation, both with
                // "tessVertexCount" vertices.
                assert(m_prepassTessLocation != 0); // With padding, this will
                                                    // only be zero if it wasn't
                                                    // initialized.
                forwardTessVertexCount = mirroredTessVertexCount =
                    tessVertexCount;
                forwardTessLocation = tessLocation;
                mirroredTessLocation = m_prepassTessLocation + tessVertexCount;
            }
            else
            {
                // The reverse and forward tessellations are allocated
                // contiguously, with a combined vertex count of
                // "tessVertexCount". (tessVertexCount/2 vertices each.)
                assert(tessVertexCount % 2 == 0);
                forwardTessVertexCount = mirroredTessVertexCount =
                    tessVertexCount / 2;
                forwardTessLocation = mirroredTessLocation =
                    tessLocation + tessVertexCount / 2;
            }
            break;
        case gpu::ContourDirections::forwardThenReverse:
            if (m_coverageType == CoverageType::clockwiseAtomic && !isStroke())
            {
                // The tessellation for borrowed coverage was allocated at a
                // different location than the forward tessellation, both with
                // "tessVertexCount" vertices.
                assert(m_prepassTessLocation != 0); // With padding, this will
                                                    // only be zero if it wasn't
                                                    // initialized.
                forwardTessVertexCount = mirroredTessVertexCount =
                    tessVertexCount;
                forwardTessLocation = m_prepassTessLocation;
                mirroredTessLocation = tessLocation + tessVertexCount;
            }
            else
            {
                // The reverse and forward tessellations are allocated
                // contiguously, with a combined vertex count of
                // "tessVertexCount". (tessVertexCount/2 vertices each.)
                assert(tessVertexCount % 2 == 0);
                forwardTessVertexCount = mirroredTessVertexCount =
                    tessVertexCount / 2;
                forwardTessLocation = tessLocation;
                mirroredTessLocation = tessLocation + tessVertexCount;
            }
            break;
    }

    // Write out the TessVertexSpans and path contours.
    RenderContext::TessellationWriter tessWriter(flush,
                                                 m_pathID,
                                                 m_contourDirections,
                                                 forwardTessVertexCount,
                                                 forwardTessLocation,
                                                 mirroredTessVertexCount,
                                                 mirroredTessLocation);

    if (m_triangulator != nullptr)
    {
        iterateInteriorTriangulation(
            InteriorTriangulationOp::pushOuterCubicTessellationData,
            nullptr,
            nullptr,
            TriangulatorAxis::dontCare,
            &tessWriter);
    }
    else
    {
        pushMidpointFanTessellationData(&tessWriter);
    }
}

void PathDraw::pushMidpointFanTessellationData(
    RenderContext::TessellationWriter* tessWriter)
{
    const RawPath& rawPath = m_pathRef->getRawPath();
    RawPath::Iter startOfContour = rawPath.begin();
    for (size_t i = 0; i < m_resourceCounts.contourCount; ++i)
    {
        // Push a contour and curve records.
        const ContourInfo& contour = m_contours[i];
        assert(startOfContour.verb() == PathVerb::move);
        assert(isStroke() || contour.closed); // Fills are always closed.
        RIVE_DEBUG_CODE(m_pendingStrokeJoinCount =
                            isStrokeOrFeather() ? contour.strokeJoinCount : 0;)
        RIVE_DEBUG_CODE(m_pendingStrokeCapCount =
                            contour.strokeCapSegmentCount != 0 ? 2 : 0;)

        const Vec2D* pts = startOfContour.rawPtsPtr();
        size_t curveIdx = contour.firstCurveIdx;
        size_t rotationIdx = contour.firstRotationIdx;
        const RawPath::Iter end = contour.endOfContour;
        uint32_t joinTypeFlags = 0;
        bool roundJoinStroked = false;
        // Emit a starting cap before the next cubic?
        bool needsFirstEmulatedCapAsJoin = false;
        uint32_t emulatedCapAsJoinFlags = 0;
        if (isStrokeOrFeather())
        {
            joinTypeFlags = isStroke() ? join_type_flags(m_strokeJoin)
                                       : FEATHER_JOIN_CONTOUR_FLAG;
            roundJoinStroked = joinTypeFlags == ROUND_JOIN_CONTOUR_FLAG;
            if (contour.strokeCapSegmentCount != 0)
            {
                StrokeCap cap =
                    !contour.closed
                        ? m_strokeCap
                        : empty_stroke_cap(true, m_strokeJoin, m_strokeCap);
                switch (cap)
                {
                    case StrokeCap::butt:
                        emulatedCapAsJoinFlags = BEVEL_JOIN_CONTOUR_FLAG;
                        break;
                    case StrokeCap::square:
                        emulatedCapAsJoinFlags = MITER_CLIP_JOIN_CONTOUR_FLAG;
                        break;
                    case StrokeCap::round:
                        emulatedCapAsJoinFlags = ROUND_JOIN_CONTOUR_FLAG;
                        break;
                }
                emulatedCapAsJoinFlags |= EMULATED_STROKE_CAP_CONTOUR_FLAG;
                needsFirstEmulatedCapAsJoin = true;
            }
        }

        // Make a data record for this current contour on the GPU.
        uint32_t contourIDWithFlags =
            m_contourFlags |
            tessWriter->pushContour(contour.midpoint,
                                    isStroke(),
                                    contour.closed,
                                    contour.paddingVertexCount);

        // When we don't have round joins, the number of segments per join is
        // constant. (Round joins have a variable number of segments per join,
        // depending on the angle.)
        uint32_t numSegmentsInNotRoundJoin;
        if (isFeatheredFill())
        {
            numSegmentsInNotRoundJoin =
                feather_join_segment_count(m_polarSegmentsPerRadian);
        }
        else
        {
            numSegmentsInNotRoundJoin = NUM_SEGMENTS_IN_MITER_OR_BEVEL_JOIN;
        }

        // Convert all curves in the contour to cubics and push them to the GPU.
        const int styleFlags =
            style_flags(isStrokeOrFeather(), roundJoinStroked);
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
                    implicitClose[1] = iter.movePt(); // In case we need an
                                                      // implicit closing line.
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
                        // End with a 180-degree join that looks like the stroke
                        // cap.
                        joinTangent =
                            -find_ending_tangent(pts, end.rawPtsPtr());
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
                        joinSegmentCount = numSegmentsInNotRoundJoin;
                        RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                    }
                    else
                    {
                        // End with a 180-degree join that looks like the stroke
                        // cap.
                        joinTangent =
                            -find_ending_tangent(pts, end.rawPtsPtr());
                        joinTypeFlags = emulatedCapAsJoinFlags;
                        joinSegmentCount = contour.strokeCapSegmentCount;
                        RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
                    }
                    [[fallthrough]];
                case StyledVerb::filledLine:
                line_common:
                {
                    std::array<Vec2D, 4> cubic =
                        convert_line_to_cubic(iter.linePts());
                    if (needsFirstEmulatedCapAsJoin)
                    {
                        // Emulate the start cap as a 180-degree join before the
                        // first stroke.
                        pushEmulatedStrokeCapAsJoinBeforeCubic(
                            tessWriter,
                            cubic.data(),
                            contour.strokeCapSegmentCount,
                            contourIDWithFlags | emulatedCapAsJoinFlags);
                        needsFirstEmulatedCapAsJoin = false;
                    }
                    tessWriter->pushCubic(cubic.data(),
                                          m_contourDirections,
                                          joinTangent,
                                          1,
                                          1,
                                          joinSegmentCount,
                                          contourIDWithFlags | joinTypeFlags);
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
                            // We have to chop carefully around stroked cusps in
                            // order to avoid rendering artifacts. Luckily,
                            // cusps are extremely rare in real-world content.
                            chop_cubic_around_cusps(
                                p,
                                localChopBuffer,
                                &m_chopVertices.pop_front().x,
                                chopKey >> 1,
                                m_strokeMatrixMaxScale);
                            p = localChopBuffer;
                            // The bottom bit of chopKey is 1, meaning
                            // "areCusps". Clearing the bottom bit leaves
                            // "numChops * 2", which is the number of chops a
                            // cusp needs!
                            numChops = chopKey ^ 1;
                            break;

                        case simple_chop_key(2): // 2 non-cusp chops
                        {
                            // Curves that need 2 chops are rare in real-world
                            // content. Just re-chop the curve this time around
                            // as well.
                            auto [t0, t1] = m_chopVertices.pop_front();
                            math::chop_cubic_at(p, localChopBuffer, t0, t1);
                            p = localChopBuffer;
                            numChops = 2;
                            break;
                        }
                        case simple_chop_key(1): // 1 non-cusp chop
                            // Single-chop curves were saved in the
                            // m_chopVertices queue.
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
                        // Emulate the start cap as a 180-degree join before the
                        // first stroke.
                        pushEmulatedStrokeCapAsJoinBeforeCubic(
                            tessWriter,
                            p,
                            contour.strokeCapSegmentCount,
                            contourIDWithFlags | emulatedCapAsJoinFlags);
                        needsFirstEmulatedCapAsJoin = false;
                    }
                    // Push chops before the final one.
                    for (size_t end = curveIdx + numChops; curveIdx != end;
                         ++curveIdx, ++rotationIdx, p += 3)
                    {
                        uint32_t parametricSegmentCount =
                            m_parametricSegmentCounts[curveIdx];
                        uint32_t polarSegmentCount =
                            m_polarSegmentCounts[rotationIdx];
                        tessWriter->pushCubic(p,
                                              m_contourDirections,
                                              joinTangent,
                                              parametricSegmentCount,
                                              polarSegmentCount,
                                              1,
                                              contourIDWithFlags |
                                                  joinTypeFlags);
                        RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                        RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    }
                    // Push the final chop, with a join.
                    uint32_t parametricSegmentCount =
                        m_parametricSegmentCounts[curveIdx++];
                    uint32_t polarSegmentCount =
                        m_polarSegmentCounts[rotationIdx++];
                    RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    if (contour.closed || !is_final_verb_of_contour(iter, end))
                    {
                        if (styledVerb == StyledVerb::roundJoinStrokedCubic)
                        {
                            joinTangent = m_tangentPairs[rotationIdx][1];
                            joinSegmentCount =
                                m_polarSegmentCounts[rotationIdx];
                            ++rotationIdx;
                            RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                        }
                        else
                        {
                            joinTangent = find_join_tangent(iter.cubicPts() + 3,
                                                            end.rawPtsPtr(),
                                                            contour.closed,
                                                            pts);
                            joinSegmentCount = numSegmentsInNotRoundJoin;
                        }
                        RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                    }
                    else
                    {
                        // End with a 180-degree join that looks like the stroke
                        // cap.
                        joinTangent =
                            -find_ending_tangent(pts, end.rawPtsPtr());
                        joinTypeFlags = emulatedCapAsJoinFlags;
                        joinSegmentCount = contour.strokeCapSegmentCount;
                        RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
                    }
                    tessWriter->pushCubic(p,
                                          m_contourDirections,
                                          joinTangent,
                                          parametricSegmentCount,
                                          polarSegmentCount,
                                          joinSegmentCount,
                                          contourIDWithFlags | joinTypeFlags);
                    RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                    break;
                }
                case StyledVerb::filledCubic:
                {
                    uint32_t parametricSegmentCount =
                        m_parametricSegmentCounts[curveIdx++];
                    tessWriter->pushCubic(iter.cubicPts(),
                                          m_contourDirections,
                                          Vec2D{},
                                          parametricSegmentCount,
                                          1,
                                          1,
                                          contourIDWithFlags);
                    RIVE_DEBUG_CODE(--m_pendingCurveCount;)
                    break;
                }
            }
        }

        if (needsFirstEmulatedCapAsJoin)
        {
            // The contour was empty. Emit both caps on p0.
            Vec2D p0 = pts[0], left = {p0.x - 1, p0.y},
                  right = {p0.x + 1, p0.y};
            pushEmulatedStrokeCapAsJoinBeforeCubic(
                tessWriter,
                std::array{p0, right, right, right}.data(),
                contour.strokeCapSegmentCount,
                contourIDWithFlags | emulatedCapAsJoinFlags);
            pushEmulatedStrokeCapAsJoinBeforeCubic(
                tessWriter,
                std::array{p0, left, left, left}.data(),
                contour.strokeCapSegmentCount,
                contourIDWithFlags | emulatedCapAsJoinFlags);
        }
        else if (contour.closed)
        {
            implicitClose[0] = end.rawPtsPtr()[-1];
            // Bit-cast to uint64_t because we don't want the special equality
            // rules for NaN. If we're empty or otherwise return back to p0, we
            // want to detect this, regardless of whether there are NaN values.
            if (math::bit_cast<uint64_t>(implicitClose[0]) !=
                math::bit_cast<uint64_t>(implicitClose[1]))
            {
                // Draw a line back to the beginning of the contour.
                std::array<Vec2D, 4> cubic =
                    convert_line_to_cubic(implicitClose);
                // Closing join back to the beginning of the contour.
                if (roundJoinStroked)
                {
                    joinTangent = m_tangentPairs[rotationIdx][1];
                    joinSegmentCount = m_polarSegmentCounts[rotationIdx];
                    ++rotationIdx;
                    RIVE_DEBUG_CODE(--m_pendingRotationCount;)
                    RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                }
                else if (isStrokeOrFeather())
                {
                    joinTangent = find_starting_tangent(pts, end.rawPtsPtr());
                    joinSegmentCount = numSegmentsInNotRoundJoin;
                    RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
                }
                tessWriter->pushCubic(cubic.data(),
                                      m_contourDirections,
                                      joinTangent,
                                      1,
                                      1,
                                      joinSegmentCount,
                                      contourIDWithFlags | joinTypeFlags);
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

void PathDraw::pushEmulatedStrokeCapAsJoinBeforeCubic(
    RenderContext::TessellationWriter* tessWriter,
    const Vec2D cubic[],
    uint32_t strokeCapSegmentCount,
    uint32_t contourIDWithFlags)
{
    // Reverse the cubic and push it with zero parametric and polar segments,
    // and a 180-degree join tangent. This results in a solitary join,
    // positioned immediately before the provided cubic, that looks like the
    // desired stroke cap.
    assert(strokeCapSegmentCount >= 2);
    tessWriter->pushCubic(
        std::array{cubic[3], cubic[2], cubic[1], cubic[0]}.data(),
        m_contourDirections,
        math::find_cubic_tan0(cubic),
        0,
        0,
        strokeCapSegmentCount,
        contourIDWithFlags);
    RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
    RIVE_DEBUG_CODE(--m_pendingEmptyStrokeCountForCaps;)
}

void PathDraw::iterateInteriorTriangulation(
    InteriorTriangulationOp op,
    TrivialBlockAllocator* allocator,
    RawPath* scratchPath,
    TriangulatorAxis triangulatorAxis,
    RenderContext::TessellationWriter* tessWriter)
{
    Vec2D chops[kMaxCurveSubdivisions * 3 + 1];
    const RawPath& rawPath = m_pathRef->getRawPath();
    assert(!rawPath.empty());
    wangs_formula::VectorXform vectorXform(m_matrix);
    size_t patchCount = 0;
    size_t contourCount = 0;
    Vec2D p0 = {0, 0};
    if (op == InteriorTriangulationOp::countDataAndTriangulate)
    {
        scratchPath->rewind();
    }
    // Used with InteriorTriangulationOp::pushOuterCubicData.
    uint32_t contourIDWithFlags = 0;
    for (const auto [verb, pts] : rawPath)
    {
        switch (verb)
        {
            case PathVerb::move:
                if (contourCount != 0 && pts[-1] != p0)
                {
                    if (op ==
                        InteriorTriangulationOp::pushOuterCubicTessellationData)
                    {
                        tessWriter->pushCubic(
                            convert_line_to_cubic(pts[-1], p0).data(),
                            m_contourDirections,
                            {0, 0},
                            kPatchSegmentCountExcludingJoin,
                            1,
                            kJoinSegmentCount,
                            contourIDWithFlags |
                                CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
                    }
                    ++patchCount;
                }
                if (op == InteriorTriangulationOp::countDataAndTriangulate)
                {
                    scratchPath->move(pts[0]);
                }
                else
                {
                    contourIDWithFlags =
                        m_contourFlags |
                        tessWriter->pushContour({0, 0},
                                                isStroke(),
                                                /*closed=*/true,
                                                0);
                }
                p0 = pts[0];
                ++contourCount;
                break;
            case PathVerb::line:
                if (op == InteriorTriangulationOp::countDataAndTriangulate)
                {
                    scratchPath->line(pts[1]);
                }
                else
                {
                    tessWriter->pushCubic(
                        convert_line_to_cubic(pts).data(),
                        m_contourDirections,
                        {0, 0},
                        kPatchSegmentCountExcludingJoin,
                        1,
                        kJoinSegmentCount,
                        contourIDWithFlags |
                            CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
                }
                ++patchCount;
                break;
            case PathVerb::quad:
                RIVE_UNREACHABLE();
            case PathVerb::cubic:
            {
                uint32_t numSubdivisions;
                if (op == InteriorTriangulationOp::countDataAndTriangulate)
                {
                    numSubdivisions =
                        find_outer_cubic_subdivision_count(pts, vectorXform);
                    m_numChops.push_back(numSubdivisions);
                }
                else
                {
                    numSubdivisions = m_numChops.pop_front();
                }
                if (numSubdivisions == 1)
                {
                    if (op == InteriorTriangulationOp::countDataAndTriangulate)
                    {
                        scratchPath->line(pts[3]);
                    }
                    else
                    {
                        tessWriter->pushCubic(
                            pts,
                            m_contourDirections,
                            {0, 0},
                            kPatchSegmentCountExcludingJoin,
                            1,
                            kJoinSegmentCount,
                            contourIDWithFlags |
                                CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
                    }
                }
                else
                {
                    // Passing nullptr for the 'tValues' causes it to chop the
                    // cubic uniformly in T.
                    math::chop_cubic_at(pts,
                                        chops,
                                        nullptr,
                                        numSubdivisions - 1);
                    const Vec2D* chop = chops;
                    for (size_t i = 0; i < numSubdivisions; ++i)
                    {
                        if (op ==
                            InteriorTriangulationOp::countDataAndTriangulate)
                        {
                            scratchPath->line(chop[3]);
                        }
                        else
                        {
                            tessWriter->pushCubic(
                                chop,
                                m_contourDirections,
                                {0, 0},
                                kPatchSegmentCountExcludingJoin,
                                1,
                                kJoinSegmentCount,
                                contourIDWithFlags |
                                    CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
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
        if (op == InteriorTriangulationOp::pushOuterCubicTessellationData)
        {
            tessWriter->pushCubic(
                convert_line_to_cubic(lastPt, p0).data(),
                m_contourDirections,
                {0, 0},
                kPatchSegmentCountExcludingJoin,
                1,
                kJoinSegmentCount,
                contourIDWithFlags |
                    CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
        }
        ++patchCount;
    }

    if (op == InteriorTriangulationOp::countDataAndTriangulate)
    {
        assert(m_triangulator == nullptr);
        assert(triangulatorAxis != TriangulatorAxis::dontCare);
        m_triangulator = allocator->make<GrInnerFanTriangulator>(
            *scratchPath,
            m_matrix,
            triangulatorAxis == TriangulatorAxis::horizontal
                ? GrTriangulator::Comparator::Direction::kHorizontal
                : GrTriangulator::Comparator::Direction::kVertical,
            // clockwise and nonZero paths both get triangulated as nonZero,
            // because clockwise fill still needs the backwards triangles for
            // borrowed coverage.
            m_pathFillRule == FillRule::evenOdd ? FillRule::evenOdd
                                                : FillRule::nonZero,
            allocator);
        float matrixDeterminant =
            m_matrix[0] * m_matrix[3] - m_matrix[2] * m_matrix[1];
        if ((matrixDeterminant < 0) !=
            static_cast<bool>(m_contourFlags & NEGATE_PATH_FILL_COVERAGE_FLAG))
        {
            m_triangulator->negateWinding();
        }
        // We also draw each "grout" triangle using an outerCubic patch.
        patchCount += m_triangulator->groutList().count();

        if (patchCount > 0)
        {
            m_resourceCounts.pathCount = 1;
            m_resourceCounts.contourCount = contourCount;
            // maxTessellatedSegmentCount does not get doubled when we emit both
            // forward and mirrored contours because the forward and mirrored
            // pair both get packed into a single gpu::TessVertexSpan.
            m_resourceCounts.maxTessellatedSegmentCount = patchCount;
            // outerCubic patches emit their tessellated geometry twice: once
            // forward and once mirrored.
            m_resourceCounts.outerCubicTessVertexCount =
                gpu::ContourDirectionsAreDoubleSided(m_contourDirections)
                    ? patchCount * kOuterCurvePatchSegmentSpan * 2
                    : patchCount * kOuterCurvePatchSegmentSpan;
            m_resourceCounts.maxTriangleVertexCount +=
                m_triangulator->maxVertexCount();
        }
    }
    else
    {
        assert(m_triangulator != nullptr);
        // Submit grout triangles, retrofitted into outerCubic patches.
        for (auto* node = m_triangulator->groutList().head(); node;
             node = node->fNext)
        {
            Vec2D triangleAsCubic[4] = {node->fPts[0],
                                        node->fPts[1],
                                        {0, 0},
                                        node->fPts[2]};
            tessWriter->pushCubic(triangleAsCubic,
                                  m_contourDirections,
                                  {0, 0},
                                  kPatchSegmentCountExcludingJoin,
                                  1,
                                  kJoinSegmentCount,
                                  contourIDWithFlags |
                                      RETROFITTED_TRIANGLE_CONTOUR_FLAG);
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

ImageRectDraw::ImageRectDraw(RenderContext* context,
                             IAABB pixelBounds,
                             const Mat2D& matrix,
                             BlendMode blendMode,
                             rcp<const Texture> imageTexture,
                             const ImageSampler imageSampler,
                             float opacity) :
    Draw(pixelBounds,
         matrix,
         blendMode,
         std::move(imageTexture),
         imageSampler,
         Type::imageRect),
    m_opacity(opacity)
{
    // If we support image paints for paths, the client should draw a
    // rectangular path with an image paint instead of using this draw.
    assert(!context->frameSupportsImagePaintForPaths());
    m_resourceCounts.imageDrawCount = 1;
}

void ImageRectDraw::pushToRenderContext(RenderContext::LogicalFlush* flush,
                                        int subpassIndex)
{
    assert(subpassIndex == 0);
    flush->pushImageRectDraw(this);
}

ImageMeshDraw::ImageMeshDraw(IAABB pixelBounds,
                             const Mat2D& matrix,
                             BlendMode blendMode,
                             rcp<const Texture> imageTexture,
                             const ImageSampler imageSampler,
                             rcp<RenderBuffer> vertexBuffer,
                             rcp<RenderBuffer> uvBuffer,
                             rcp<RenderBuffer> indexBuffer,
                             uint32_t indexCount,
                             float opacity) :
    Draw(pixelBounds,
         matrix,
         blendMode,
         std::move(imageTexture),

         imageSampler,
         Type::imageMesh),
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

void ImageMeshDraw::pushToRenderContext(RenderContext::LogicalFlush* flush,
                                        int subpassIndex)
{
    assert(subpassIndex == 0);
    flush->pushImageMeshDraw(this);
}

void ImageMeshDraw::releaseRefs()
{
    Draw::releaseRefs();
    m_vertexBufferRef->unref();
    m_uvBufferRef->unref();
    m_indexBufferRef->unref();
}

StencilClipReset::StencilClipReset(RenderContext* context,
                                   uint32_t previousClipID,
                                   gpu::DrawContents previousClipDrawContents,
                                   ResetAction resetAction) :
    Draw(context->getClipContentBounds(previousClipID),
         Mat2D(),
         BlendMode::srcOver,
         nullptr,
         ImageSampler::LinearClamp(),
         Type::stencilClipReset),
    m_previousClipID(previousClipID)
{
    constexpr static gpu::DrawContents FILL_RULE_FLAGS =
        gpu::DrawContents::nonZeroFill | gpu::DrawContents::evenOddFill |
        gpu::DrawContents::clockwiseFill;
    m_drawContents |= previousClipDrawContents & FILL_RULE_FLAGS;
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

void StencilClipReset::pushToRenderContext(RenderContext::LogicalFlush* flush,
                                           int subpassIndex)
{
    assert(subpassIndex == 0);
    flush->pushStencilClipResetDraw(this);
}
} // namespace rive::gpu
