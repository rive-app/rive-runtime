/*
 * Copyright 2023 Rive
 */

#include "rive/renderer/draw.hpp"

#include "gr_inner_fan_triangulator.hpp"
#include "rive_render_path.hpp"
#include "rive_render_paint.hpp"
#include "rive/math/bezier_utils.hpp"
#include "rive/math/wangs_formula.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "rive/renderer/stack_vector.hpp"
#include "rive/renderer/texture.hpp"
#include "gradient.hpp"
#include "shaders/constants.glsl"
#include "rive/profiler/profiler_macros.h"
#include <cmath>

namespace rive::gpu
{
namespace
{
// Maximum # of outerCurve patches a curve on the path can be subdivided into.
constexpr static size_t kMaxCurveSubdivisions = (kMaxParametricSegments +
                                                 OuterCubicPatchSegmentSpan -
                                                 1) /
                                                OuterCubicPatchSegmentSpan;

static uint32_t find_outer_cubic_subdivision_count(
    const Vec2D pts[],
    const wangs_formula::VectorXform& vectorXform)
{
  float numSubdivisions = ceilf(
      wangs_formula::cubic(pts, kParametricPrecision, vectorXform) *
      (1.f / OuterCubicPatchSegmentSpan));
  return static_cast<uint32_t>(
      math::clamp(numSubdivisions, 1, kMaxCurveSubdivisions));
}

constexpr static int NumSpokesInMiterOrBevelJoin = 4;
// depthStencil bevels need 2 internal spokes: both colocated the ends, but with
// AA outsets in the direction of the bevel.
constexpr static int NumSpokesInDepthStencilBevelJoin = 2;
// depthStencil miters need 3 internal spokes: all colocated on the miter
// corner, with different AA outsets.
constexpr static int NumSpokesInDepthStencilMiterJoin = 3;
// depthStencil miterClip joins need 4 internal spokes: 2 colocated per corner,
// with different AA outsets.
constexpr static int NumSpokesInDepthStencilMiterClipJoin = 4;
// "Fixed" join types, i.e., miter or bevel (not round), all have a fixed number
// of spokes regardless of angle.
constexpr static int numSpokesInFixedJoin(
    PathDraw::CoverageType coverageType,
    StrokeJoin join)
{
  assert(join != StrokeJoin::round);
  if (coverageType == PathDraw::CoverageType::depthStencil)
  {
    switch (join)
    {
      case StrokeJoin::bevel:
        return NumSpokesInDepthStencilBevelJoin;
      case StrokeJoin::miter:
        return NumSpokesInDepthStencilMiterJoin;
      case StrokeJoin::round:
        RIVE_UNREACHABLE();
    }
  }
  return NumSpokesInMiterOrBevelJoin;
}
constexpr static int numSegmentsInFixedJoin(
    PathDraw::CoverageType coverageType,
    StrokeJoin join)
{
  // Since a join sits between 2 cubics, it already has an implicit spoke at
  // either end, shared with its cubic neighbor. A join SEGMENT count is
  // therefore its internal spoke count plus 1.
  return numSpokesInFixedJoin(coverageType, join) + 1;
}
constexpr static int numSegmentsInMiterClipJoin(
    PathDraw::CoverageType coverageType)
{
  return (coverageType != PathDraw::CoverageType::depthStencil
              ? NumSpokesInMiterOrBevelJoin
              : NumSpokesInDepthStencilMiterClipJoin) +
         1;
}

constexpr static int STROKE_OR_FEATHER_STYLE_FLAG = 8;
constexpr static int ROUND_JOIN_STYLE_FLAG = STROKE_OR_FEATHER_STYLE_FLAG << 1;
RIVE_ALWAYS_INLINE constexpr int style_flags(
    bool isStrokeOrFeather,
    bool roundJoinStroked)
{
  int styleFlags = (isStrokeOrFeather << 3) | (roundJoinStroked << 4);
  assert(bool(styleFlags & STROKE_OR_FEATHER_STYLE_FLAG) == isStrokeOrFeather);
  assert(bool(styleFlags & ROUND_JOIN_STYLE_FLAG) == roundJoinStroked);
  return styleFlags;
}

// Switching on a StyledVerb reduces "if (stroked)" branching and makes the code
// cleaner.
enum class StyledVerb
{
  filledMove = static_cast<int>(PathVerb::move),
  strokedMove = STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::move),
  roundJoinStrokedMove = STROKE_OR_FEATHER_STYLE_FLAG | ROUND_JOIN_STYLE_FLAG |
                         static_cast<int>(PathVerb::move),

  filledLine = static_cast<int>(PathVerb::line),
  strokedLine = STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::line),
  roundJoinStrokedLine = STROKE_OR_FEATHER_STYLE_FLAG | ROUND_JOIN_STYLE_FLAG |
                         static_cast<int>(PathVerb::line),

  filledQuad = static_cast<int>(PathVerb::quad),
  strokedQuad = STROKE_OR_FEATHER_STYLE_FLAG | static_cast<int>(PathVerb::quad),
  roundJoinStrokedQuad = STROKE_OR_FEATHER_STYLE_FLAG | ROUND_JOIN_STYLE_FLAG |
                         static_cast<int>(PathVerb::quad),

  filledCubic = static_cast<int>(PathVerb::cubic),
  strokedCubic = STROKE_OR_FEATHER_STYLE_FLAG |
                 static_cast<int>(PathVerb::cubic),
  roundJoinStrokedCubic = STROKE_OR_FEATHER_STYLE_FLAG | ROUND_JOIN_STYLE_FLAG |
                          static_cast<int>(PathVerb::cubic),

  filledClose = static_cast<int>(PathVerb::close),
  strokedClose = STROKE_OR_FEATHER_STYLE_FLAG |
                 static_cast<int>(PathVerb::close),
  roundJoinStrokedClose = STROKE_OR_FEATHER_STYLE_FLAG | ROUND_JOIN_STYLE_FLAG |
                          static_cast<int>(PathVerb::close),
};
RIVE_ALWAYS_INLINE constexpr StyledVerb styled_verb(
    PathVerb verb,
    int styleFlags)
{
  return static_cast<StyledVerb>(styleFlags | static_cast<int>(verb));
}

// When chopping strokes, switching on a "chopKey" reduces "if (areChopsCusps)"
// branching and makes the code cleaner.
RIVE_ALWAYS_INLINE constexpr uint8_t chopKey(
    bool areChopsCusps,
    uint8_t numChops)
{
  return (numChops << 1) | static_cast<uint8_t>(areChopsCusps);
}
RIVE_ALWAYS_INLINE constexpr uint8_t cuspChopKey(uint8_t n)
{
  return chopKey(true, n);
}
RIVE_ALWAYS_INLINE constexpr uint8_t simpleChopKey(uint8_t n)
{
  return chopKey(false, n);
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
RIVE_ALWAYS_INLINE std::array<Vec2D, 4> convert_line_to_cubic(
    Vec2D p0,
    Vec2D p1)
{
  Vec2D line[2] = {p0, p1};
  return convert_line_to_cubic(line);
}

// Room for the pieces of any chopped cubic. Two cusp chops require storage for
// 16 points. This is because when chopping around a cusp, we temporarily
// produce three cubics (one before the cusp, a tiny one at the cusp, and one
// after the cusp), then remove the tiny middle one (it's replaced with a cusp
// join). So performing two cusp chops temporarily produces: cubic A, cusp
// cubic, cubic B, cusp cubic, cubic C. Each cubic requires three points plus
// the last cubic's end control point, bringing us to 16 points. (After removing
// the two cusp cubics, we're back down to 10 points.)
using ChopBuffer = std::array<Vec2D, 16>;

// Chops a cubic at each of its n cusps into n + 1 pieces that meet exactly on
// the cusp points. Strokes draw a cusp join across each cusp.
//
// To avoid zero tangents that occur at cusps, for a cusp at T, two chops are
// performed at T - EPSILON and T + EPSILON and the middle cubic is collapsed by
// snapping the surrounding two cubics' endpoints to the cusp's position.
//
// dst needs room for 6 * n + 4 points because it has to hold an intermediate
// result. The result is the first 3 * n + 4 points.
RIVE_ALWAYS_INLINE static void chopCubicAtCusps(
    const Vec2D p[4],
    ChopBuffer* dst,
    const float cuspT[],
    int n)
{
  float t[4];
  assert(n * 2 <= std::size(t));
  assert(6 * n + 4 <= std::tuple_size<ChopBuffer>::value);
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

  // chopCubicAt() will produce 2n + 1 pieces, with piece j at dst[3 * j].
  // Each odd piece is a sliver around a cusp.
  math::chopCubicAt(p, dst->data(), t, n * 2);

  // Collapse each sliver to the cusp by placing the previous piece's last
  // control point (which is shared with the next piece's first control
  // point) directly at the cusp's position and shifting the next piece down
  // in the list.
  ChopBuffer& pts = *dst;
  for (int i = 0; i < n; ++i)
  {
    pts[3 * i + 3] = math::evalCubicAt(p, cuspT[i]);
    pts[3 * i + 4] = pts[6 * i + 7];
    pts[3 * i + 5] = pts[6 * i + 8];
  }
  // Copy the cubic's original endpoint.
  pts[3 * n + 3] = pts[6 * n + 3];
}

// Chops a stroked cubic at the T values its chop key calls for, writing the
// pieces end to end into dst. The key must have at least one chop.
RIVE_ALWAYS_INLINE static void chopCubic(
    const Vec2D p[4],
    uint8_t chopKey,
    const float t[2],
    ChopBuffer* dst)
{
  switch (chopKey)
  {
    case cuspChopKey(2): // 2 cusps
    case cuspChopKey(1): // 1 cusp
      chopCubicAtCusps(p, dst, t, chopKey >> 1);
      break;
    case simpleChopKey(2): // 2 non-cusp chops
      math::chopCubicAt(p, dst->data(), t[0], t[1]);
      break;
    case simpleChopKey(1): // 1 non-cusp chop
      math::chopCubicAt(p, dst->data(), t[0]);
      break;
    default:
      RIVE_UNREACHABLE();
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

static Vec2D find_join_tangent_full_impl(
    const Vec2D* joinPoint,
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

RIVE_ALWAYS_INLINE Vec2D find_join_tangent(
    const Vec2D* joinPoint,
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

RIVE_ALWAYS_INLINE bool is_final_verb_of_contour(
    const RawPath::Iter& iter,
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

inline float find_atlas_feather_scale_factor(
    float featherRadius,
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
  uint32_t n = static_cast<uint32_t>(ceilf(polarSegmentsPerRadian * math::PI)) +
               FEATHER_JOIN_HELPER_SEGMENT_COUNT;
  n = std::max(n, FEATHER_JOIN_MIN_SEGMENT_COUNT);
  // FEATHER_POLAR_SEGMENT_MIN_ANGLE should limit n long before we reach
  // kMaxPolarSegments.
  assert(n < gpu::kMaxPolarSegments);
  return n;
}

// Finds where each of a stroke's batched cubics gets chopped, four at a time.
// Returns the total number of chops.
RIVE_NEVER_INLINE static size_t findCubicChops(
    RenderContext::CubicBatch* batch,
    size_t cubicCount)
{
  // Most cubics need no chops. The candidates' keys get filled in below.
  batch->chopKeys.assign(
      math::round_up_to_multiple_of<4>(cubicCount),
      chopKey(false, 0));
  batch->chopCandidates.clear();

  const float4* blocks = batch->points.data();
  for (size_t blockStart = 0; blockStart < cubicCount; blockStart += 4)
  {
    // Padding lanes hold copies of the last cubic, so whole blocks are safe
    // to process.
    const float4* block = blocks + blockStart * 2;
    auto [p0x, p0y, p1x, p1y] = simd::load4x4f(
        reinterpret_cast<const float*>(block));
    auto [p2x, p2y, p3x, p3y] = simd::load4x4f(
        reinterpret_cast<const float*>(block + 4));
    // The early-out finds cubics that need no chops four at a time. The
    // rest become candidates for the chop finder.
    uint32_t candidateLanes = ~simd::bitmask(math::cubicsNeedNoChops(
                                  p0x,
                                  p0y,
                                  p1x,
                                  p1y,
                                  p2x,
                                  p2y,
                                  p3x,
                                  p3y)) &
                              0xf;
    if (candidateLanes == 0)
    {
      continue;
    }
    if (cubicCount - blockStart < 4)
    {
      // Padding lanes are never candidates.
      candidateLanes &= (1u << (cubicCount - blockStart)) - 1;
    }
    for (uint32_t lane = 0; lane < 4; ++lane)
    {
      if (candidateLanes & (1u << lane))
      {
        batch->chopCandidates.push_back(
            math::lossless_numeric_cast<uint32_t>(blockStart + lane));
      }
    }
  }

  // Find the candidates' chops four at a time.
  size_t chopCount = 0;
  const uint32_t* candidates = batch->chopCandidates.data();
  size_t candidateCount = batch->chopCandidates.size();
  // Room to store whole groups of 4.
  size_t paddedCandidateCount = math::round_up_to_multiple_of<4>(
      candidateCount);
  if (batch->chopT0.size() < paddedCandidateCount)
  {
    batch->chopT0.resize(paddedCandidateCount);
    batch->chopT1.resize(paddedCandidateCount);
  }

  for (size_t first = 0; first < candidateCount; first += 4)
  {
    // Gather up to four candidates' rows, repeating the last to fill a
    // partial group.
    float4 p01[4];
    float4 p23[4];
    for (size_t lane = 0; lane < 4; ++lane)
    {
      uint32_t i = candidates[std::min(first + lane, candidateCount - 1)];
      const float4* block = blocks + (i / 4) * 8;
      p01[lane] = block[i % 4];
      p23[lane] = block[4 + i % 4];
    }
    auto [p0x, p0y, p1x, p1y] = simd::transpose4x4f(
        p01[0],
        p01[1],
        p01[2],
        p01[3]);
    auto [p2x, p2y, p3x, p3y] = simd::transpose4x4f(
        p23[0],
        p23[1],
        p23[2],
        p23[3]);
    math::Convex180Chops4 chops = math::findCubicConvex180Chops4(
        p0x,
        p0y,
        p1x,
        p1y,
        p2x,
        p2y,
        p3x,
        p3y);
    // A group's candidates have consecutive slots, so store all 4 lanes at
    // once. The scalar lanes below overwrite theirs.
    simd::store(batch->chopT0.data() + first, chops.T0);
    simd::store(batch->chopT1.data() + first, chops.T1);

    for (size_t lane = 0; lane < 4 && first + lane < candidateCount; ++lane)
    {
      uint32_t i = candidates[first + lane];
      uint8_t numChops;
      if (chops.possibleCusps[lane])
      {
        // Cusps are rare enough to handle one at a time.
        Vec2D p[4];
        simd::store(&p[0].x, p01[lane]);
        simd::store(&p[2].x, p23[lane]);
        float t[2] = {0, 0};
        bool areChopsCusps = false;
        // Note: calling the full findCubicConvex180Chops() is doing a
        // bit of redundant work (e.g. the early-out check, and tests
        // for non-cusp cases). I haven't optimized for this case
        // because the possibleCusps branch is fairly rare.
        numChops = math::findCubicConvex180Chops(p, t, &areChopsCusps);
        batch->chopKeys[i] = chopKey(areChopsCusps, numChops);
        batch->chopT0[first + lane] = t[0];
        batch->chopT1[first + lane] = t[1];
      }
      else
      {
        numChops = static_cast<uint8_t>(chops.numChops[lane]);
        batch->chopKeys[i] = chopKey(false, numChops);
      }
      chopCount += numChops;
    }
  }
  return chopCount;
}

// Finds the polar segment count of every rotation from its tangent pair, four
// at a time.
RIVE_NEVER_INLINE static void countPolarSegments(
    std::array<Vec2D, 2>* tangentPairs,
    size_t rotationCount,
    float polarSegmentsPerRadian,
    uint32_t* polarSegmentCounts)
{
  size_t paddedRotationCount = math::round_up_to_multiple_of<4>(rotationCount);
  for (size_t j = rotationCount; j < paddedRotationCount; ++j)
  {
    tangentPairs[j] = {Vec2D{0, 1}, Vec2D{0, 1}};
  }
  for (size_t j = 0; j < paddedRotationCount; j += 4)
  {
    auto [tx0, ty0, tx1, ty1] = simd::load4x4f(&tangentPairs[j][0].x);
    float4 numer = tx0 * tx1 + ty0 * ty1;
    float4 denomPow2 = (tx0 * tx0 + ty0 * ty0) * (tx1 * tx1 + ty1 * ty1);
#ifdef RIVE_HAS_FAST_INVERSE_SQRT
    float4 cosTheta = numer * simd::inverseSqrtFast(denomPow2);
#else
    float4 cosTheta = numer / simd::sqrt(denomPow2);
#endif
    cosTheta = simd::clamp(cosTheta, float4(-1), float4(1));
    float4 theta = simd::fast_acos(cosTheta);
    float4 n = simd::ceil(theta * polarSegmentsPerRadian);
    n = simd::clamp(n, float4(1), float4(kMaxPolarSegments));
    simd::store(polarSegmentCounts + j, simd::cast<uint32_t>(n));
  }
}

// Finds the parametric segment count of every piece, four at a time, and, when
// given tangentPairs, the tangents at both ends of each piece, which go to the
// piece's rotation slot. Returns the total parametric segment count.
static uint32_t processPieces(
    const float4* blocks,
    size_t pieceCount,
    float parametricPrecision,
    const wangs_formula::VectorXform& vectorXform,
    uint32_t* parametricSegmentCounts,
    std::array<Vec2D, 2>* tangentPairs,
    const uint32_t* rotationSlots)
{
  uint4 total = 0;
  for (size_t first = 0; first < pieceCount; first += 4)
  {
    const float4* block = blocks + first * 2;
    auto [p0x, p0y, p1x, p1y] = simd::load4x4f(
        reinterpret_cast<const float*>(block));
    auto [p2x, p2y, p3x, p3y] = simd::load4x4f(
        reinterpret_cast<const float*>(block + 4));
    uint4 counts = wangs_formula::cubicSegmentCounts<kMaxParametricSegments>(
        p0x,
        p0y,
        p1x,
        p1y,
        p2x,
        p2y,
        p3x,
        p3y,
        parametricPrecision,
        vectorXform);
    // parametricSegmentCounts has room for whole blocks.
    simd::store(parametricSegmentCounts + first, counts);
    // Padding lanes don't count toward the total.
    total += simd::if_then_else(
        uint4{0, 1, 2, 3} + uint32_t(first) < uint32_t(pieceCount),
        counts,
        uint4(0));
    if (tangentPairs == nullptr)
    {
      continue;
    }
    auto [tan0x, tan0y, tan1x, tan1y] = math::findCubicTangents4(
        p0x,
        p0y,
        p1x,
        p1y,
        p2x,
        p2y,
        p3x,
        p3y);
    // Each column is one piece's tangent pair.
    simd::gvec4x4<float> pairs = simd::transpose4x4f(
        tan0x,
        tan0y,
        tan1x,
        tan1y);
    size_t laneCount = std::min<size_t>(pieceCount - first, 4);
    for (size_t lane = 0; lane < laneCount; ++lane)
    {
      simd::store(
          tangentPairs[rotationSlots[first + lane]].data(),
          pairs.cols[lane]);
    }
  }
  return simd::reduce_add(total);
}
} // namespace

Draw::Draw(
    IAABB pixelBounds,
    const Mat2D& paintMatrix,
    const Mat2D* imageMatrix,
    BlendMode blendMode,
    float additiveness,
    rcp<Texture> imageTexture,
    ImageSampler imageSampler,
    Type type)
    : m_imageTextureRef(imageTexture.release())
    , m_imageSampler(imageSampler)
    , m_pixelBounds(pixelBounds)
    , m_paintMatrix(paintMatrix)
    , m_imageMatrix((imageMatrix != nullptr) ? *imageMatrix : paintMatrix)
    , m_blendMode(blendMode)
    , m_additiveness(math::clamp(additiveness, 0.f, 1.f))
    , m_type(type)
    , m_clippedPixelBounds(pixelBounds)
{
  if (m_blendMode != BlendMode::srcOver)
  {
    m_combinedDrawContents |= gpu::DrawContents::advancedBlend;
  }
}

void Draw::setClipID(uint32_t clipID)
{
  m_clipID = clipID;

  // For clipUpdates, m_clipID refers to the ID we are writing to the stencil
  // buffer (NOT the ID we are clipping against). It therefore doesn't affect
  // the activeClip flag in that case.
  if (!enums::is_flag_set(
          m_combinedDrawContents,
          gpu::DrawContents::clipUpdate))
  {
    if (m_clipID != 0)
    {
      m_combinedDrawContents |= gpu::DrawContents::activeClip;
    }
    else
    {
      m_combinedDrawContents &= ~gpu::DrawContents::activeClip;
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
        interlockMode == gpu::InterlockMode::depthStencil ||
        // Always switch to the atlas once we can render quarter-resultion.
        find_atlas_feather_scale_factor(
            featherRadiusFromFeather(paint->getFeather()),
            matrixMaxScale) <= .5f)
    {
      return CoverageType::featherAtlas;
    }
  }
  switch (interlockMode)
  {
    case gpu::InterlockMode::rasterOrdering:
    case gpu::InterlockMode::atomics:
      return CoverageType::pixelLocalStorage;
    case gpu::InterlockMode::clockwise:
      return CoverageType::clockwise;
    case gpu::InterlockMode::clockwiseAtomic:
      return CoverageType::clockwiseAtomic;
    case gpu::InterlockMode::depthStencil:
      return CoverageType::depthStencil;
  }
  RIVE_UNREACHABLE();
}

IAABB PathDraw::calculatePixelBounds(
    const Mat2D& paintMatrix,
    const RiveRenderPath* path,
    const RiveRenderPaint* paint,
    bool inkbleed)
{
  std::optional<StrokeParams> stroke;
  if (paint->getIsStroked())
  {
    stroke = paint->getStrokeParams();
  }
  return path->calculatePixelBounds(
      paintMatrix,
      stroke,
      paint->getFeather(),
      inkbleed && paint->getType() != gpu::PaintType::clipUpdate);
}

DrawUniquePtr PathDraw::Make(
    RenderContext* context,
    const Mat2D& paintMatrix,
    const Mat2D* imageMatrix,
    rcp<const RiveRenderPath> path,
    FillRule fillRule,
    const RiveRenderPaint* paint,
    float modulatedOpacity,
    ColorInt modulatedColor,
    std::optional<IAABB> precomputedPixelBounds)
{
  RIVE_PROF_SCOPE_L(2);
  assert(path != nullptr);
  assert(paint != nullptr);

  CoverageType coverageType = SelectCoverageType(
      paint,
      paintMatrix.findMaxScale(),
      context->platformFeatures(),
      context->frameInterlockMode());

  IAABB pixelBounds;

  // In debug mode, we want to always calculate the bounds and validate
  // against the pre-calculated ones so skip this
#ifdef NDEBUG
  if (precomputedPixelBounds.has_value())
  {
    // These were already computed so we don't need to do it again.
    pixelBounds = *precomputedPixelBounds;
  }
  else
#endif
  {
    // We weren't given pre-computed bounds, so calculate them.
    pixelBounds = calculatePixelBounds(
        paintMatrix,
        path.get(),
        paint,
        context->frameDescriptor().inkbleedOverride);
  }

  // In debug mode, we always compute the pixel bounds, so validate that the
  // value we got was exactly the same as the precomputed bounds.
  assert(
      !precomputedPixelBounds.has_value() ||
      pixelBounds == *precomputedPixelBounds);

  GrInnerFanTriangulator* triangulator = nullptr;
  const AABB& localBounds = path->getBounds();
  if (context->isOutsideCurrentFrame(pixelBounds))
  {
    // Anything precomputing the pixel bounds should be checking against the
    // screen bounds rather than letting it bubble down into here.
    assert(!precomputedPixelBounds.has_value());
    return nullptr;
  }

  // Skip interior triangulation on inkbleed until we have a compute-based
  // index generator. We're vertex bound on Android now, and outerCubic
  // patches generate a lot of vertex waste, which then gets compounded by the
  // multiple lanes and multiple passes of inkbleed hairlines.
  if (!paint->getIsStroked() && paint->getFeather() == 0 &&
      !context->frameDescriptor().inkbleedOverride)
  {
    // Use interior triangulation to draw filled paths if they're large
    // enough to benefit from it.
    //
    // FIXME! Implement interior triangulation for feathers.
    auto& triController = context->triangulationController();
    const size_t triVerbCount = path->getRawPath().verbs().count();
    const float triArea = gpu::find_transformed_area(localBounds, paintMatrix);
    if (triController.isEligible(triArea, triVerbCount))
    {
      triangulator = path->cachedTriangulator();
      if (triangulator != nullptr)
      {
        // A cached triangulation is free to reuse.
        triController.recordCacheHit();
      }
      else if (triController.admits(triArea, triVerbCount))
      {
        const double triStart = context->impl()->secondsNow();
        triangulator = path->createTriangulator(context->perFrameAllocator());
        triController.recordBuilt(context->impl()->secondsNow() - triStart);
      }
    }
  }

  auto draw = PathDrawUniquePtr(context->make<PathDraw>(
      pixelBounds,
      paintMatrix,
      imageMatrix,
      std::move(path),
      fillRule,
      paint,
      modulatedOpacity,
      modulatedColor,
      coverageType,
      context->frameDescriptor()));
  if (triangulator != nullptr)
  {
    draw->initForInteriorTriangulation(context, triangulator);
  }
  else
  {
    draw->initForMidpointFan(context, paint);
  }

  // Drop empty draws before they get sorted into batches.
  if (draw->isEmpty())
  {
    return nullptr;
  }

  return draw;
}

PathDraw::PathDraw(
    IAABB pixelBounds,
    const Mat2D& paintMatrix,
    const Mat2D* imageMatrix,
    rcp<const RiveRenderPath> path,
    FillRule initialFillRule,
    const RiveRenderPaint* paint,
    float modulatedOpacity,
    ColorInt modulatedColor,
    CoverageType coverageType,
    const RenderContext::FrameDescriptor& frameDesc)
    : Draw(
          pixelBounds,
          paintMatrix,
          imageMatrix,
          paint->getBlendMode(),
          paint->getAdditiveness(),
          ref_rcp(paint->getImageTexture()),
          paint->getImageSampler(),
          Type::path)
    , m_pathRef(path.release())
    , m_pathFillRule(
          frameDesc.clockwiseFillOverride ? FillRule::clockwise
                                          : initialFillRule)
    , m_gradientRef(
          paint->getModulatedGradient(modulatedOpacity, modulatedColor)
              .release())
    , m_inverseGradientTransform(paint->getInverseGradientTransform())
    , m_paintType(paint->getType())
    , m_coverageType(coverageType)
{
  assert(m_pathRef != nullptr);
  assert(!m_pathRef->getRawPath().empty());
  assert(paint != nullptr);

  if (paint->getIsOpaque() &&
      // Inkbleed strokes introduce coverage, which is no longer opaque.
      !(frameDesc.inkbleedOverride && paint->getIsStroked()))
  {
    m_combinedDrawContents |= gpu::DrawContents::opaquePaint;
  }

  m_hasHairlinePass = frameDesc.inkbleedOverride &&
                      m_coverageType == CoverageType::depthStencil &&
                      m_pathFillRule == FillRule::clockwise &&
                      !paint->getIsStroked() && paint->getFeather() == 0 &&
                      paint->getType() != gpu::PaintType::clipUpdate;

  if (paint->getIsLayerMask())
  {
    m_isLayerMask = true;
    m_layerMaskMode = paint->getLayerMaskMode();
  }

  if (paint->getFeather() != 0)
  {
    m_featherRadius = featherRadiusFromFeather(paint->getFeather());
    assert(!std::isnan(m_featherRadius)); // These should get culled in
                                          // RiveRenderer::drawPath().
    assert(m_featherRadius > 0);
  }

  if (paint->getIsStroked())
  {
    m_strokeRadius = paint->getThickness() * .5f;
    // Ensure stroke radius is nonzero. (In PLS, zero radius means the path
    // is filled.)
    m_strokeRadius = std::max(
        std::numeric_limits<float>::min(),
        m_strokeRadius);
    assert(!std::isnan(m_strokeRadius)); // These should get culled in
                                         // RiveRenderer::drawPath().
    assert(m_strokeRadius > 0);
  }

  // For atlased paths, m_combinedDrawContents refer to the rectangle being
  // drawn into the main render target (not the step that generates the atlas
  // mask, which uses, e.g., fill rules, etc.).
  if (m_coverageType != CoverageType::featherAtlas)
  {
    if (isStroke())
    {
      m_combinedDrawContents |= gpu::DrawContents::stroke;
    }
    else
    {
      if (m_featherRadius)
      {
        m_combinedDrawContents |= gpu::DrawContents::featheredFill;
      }
      if (initialFillRule == FillRule::clockwise ||
          frameDesc.clockwiseFillOverride)
      {
        m_combinedDrawContents |= gpu::DrawContents::clockwiseFill;
      }
      else if (initialFillRule == FillRule::nonZero)
      {
        m_combinedDrawContents |= gpu::DrawContents::nonZeroFill;
      }
      else if (initialFillRule == FillRule::evenOdd)
      {
        m_combinedDrawContents |= gpu::DrawContents::evenOddFill;
      }
    }
  }

  if (paint->getType() == gpu::PaintType::clipUpdate)
  {
    m_combinedDrawContents |= gpu::DrawContents::clipUpdate;
    if (paint->getSimpleValue().outerClipID != 0)
    {
      m_combinedDrawContents |= gpu::DrawContents::activeClip;
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
    float det = m_paintMatrix.xx() * m_paintMatrix.yy() -
                m_paintMatrix.yx() * m_paintMatrix.xy();
    if (det < 0)
    {
      m_contourDirections = (m_coverageType == CoverageType::depthStencil ||
                             m_coverageType == CoverageType::featherAtlas)
                                ? gpu::ContourDirections::reverse
                                : gpu::ContourDirections::forwardThenReverse;
      // Ignored by depthStencil.
      m_contourFlags |= NEGATE_PATH_FILL_COVERAGE_FLAG;
    }
    else
    {
      m_contourDirections = (m_coverageType == CoverageType::depthStencil ||
                             m_coverageType == CoverageType::featherAtlas)
                                ? gpu::ContourDirections::forward
                                : gpu::ContourDirections::reverseThenForward;
    }
  }
  else if (m_coverageType != CoverageType::depthStencil)
  {
    // atomic and rasterOrdering fills need reverse AND forward triangles.
    if (frameDesc.clockwiseFillOverride &&
        !m_pathRef->isClockwiseDominant(m_paintMatrix))
    {
      // For clockwiseFill, this is also our opportunity to logically
      // reverse the winding of the path, if it is predominantly
      // counterclockwise.
      m_contourDirections = (m_coverageType == CoverageType::featherAtlas)
                                ? gpu::ContourDirections::reverse
                                : gpu::ContourDirections::forwardThenReverse;
      m_contourFlags |= NEGATE_PATH_FILL_COVERAGE_FLAG;
    }
    else
    {
      m_contourDirections = (m_coverageType == CoverageType::featherAtlas)
                                ? gpu::ContourDirections::forward
                                : gpu::ContourDirections::reverseThenForward;
    }
  }
  else
  {
    if (initialFillRule == FillRule::nonZero || frameDesc.clockwiseFillOverride)
    {
      // Emit "nonZero" depthStencil fills in a direction such that the
      // dominant triangle winding area is always clockwise. This
      // maximizes pixel throughput since we will draw counterclockwise
      // triangles twice and clockwise only once.
      m_contourDirections = m_pathRef->isClockwiseDominant(m_paintMatrix)
                                ? gpu::ContourDirections::forward
                                : gpu::ContourDirections::reverse;
    }
    else
    {
      // "evenOdd" depthStencil fills just get drawn twice, so any
      // direction is fine.
      m_contourDirections = gpu::ContourDirections::forward;
    }
  }

  m_simplePaintValue = paint->getSimpleValue();

  // Apply modulation to the paint value.
  // Gradient modulation is handled upfront in the gradient initialization.
  if (modulatedOpacity != 1.0f || modulatedColor != 0xFFFFFFFF)
  {
    switch (m_paintType)
    {
      case gpu::PaintType::solidColor:
        m_simplePaintValue.color = colorModulate(
            m_simplePaintValue.color,
            modulatedColor,
            modulatedOpacity);
        break;
      case gpu::PaintType::linearGradient:
      case gpu::PaintType::radialGradient:
      case gpu::PaintType::angularGradient:
      case gpu::PaintType::clipUpdate:
        break;
    }
  }

  if (m_coverageType == CoverageType::featherAtlas)
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
  RIVE_PROF_SCOPE_L(2)
  Draw::releaseRefs();
  RIVE_DEBUG_CODE(m_pathRef->unlockRawPathMutations();)
  m_pathRef->unref();
  safe_unref(m_gradientRef);
}

// The cubics start out as one list, with one curve slot and one rotation slot
// each, as if none get chopped. For each cubic that does get chopped, its first
// piece replaces the unchopped entry in-place and its other pieces are appended
// to the end of the curve list. Their rotations, plus a cusp join before each
// piece that follows a cusp, are appended to the end of the rotation list.
// Returns the number of rotations added.
size_t PathDraw::appendChoppedPieces(
    RenderContext::CubicBatch* batch,
    size_t cubicCount,
    [[maybe_unused]] size_t contourCount,
    size_t mainRotationCount)
{
  float4* blocks = batch->points.data();
  uint32_t* rotationSlots = batch->rotationSlots.data();
  batch->cuspJoins.clear();
  auto writePiece = [blocks](size_t curveIdx, const Vec2D* p) {
    float4* block = blocks + (curveIdx / 4) * 8;
    block[curveIdx % 4] = simd::load4f(p);
    block[4 + curveIdx % 4] = simd::load4f(p + 2);
  };

  // The extra chopped entries start after all of the unchopped entries in the
  // curve and rotation lists
  size_t extraCurveIdx = cubicCount;
  size_t extraRotationIdx = mainRotationCount;
  ContourInfo* contour = m_contours;

  for (size_t candidateIdx = 0; candidateIdx < batch->chopCandidates.size();
       ++candidateIdx)
  {
    uint32_t k = batch->chopCandidates[candidateIdx];
    uint8_t chopKey = batch->chopKeys[k];
    uint8_t numChops = chopKey >> 1;
    if (numChops == 0)
    {
      continue;
    }

    bool areChopsCusps = (chopKey & 1) != 0;

    // Advance to the contour that owns this cubic, skipping any with no
    // cubics (e.g., only lines). A contour's curve slots are the indices
    // of its cubics (extraCurveCount tracks the appended pieces).
    while (k >= contour->endCurveIdx)
    {
      ++contour;
      assert(contour < m_contours + contourCount);
    }

    // Chop from the RawPath (batch->rawPathCubicPoints rather than
    // batch->points). The chop functions need the points in one contiguous
    // array, which the batch's block layout doesn't keep, and copying them
    // out of the block just to read them back would stall on store
    // forwarding.
    float t[2] = {batch->chopT0[candidateIdx], batch->chopT1[candidateIdx]};
    ChopBuffer localChopBuffer;
    chopCubic(batch->rawPathCubicPoints[k], chopKey, t, &localChopBuffer);

    // pushMidpointFanTessellationData() only gets the T values and chops
    // the cubic again, which measured cheaper than saving these pieces for
    // it to read back. (The pieces here only feed processPieces().)
    m_chopVertices.push_back() = {t[0], t[1]};

    const Vec2D* p = localChopBuffer.data();

    writePiece(k, p);
    uint32_t lastPieceRotationIdx = rotationSlots[k];
    for (int piece = 1; piece <= numChops; ++piece)
    {
      p += 3;
      if (areChopsCusps)
      {
        // A cusp join across the cusp before this piece. Its tangents
        // come from the pieces on either side.
        uint32_t joinIdx = math::lossless_numeric_cast<uint32_t>(
            extraRotationIdx++);
        uint32_t nextPieceRotationIdx = math::lossless_numeric_cast<uint32_t>(
            extraRotationIdx);
        batch->cuspJoins.push_back(
            {joinIdx, lastPieceRotationIdx, nextPieceRotationIdx});
        ++contour->cuspJoinCount;
        ++contour->extraRotationCount;
      }

      writePiece(extraCurveIdx, p);
      lastPieceRotationIdx = math::lossless_numeric_cast<uint32_t>(
          extraRotationIdx++);
      rotationSlots[extraCurveIdx++] = lastPieceRotationIdx;
      ++contour->extraCurveCount;
      ++contour->extraRotationCount;
    }
  }
  return extraRotationIdx - mainRotationCount;
}

RIVE_NEVER_INLINE uint32_t PathDraw::countStrokeContourVertices(
    ContourInfo* contour,
    bool empty,
    size_t* extraRotationIdx,
    size_t* emptyStrokeCountForCaps)
{
  uint32_t vertexCount = 0;
  // Polar and parametric segments share the first and final vertices.
  // Therefore:
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
  for (size_t j = contour->firstRotationIdx; j < contour->endRotationIdx; ++j)
  {
    vertexCount += m_polarSegmentCounts[j];
  }
  size_t extraIdx = *extraRotationIdx;
  for (size_t end = extraIdx + contour->extraRotationCount; extraIdx < end;
       ++extraIdx)
  {
    vertexCount += m_polarSegmentCounts[extraIdx];
  }
  *extraRotationIdx = extraIdx;

  // Count joins.
  if (!isStroke())
  {
    assert(isFeatheredFill());
    uint32_t numSegmentsInFeatherJoin = feather_join_segment_count(
        m_polarSegmentsPerRadian);
    vertexCount += contour->strokeJoinCount * (numSegmentsInFeatherJoin - 1);
  }
  else if (m_strokeJoin == StrokeJoin::round)
  {
    // Round joins share their beginning and ending spokes with the curve
    // on either side. Therefore, the number of spokes we need to allocate
    // for a round join is "joinSegmentCount - 1". Do all the -1's here.
    vertexCount -= contour->strokeJoinCount;
  }
  else
  {
    vertexCount += contour->strokeJoinCount *
                   numSpokesInFixedJoin(m_coverageType, m_strokeJoin);
  }
  // Cusp joins share both end vertices (like round joins). Their polar
  // segments were summed above, with the other rotations.
  vertexCount -= contour->cuspJoinCount;

  // Count stroke caps, if any.
  StrokeCap cap;
  bool needsCaps = false;
  if (!empty)
  {
    cap = m_strokeCap;
    needsCaps = !contour->closed;
  }
  else if (isStroke())
  {
    cap = empty_stroke_cap(contour->closed, m_strokeJoin, m_strokeCap);
    // Ignore butt caps when the contour is empty.
    needsCaps = cap != StrokeCap::butt;
  }
  if (needsCaps)
  {
    // We emulate stroke caps as 180-degree joins.
    if (cap == StrokeCap::round)
    {
      // Round caps rotate 180 degrees.
      float strokeCapSegmentCount = ceilf(m_polarSegmentsPerRadian * math::PI);
      // Make sure not to exceed kMaxPolarSegments.
      strokeCapSegmentCount = std::min<float>(
          kMaxPolarSegments,
          strokeCapSegmentCount);
      contour->strokeCapSegmentCount = static_cast<uint32_t>(
          strokeCapSegmentCount);
    }
    else
    {
      // Butt caps are emulated as bevel joins, square caps as
      // miterClip joins.
      contour->strokeCapSegmentCount =
          (cap == StrokeCap::square
               ? numSegmentsInMiterClipJoin(m_coverageType)
               : numSegmentsInFixedJoin(m_coverageType, StrokeJoin::bevel));
    }
    // +2 because caps emulated as joins need to emit spokes at T=0 and
    // T=1, unlike normal joins, which always have a cubic neighbor to
    // share with on both sides.
    contour->strokeCapSegmentCount = std::min(
        contour->strokeCapSegmentCount + 2,
        kMaxPolarSegments);
    // As long as a contour isn't empty, we can tack the end cap onto the
    // join section of the final curve in the stroke. Otherwise, we need to
    // introduce 0-tessellation-segment curves with non-empty joins to
    // carry the caps.
    *emptyStrokeCountForCaps += empty ? 2 : 1;
    vertexCount += (contour->strokeCapSegmentCount - 1) * 2;
  }
  return vertexCount;
}

void PathDraw::initForMidpointFan(
    RenderContext* context,
    const RiveRenderPaint* paint)
{
  RIVE_PROF_SCOPE_L(2)
  // Only call init() once.
  assert(
      (m_resourceCounts.midpointFanTessVertexCount |
       m_resourceCounts.outerCubicTessVertexCount) == 0);

  if (isStrokeOrFeather())
  {
    m_strokeMatrixMaxScale = m_paintMatrix.findMaxScale();

    float r_ = 0;
    if (m_featherRadius != 0)
    {
      r_ = m_featherRadius * m_strokeMatrixMaxScale;

      // Inverse of 1/calcPolarSegmentsPerRadian at
      // FEATHER_MIN_POLAR_SEGMENT_ANGLE.
      //
      // i.e., 1 / calcPolarSegmentsPerRadian<kPolarPrecision>(
      //           FEATHER_MIN_POLAR_SEGMENT_ANGLE) ==
      //       FEATHER_MAX_SCREEN_SPACE_RADIUS
      //
      constexpr static float FEATHER_MAX_SCREEN_SPACE_RADIUS =
          1 /
          (kPolarPrecision * (1 - COS_FEATHER_POLAR_SEGMENT_MIN_ANGLE_OVER_2));
      assert(math::nearly_equal(
          1 / math::calcPolarSegmentsPerRadian<kPolarPrecision>(
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
    m_polarSegmentsPerRadian = math::calcPolarSegmentsPerRadian<
        kPolarPrecision>(r_);

    m_strokeJoin = paint->getJoin();
    m_strokeCap = paint->getCap();
  }

  // Count up how much temporary storage this function will need to reserve in
  // CPU buffers.
  const RawPath& rawPath = m_pathRef->getRawPath();
  assert(rawPath.verbs()[0] == PathVerb::move);
  size_t contourCount = rawPath.verbCount(PathVerb::move);
  assert(contourCount != 0);

  m_contours = reinterpret_cast<ContourInfo*>(
      context->perFrameAllocator().alloc(sizeof(ContourInfo) * contourCount));

  assert(rawPath.verbCount(PathVerb::quad) == 0);
  size_t cubicCount = rawPath.verbCount(PathVerb::cubic);
  // Only cubics take curve slots. A stroked cubic chops into at most 3
  // pieces.
  size_t maxCurves = isStrokeOrFeather() ? cubicCount * 3 : cubicCount;
  size_t maxRotations = 0;
  if (isStrokeOrFeather())
  {
    // Each piece of a stroked cubic takes a rotation slot, as does the cusp
    // join across each cusp.
    maxRotations += cubicCount * 5;
    if (m_strokeJoin == StrokeJoin::round)
    {
      // If the stroke has round joins, we also record the rotations
      // between (pre-chopped) joins in order to calculate how many
      // vertices are in each round join. Each line or cubic has at most
      // one join before it, and closing a contour can add an additional
      // join. Note that we append `contourCount` rather than the number
      // of explicit `close` verbs to account for implicit closes.
      maxRotations += rawPath.verbCount(PathVerb::line) + cubicCount +
                      contourCount;
    }
  }

  // Each stroked cubic will record the number of chops it requires (either
  // 0, 1, or 2).
  size_t maxChops = isStrokeOrFeather() ? cubicCount : 0;
  // Each chopped cubic saves its T values for
  // pushMidpointFanTessellationData() to chop it again.
  size_t maxChopVertices = maxChops;
  // Curves and rotations are processed in whole blocks of 4.
  size_t maxPaddedRotations = isStrokeOrFeather()
                                  ? math::round_up_to_multiple_of<4>(
                                        maxRotations)
                                  : 0;
  size_t maxPaddedCurves = math::round_up_to_multiple_of<4>(maxCurves);

  // Reserve intermediate space for the polar segment counts of each curve and
  // round join.
  if (isStrokeOrFeather())
  {
    m_numChops.reset(context->numChopsAllocator(), maxChops);
    m_chopVertices.reset(context->chopVerticesAllocator(), maxChopVertices);
    m_tangentPairs = context->tangentPairsAllocator().alloc(maxPaddedRotations);
    m_polarSegmentCounts = context->polarSegmentCountsAllocator().alloc(
        maxPaddedRotations);
  }
  m_parametricSegmentCounts = context->parametricSegmentCountsAllocator().alloc(
      maxPaddedCurves);

  float parametricPrecision = gpu::kParametricPrecision;
  if (m_featherRadius > 1)
  {
    // Once the blur radius is above ~50 pixels, we don't have to tessellate
    // within 1/4px of the edge anymore.
    // At this point, tessellate within strokeRadius/200 pixels of the edge.
    // (parametricPrecision == 1/tolerance.)
    parametricPrecision = std::min(
        parametricPrecision * 100.f /
            (m_featherRadius * m_strokeMatrixMaxScale),
        parametricPrecision);
  }

  size_t lineCount = 0;
  size_t emptyStrokeCountForCaps = 0;

  // Iteration pass 1: Collect contours, round joins and line counts, and copy
  // the cubics into the batch buffer. Each cubic gets one curve and rotation
  // slot. appendChoppedPieces() will later add any slots for the extra pieces
  // that result from chopping.
  size_t contourIdx = 0;
  size_t curveIdx = 0;
  // We measure rotations on both curves and round joins.
  size_t rotationIdx = 0;
  bool roundJoinStroked = isStroke() && m_strokeJoin == StrokeJoin::round;
  wangs_formula::VectorXform vectorXform(m_paintMatrix);

  RenderContext::CubicBatch& cubicBatch = context->cubicBatch();
  cubicBatch.resize(cubicCount, maxCurves, isStroke());
  float4* cubicBlocks = cubicBatch.points.data();
  // Where the next cubic's [p0 p1] goes. Its [p2 p3] goes 4 lanes later.
  float4* nextCubicLane = cubicBlocks;
  // Copies a cubic into its block and gives it the next curve slot. In pass 1
  // the curve slots are the cubics' indices.
  auto gatherCubic = [&](const Vec2D* p) {
    assert(curveIdx < cubicCount);
    nextCubicLane[0] = simd::load4f(p);
    nextCubicLane[4] = simd::load4f(p + 2);
    ++curveIdx;
    // Skip the [p2 p3] half after a block's last lane.
    nextCubicLane += (curveIdx % 4 != 0) ? 1 : 5;
  };

  RawPath::Iter startOfContour = rawPath.begin();
  RawPath::Iter end = rawPath.end();
  // Original number of lines and curves, before chopping.
  int preChopVerbCount = 0;
  Vec2D endpointsSum{};
  bool closed = !isStroke() || paint->getForceClosed();
  Vec2D lastTangent = {0, 1};
  Vec2D firstTangent = {0, 1};
  size_t roundJoinCount = 0;
  size_t contourFirstCurveIdx = curveIdx;
  size_t contourFirstRotationIdx = rotationIdx;
  // Inlined so the counters it shares with the loop can stay in registers.
  auto finishAndAppendContour = [&](RawPath::Iter iter) RIVE_INLINE_LAMBDA {
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
        0,                 // cuspJoinCount (see appendChoppedPieces())
        0,                 // extraCurveCount
        0,                 // extraRotationCount
        0,                 // strokeCapSegmentCount
        0,                 // paddingVertexCount
        RIVE_DEBUG_CODE(0) // tessVertexCount
    };
    contourFirstCurveIdx = curveIdx;
    contourFirstRotationIdx = rotationIdx;
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
        closed = !isStroke() || paint->getForceClosed();
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
        math::findCubicTangents(p, unchoppedTangents);
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
        assert(curveIdx < maxPaddedCurves);
        assert(rotationIdx < maxPaddedRotations);
        if (isStroke())
        {
          // Where this cubic's tangents go. processPieces() fills
          // them in after chopping, once the slot holds the cubic's
          // first piece.
          cubicBatch
              .rotationSlots[curveIdx] = math::lossless_numeric_cast<uint32_t>(
              rotationIdx);
          cubicBatch.rawPathCubicPoints[curveIdx] = p;
        }
        else
        {
          // FIXME: Feathered fills don't have polar segments for now,
          // but we're leaving space for them in m_tangentPairs
          // because we will convert them to use a similar concept of
          // "polar joins" once we move them onto the GPU.
          m_tangentPairs[rotationIdx] = {Vec2D{0, 1}, Vec2D{0, 1}};
        }
        gatherCubic(p);
        ++rotationIdx;
        break;
      }
      case StyledVerb::filledCubic:
      {
        const Vec2D* p = iter.cubicPts();
        ++preChopVerbCount;
        endpointsSum += p[3];
        assert(curveIdx < maxPaddedCurves);
        gatherCubic(p);
        break;
      }
    }
  }
  if (startOfContour != end)
  {
    finishAndAppendContour(end);
  }
  assert(curveIdx == cubicCount);
  assert(contourIdx == contourCount);
  assert(contourCount > 0);
  assert(rotationIdx <= maxPaddedRotations);
  assert(isStrokeOrFeather() || maxPaddedRotations == 0);
  assert(isStrokeOrFeather() || rotationIdx == 0);

  // Fill the final block's unused lanes with copies of the last cubic, so
  // SIMD code never sees uninitialized data.
  if (size_t lastLane = cubicCount % 4; lastLane != 0)
  {
    float4* block = cubicBlocks + (cubicCount / 4) * 8;
    for (size_t lane = lastLane; lane < 4; ++lane)
    {
      block[lane] = block[lastLane - 1];
      block[4 + lane] = block[4 + lastLane - 1];
    }
  }

  // Chop strokes into sections that do not inflect (i.e, are convex), and do
  // not rotate more than 180 degrees. This is required by the GPU
  // parametric/polar sorter. (Feathered fills come here already chopped.)
  size_t chopCount = 0;
  if (isStroke() && cubicCount != 0)
  {
    chopCount = findCubicChops(&cubicBatch, cubicCount);
    // pushMidpointFanTessellationData() chops each cubic again from its
    // key and T values.
    memcpy(
        m_numChops.push_back_n(cubicCount),
        cubicBatch.chopKeys.data(),
        cubicCount);
  }
  else if (isStrokeOrFeather())
  {
    memset(m_numChops.push_back_n(cubicCount), chopKey(false, 0), cubicCount);
  }
  m_firstExtraCurveIdx = curveIdx;
  m_firstExtraRotationIdx = rotationIdx;
  if (chopCount != 0)
  {
    rotationIdx += appendChoppedPieces(
        &cubicBatch,
        cubicCount,
        contourCount,
        rotationIdx);
    curveIdx += chopCount;
  }
  assert(curveIdx <= maxPaddedCurves);
  assert(rotationIdx <= maxPaddedRotations);

  // Count every curve's parametric segments and, for strokes, find its
  // tangents.
  uint32_t parametricSegmentTotal = processPieces(
      cubicBatch.points.data(),
      curveIdx,
      parametricPrecision,
      vectorXform,
      m_parametricSegmentCounts,
      isStroke() ? m_tangentPairs : nullptr,
      cubicBatch.rotationSlots.data());
  if (chopCount != 0)
  {
    // Fill in the tangents for cusp joins that were added for cusp chops.
    // These just bridge the surrounding cubics' tangents.
    for (auto [joinIdx, beforeIdx, afterIdx] : cubicBatch.cuspJoins)
    {
      m_tangentPairs[joinIdx] = {
          m_tangentPairs[beforeIdx][1],
          m_tangentPairs[afterIdx][0]};
    }
  }

  // Return any data we conservatively allocated but did not use.
  size_t paddedCurveCount = math::round_up_to_multiple_of<4>(curveIdx);
  size_t paddedRotationCount = math::round_up_to_multiple_of<4>(rotationIdx);
  if (isStrokeOrFeather())
  {
    m_numChops.shrinkToFit(context->numChopsAllocator(), maxChops);
    m_chopVertices.shrinkToFit(
        context->chopVerticesAllocator(),
        maxChopVertices);
    context->tangentPairsAllocator().rewindLastAllocation(
        maxPaddedRotations - paddedRotationCount);
    context->polarSegmentCountsAllocator().rewindLastAllocation(
        maxPaddedRotations - paddedRotationCount);
  }
  context->parametricSegmentCountsAllocator().rewindLastAllocation(
      maxPaddedCurves - paddedCurveCount);

  if (isStrokeOrFeather())
  {
    countPolarSegments(
        m_tangentPairs,
        rotationIdx,
        m_polarSegmentsPerRadian,
        m_polarSegmentCounts);
  }

  // Iteration pass 2: Count the tessellation vertices in each contour.
  size_t contourFirstLineIdx = 0;
  size_t tessVertexCount = 0;
  size_t extraCurveIdx = m_firstExtraCurveIdx;
  size_t extraRotationIdx = m_firstExtraRotationIdx;
  for (size_t i = 0; i < contourCount; ++i)
  {
    ContourInfo* contour = &m_contours[i];
    size_t contourLineCount = contour->endLineIdx - contourFirstLineIdx;
    uint32_t contourVertexCount = math::lossless_numeric_cast<uint32_t>(
        contourLineCount * 2); // Each line tessellates to 2 vertices.

    if (contourCount == 1)
    {
      // A single contour has every curve.
      contourVertexCount += parametricSegmentTotal;
    }
    else
    {
      for (size_t c = contour->firstCurveIdx; c < contour->endCurveIdx; ++c)
      {
        contourVertexCount += m_parametricSegmentCounts[c];
      }
      for (size_t end = extraCurveIdx + contour->extraCurveCount;
           extraCurveIdx < end;
           ++extraCurveIdx)
      {
        contourVertexCount += m_parametricSegmentCounts[extraCurveIdx];
      }
    }

    if (isStrokeOrFeather())
    {
      bool empty = contour->endLineIdx == contourFirstLineIdx &&
                   contour->endCurveIdx == contour->firstCurveIdx;
      contourVertexCount += countStrokeContourVertices(
          contour,
          empty,
          &extraRotationIdx,
          &emptyStrokeCountForCaps);
    }
    else
    {
      // Fills don't have polar segments:
      //
      //   mergedVertexCount = parametricVertexCount =
      //   parametricSegmentCount + 1
      //
      // Just collect the +1 for each non-stroked curve.
      size_t contourCurveCount = contour->endCurveIdx - contour->firstCurveIdx;
      contourVertexCount += contourCurveCount;
    }

    // Add padding vertices until the number of tessellation vertices in the
    // contour is an exact multiple of kMidpointFanPatchSegmentSpan. This
    // ensures that patch boundaries align with contour boundaries.
    contour->paddingVertexCount = math::padding_to_align_up<
        kMidpointFanPatchSegmentSpan>(contourVertexCount);
    contourVertexCount += contour->paddingVertexCount;
    assert(contourVertexCount % kMidpointFanPatchSegmentSpan == 0);
    RIVE_DEBUG_CODE(contour->tessVertexCount = contourVertexCount;)

    tessVertexCount += contourVertexCount;
    contourFirstLineIdx = contour->endLineIdx;
  }

  assert(contourFirstLineIdx == lineCount);
  RIVE_DEBUG_CODE(m_pendingLineCount = lineCount);
  RIVE_DEBUG_CODE(m_pendingCurveCount = curveIdx);
  RIVE_DEBUG_CODE(m_pendingRotationCount = rotationIdx);
  RIVE_DEBUG_CODE(m_pendingEmptyStrokeCountForCaps = emptyStrokeCountForCaps);

  if (tessVertexCount > 0)
  {
    m_resourceCounts.pathCount = 1;
    m_resourceCounts.contourCount = contourCount;
    // maxTessellatedSegmentCount does not get doubled when we emit both
    // forward and mirrored contours because the forward and mirrored pair
    // both get packed into a single gpu::TessVertexSpan.
    m_resourceCounts.maxTessellatedSegmentCount = lineCount + curveIdx +
                                                  emptyStrokeCountForCaps;
    m_resourceCounts
        .midpointFanTessVertexCount = gpu::ContourDirectionsAreDoubleSided(
                                          m_contourDirections)
                                          ? tessVertexCount * 2
                                          : tessVertexCount;
  }
}

void PathDraw::initForInteriorTriangulation(
    RenderContext* context,
    GrInnerFanTriangulator* triangulator)
{
  RIVE_PROF_SCOPE_L(2)
  PUSH_DISABLE_CLANG_SIMD_ABI_WARNING()
  assert(simd::all(m_resourceCounts.toVec() == 0)); // Only call init() once.
  POP_DISABLE_CLANG_SIMD_ABI_WARNING()
  assert(!isStrokeOrFeather());
  assert(m_strokeRadius == 0);
  assert(m_triangulator == nullptr);
  assert(triangulator != nullptr);

  m_triangulator = triangulator;

  // The triangulator's mesh is transform- and fill-rule-independent (so it
  // can be cached); the fill rule and winding are fed per draw.
  float matrixDeterminant = m_paintMatrix[0] * m_paintMatrix[3] -
                            m_paintMatrix[2] * m_paintMatrix[1];
  m_triangulatorReverseTriangles = matrixDeterminant < 0;
  m_triangulatorNegateWinding = (matrixDeterminant < 0) !=
                                static_cast<bool>(
                                    m_contourFlags &
                                    NEGATE_PATH_FILL_COVERAGE_FLAG);

  // Every path has at least 1 (non-cubic) move.
  size_t originalNumChopsSize = m_pathRef->getRawPath().verbs().size() - 1;
  m_numChops.reset(context->numChopsAllocator(), originalNumChopsSize);
  iterateOuterCubics(nullptr); // Fill in m_resourceCounts.
  m_numChops.shrinkToFit(context->numChopsAllocator(), originalNumChopsSize);
}

bool PathDraw::allocateResources(RenderContext::LogicalFlush* flush)
{
  RIVE_PROF_SCOPE_L(2)
  const RenderContext::FrameDescriptor& frameDesc = flush->frameDescriptor();

  // Allocate a gradient if needed. Do this first since it's more expensive to
  // fail after setting up an atlas draw than a gradient draw.
  if (m_gradientRef != nullptr)
  {
    if (!flush->allocateGradient(
            m_gradientRef,
            &m_simplePaintValue.colorRampLocation))
    {
      return false;
    }
  }

  // Allocate a coverage buffer range or feather atlas region if needed.
  if (m_coverageType == CoverageType::featherAtlas ||
      (m_coverageType == CoverageType::clockwiseAtomic &&
       // Outermost (i.e., non-nested) clockwiseAtomic clips render directly
       // to the clip buffer without using the coverage buffer.
       !isOutermostClipUpdate()))
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

    if (m_coverageType == CoverageType::featherAtlas)
    {
      const float scaleFactor = find_atlas_feather_scale_factor(
          m_featherRadius,
          m_strokeMatrixMaxScale);
      auto w = static_cast<uint16_t>(
          ceilf(visibleBounds.width() * scaleFactor));
      auto h = static_cast<uint16_t>(
          ceilf(visibleBounds.height() * scaleFactor));
      uint16_t x, y;
      if (!flush->allocateFeatherAtlasDraw(
              this,
              w,
              h,
              PADDING,
              &x,
              &y,
              &m_featherAtlasScissor))
      {
        return false; // There wasn't room for our path in the atlas.
      }
      m_featherAtlasTransform.scaleFactor = scaleFactor;
      m_featherAtlasTransform.translateX = x - visibleBounds.left * scaleFactor;
      m_featherAtlasTransform.translateY = y - visibleBounds.top * scaleFactor;
      m_featherAtlasScissorEnabled = visibleBounds != m_pixelBounds;
    }
    else
    {
      // Round up width and height to multiples of BUFFER_IMAGE_TILE_SIZE
      // for tiling.
      uint32_t
          coverageWidth = math::round_up_to_multiple_of<BUFFER_IMAGE_TILE_SIZE>(
              visibleBounds.width() + PADDING * 2);
      uint32_t coverageHeight = math::round_up_to_multiple_of<
          BUFFER_IMAGE_TILE_SIZE>(visibleBounds.height() + PADDING * 2);

      // Get our coverage allocation.
      size_t offset = flush->allocateCoverageBufferRange(
          coverageHeight * coverageWidth);
      if (offset == -1)
      {
        return false; // There wasn't room for our coverage buffer.
      }
      m_coverageBufferRange.offset = math::lossless_numeric_cast<uint32_t>(
          offset);
      m_coverageBufferRange.pitch = coverageWidth;
      m_coverageBufferRange.offsetX = -visibleBounds.left + PADDING;
      m_coverageBufferRange.offsetY = -visibleBounds.top + PADDING;
    }
  }
  return true;
}

void PathDraw::countSubpasses(const gpu::PlatformFeatures& platformFeatures)
{
  RIVE_PROF_SCOPE_L(2)
  m_subpassCount = 1;
  m_prepassCount = 0;

  switch (m_coverageType)
  {
    case CoverageType::featherAtlas:
      assert(m_triangulator == nullptr);
      m_subpassCount = 1;
      break;

    case CoverageType::pixelLocalStorage:
      m_subpassCount = (m_triangulator != nullptr)
                           ? 2 // outer cubics, interior triangles
                           : 1;
      break;

    case CoverageType::clockwise:
      m_subpassCount = (m_triangulator != nullptr)
                           ? 3 // ccw interior tris, outer cubics, cw interior
                               // tris
                           : 1;
      break;

    case CoverageType::clockwiseAtomic:
      m_subpassCount = (m_triangulator != nullptr) ? 2 : 1;
      if (needsBorrowedCoveragePrepass())
      {
        // Add prepasses for borrowed coverage.
        m_prepassCount = m_subpassCount;
      }
      break;

    case CoverageType::depthStencil:
    {
      if (isStroke())
      {
        m_subpassCount = 1; // Strokes can be rendered in a single pass.
      }
      else if (enums::all_flags_set(
                   m_combinedDrawContents,
                   gpu::kNestedClipUpdateMask))
      {
        // Nested clip updates only have a stencil pass. (The reset is
        // handled by a separate ClipReset draw.)
        m_subpassCount = 1;
      }
      else if (enums::is_flag_set(
                   m_combinedDrawContents,
                   gpu::DrawContents::evenOddFill))
      {
        // depthStencil "slow" path: stencil-then-cover.
        m_subpassCount = 2;
      }
      else if (platformFeatures.supportsPipelineDynamicState)
      {
        // depthStencil "fast" path, combined: the three subpasses
        // (borrowed coverage, fans, stencil reset) collapse onto a
        // single stencilDynamicMidpointFans draw that switches between
        // them with dynamic color/depth/stencil/cull state. One batch
        // keeps the reorderer able to instance non-overlapping paths
        // together.
        m_subpassCount = 1;
      }
      else
      {
        // depthStencil "fast" path: (effectively) single pass
        // rendering.
        m_subpassCount = 3;
      }
      if (isOpaque())
      {
        const bool usesClipping = enums::any_flag_set(
            m_combinedDrawContents,
            gpu::DrawContents::activeClip | gpu::DrawContents::clipUpdate);
        if (!usesClipping)
        {
          // Render this path front-to-back instead of back-to-front.
          assert(m_prepassCount == 0);
          m_prepassCount = m_subpassCount;
          m_subpassCount = 0;
        }
      }
      if (m_hasHairlinePass)
      {
        // The hairline is always a subpass (never a prepass because the
        // AA makes it non-opaque).
        ++m_subpassCount;
      }
    }
  }

  const int passCount = m_prepassCount + m_subpassCount;
  assert(passCount <= MaxSubpassCount);
  std::fill_n(m_passDrawContents.begin(), passCount, m_combinedDrawContents);
  if (m_hasHairlinePass)
  {
    // The hairline is never opaque because it has AA.
    m_passDrawContents[passCount - 1] &= ~gpu::DrawContents::opaquePaint;
  }
}

gpu::DrawBatch* PathDraw::pushToRenderContext(
    RenderContext::LogicalFlush* flush,
    int signedSubpassIdx,
    uint32_t zIndex)
{
  RIVE_PROF_SCOPE_L(2)
  // Make sure the rawPath in our path reference hasn't changed since we began
  // holding!
  assert(m_rawPathMutationID == m_pathRef->getRawPathMutationID());
  assert(!m_pathRef->getRawPath().empty());
  assert(!isEmpty());

  assert(
      m_resourceCounts.outerCubicTessVertexCount == 0 ||
      m_resourceCounts.midpointFanTessVertexCount == 0);
  uint32_t tessVertexCount = math::lossless_numeric_cast<uint32_t>(
      m_resourceCounts.outerCubicTessVertexCount |
      m_resourceCounts.midpointFanTessVertexCount);

  const gpu::DrawContents subpassDrawContents = this->subpassDrawContents(
      signedSubpassIdx);

  if (m_pathID == 0)
  {
    // Reserve our pathID and write out a path record.
    m_pathID = flush->pushPath(this, subpassDrawContents, zIndex);
  }

  switch (m_coverageType)
  {
    case CoverageType::pixelLocalStorage:
    case CoverageType::clockwise:
    {
      const int mainSubpassIdx = (m_coverageType == CoverageType::clockwise &&
                                  m_triangulator != nullptr)
                                     ? 1
                                     : 0;
      if (signedSubpassIdx == mainSubpassIdx)
      {
        // Tessellation (midpoint fan or outer cubic).
        uint32_t tessLocation = allocateTessellationVertices(
            flush,
            tessVertexCount);
        pushTessellationData(flush, tessVertexCount, tessLocation);
        return &pushTessellationDraw(flush, tessVertexCount, tessLocation);
      }
      else
      {
        // Interior triangles.
        assert(m_triangulator != nullptr);
        assert(
            (m_coverageType == CoverageType::pixelLocalStorage &&
             signedSubpassIdx == 1) ||
            (m_coverageType == CoverageType::clockwise &&
             (signedSubpassIdx == 0 || signedSubpassIdx == 2)));
        gpu::DrawBatch* batch = flush->pushInteriorTriangulationDraw(
            this,
            subpassDrawContents,
            m_pathID,
            (m_coverageType == CoverageType::clockwise)
                // Clockwise mode renders counterclockwise (borrowed
                // coverage) interior triangles in a separate pass.
                ? (signedSubpassIdx == 0) ? gpu::WindingFaces::negative
                                          : gpu::WindingFaces::positive
                : gpu::WindingFaces::all,
            (signedSubpassIdx == 0) // => CoverageType::clockwise
                ? gpu::ShaderMiscFlags::borrowedCoveragePass
                : gpu::ShaderMiscFlags::none RIVE_DEBUG_CODE(
                      ,
                      &m_numInteriorTriangleVerticesPushed));
        assert(
            m_numInteriorTriangleVerticesPushed <=
            m_triangulator->maxVertexCount(m_pathFillRule));
        return batch;
      }
      RIVE_UNREACHABLE();
    }

    case CoverageType::clockwiseAtomic:
    {
      if (m_prepassCount != 0)
      {
        // The subpass and prepass each emit half the vertices.
        assert(m_prepassCount == m_subpassCount);
        assert(tessVertexCount % 2 == 0);
        assert(needsBorrowedCoveragePrepass());
        tessVertexCount /= 2;
      }
      switch (signedSubpassIdx)
      {
        case -1: // Tessellation (borrowed, midpointFan or outerCubic).
          assert(!isStroke());
          m_prepassTessLocation = allocateTessellationVertices(
              flush,
              tessVertexCount);
          return &pushTessellationDraw(
              flush,
              tessVertexCount,
              m_prepassTessLocation,
              gpu::ShaderMiscFlags::borrowedCoveragePass);

        case 0: // Tessellation (midpointFan or outerCubic).
        {
          uint32_t tessLocation = allocateTessellationVertices(
              flush,
              tessVertexCount);
          pushTessellationData(flush, tessVertexCount, tessLocation);
          return &pushTessellationDraw(flush, tessVertexCount, tessLocation);
        }

        case -2: // Interior triangles (borrowed).
        case 1:  // Interior triangles.
        {
          assert(!isStroke());
          assert(m_triangulator != nullptr);
          gpu::DrawBatch* batch = flush->pushInteriorTriangulationDraw(
              this,
              subpassDrawContents,
              m_pathID,
              m_prepassCount == 0    ? gpu::WindingFaces::all
              : signedSubpassIdx < 0 ? gpu::WindingFaces::negative
                                     : gpu::WindingFaces::positive,
              signedSubpassIdx < 0 ? gpu::ShaderMiscFlags::borrowedCoveragePass
                                   : gpu::ShaderMiscFlags::none RIVE_DEBUG_CODE(
                                         ,
                                         &m_numInteriorTriangleVerticesPushed));
          assert(
              m_numInteriorTriangleVerticesPushed <=
              m_triangulator->maxVertexCount(m_pathFillRule));
          return batch;
        }
      }
      RIVE_UNREACHABLE();
    }

    case CoverageType::depthStencil:
    {
      // The fill passes are either all opaque prepasses or else all
      // subpasses. The hairline adds one extra subpass on top of either.
      assert(m_prepassCount == 0 || m_subpassCount == int(m_hasHairlinePass));
      int fillPassCount = m_prepassCount + m_subpassCount -
                          int(m_hasHairlinePass);
      int passIdx = signedSubpassIdx + m_prepassCount;
      if (passIdx == 0)
      {
        m_depthStencilTessLocation = allocateTessellationVertices(
            flush,
            tessVertexCount);
        pushTessellationData(
            flush,
            tessVertexCount,
            m_depthStencilTessLocation);
      }
      if (m_hasHairlinePass && passIdx == fillPassCount)
      {
        // Hairline pass: unlike polar strokes, hairlines have no polar
        // spokes, no join spokes, and tessellate identically to
        // midpointFans.
        assert(m_triangulator == nullptr);
        return &flush->pushMidpointFanDraw(
            this,
            gpu::DrawType::depthAAOuterHairline,
            subpassDrawContents,
            tessVertexCount,
            m_depthStencilTessLocation);
      }
      assert(1 <= fillPassCount && fillPassCount <= 3);
      assert(passIdx < fillPassCount);
      if (m_triangulator != nullptr)
      {
        // depthStencil interior triangulation: the path interior is
        // filled by smuggling its triangles in with outerCubic patches,
        // rather than introducing a distinct triangle-buffer draw.
        gpu::DrawType outerCubicDrawType;
        if (fillPassCount == 1)
        {
          if (enums::all_flags_set(
                  subpassDrawContents,
                  gpu::kNestedClipUpdateMask))
          {
            outerCubicDrawType = gpu::DrawType::stencilOuterCubicWinding;
          }
          else
          {
            assert(flush->platformFeatures().supportsPipelineDynamicState);
            outerCubicDrawType = gpu::DrawType::stencilDynamicOuterCubics;
          }
        }
        else
        {
          constexpr static gpu::DrawType OuterCubicFillTypes[][3] = {
              // Slow path (fillPassCount == 2): stencil-then-cover.
              {
                  gpu::DrawType::stencilOuterCubicWinding,
                  gpu::DrawType::stencilOuterCubicCover,
              },
              // Fast path (fillPassCount == 3).
              {
                  gpu::DrawType::stencilOuterCubicBorrowedCoverage,
                  gpu::DrawType::stencilOuterCubics,
                  gpu::DrawType::stencilOuterCubicReset,
              },
          };
          outerCubicDrawType = OuterCubicFillTypes[fillPassCount - 2][passIdx];
        }
        return &flush->pushOuterCubicsDraw(
            this,
            outerCubicDrawType,
            subpassDrawContents,
            tessVertexCount,
            m_depthStencilTessLocation);
      }
      else
      {
        gpu::DrawType depthStencilDrawType;
        if (fillPassCount == 1)
        {
          if (isStroke())
          {
            depthStencilDrawType = flush->frameDescriptor().inkbleedOverride
                                       ? gpu::DrawType::depthAAStrokes
                                       : gpu::DrawType::depthStrokes;
          }
          else if (enums::all_flags_set(
                       subpassDrawContents,
                       gpu::kNestedClipUpdateMask))
          {
            depthStencilDrawType = gpu::DrawType::stencilMidpointFanWinding;
          }
          else
          {
            assert(flush->platformFeatures().supportsPipelineDynamicState);
            depthStencilDrawType = gpu::DrawType::stencilDynamicMidpointFans;
          }
        }
        else
        {
          constexpr static gpu::DrawType MidpointFanFillTypes[][3] = {
              // Slow path (fillPassCount == 2): stencil-then-cover
              {
                  gpu::DrawType::stencilMidpointFanWinding,
                  gpu::DrawType::stencilMidpointFanCover,
              },

              // Fast path (fillPassCount == 3): (mostly) single pass
              // rendering.
              {
                  gpu::DrawType::stencilMidpointFanBorrowedCoverage,
                  gpu::DrawType::stencilMidpointFans,
                  gpu::DrawType::stencilMidpointFanReset,
              },
          };
          depthStencilDrawType = MidpointFanFillTypes[fillPassCount - 2]
                                                     [passIdx];
        }
        return &flush->pushMidpointFanDraw(
            this,
            depthStencilDrawType,
            subpassDrawContents,
            tessVertexCount,
            m_depthStencilTessLocation);
      }
    }

    case CoverageType::featherAtlas:
      // Atlas draws only have one subpass -- the rectangular blit from
      // the atlas to the screen. The step that renders coverage to the
      // offscreen atlas is handled separately, outside the subpass
      // system.
      assert(signedSubpassIdx == 0);
      return &flush->pushFeatherAtlasBlit(this, subpassDrawContents, m_pathID);
  }

  RIVE_UNREACHABLE();
}

gpu::DrawBatch& PathDraw::pushTessellationDraw(
    RenderContext::LogicalFlush* flush,
    uint32_t tessVertexCount,
    uint32_t tessLocation,
    gpu::ShaderMiscFlags shaderMiscFlags)
{
  if (m_triangulator != nullptr)
  {
    assert(!isStroke());
    return flush->pushOuterCubicsDraw(
        this,
        gpu::DrawType::outerCurvePatches,
        m_combinedDrawContents,
        tessVertexCount,
        tessLocation,
        shaderMiscFlags);
  }
  else
  {
    return flush->pushMidpointFanDraw(
        this,
        isFeatheredFill() ? gpu::DrawType::midpointFanCenterAAPatches
                          : gpu::DrawType::midpointFanPatches,
        m_combinedDrawContents,
        tessVertexCount,
        tessLocation,
        shaderMiscFlags);
  }
}

void PathDraw::pushFeatherAtlasTessellation(
    RenderContext::LogicalFlush* flush,
    uint32_t* tessVertexCount,
    uint32_t* tessBaseVertex)
{
  RIVE_PROF_SCOPE_L(2)
  assert(m_coverageType == CoverageType::featherAtlas);
  assert(!isEmpty());
  assert(
      m_resourceCounts.outerCubicTessVertexCount == 0 ||
      m_resourceCounts.midpointFanTessVertexCount == 0);

  *tessVertexCount = math::lossless_numeric_cast<uint32_t>(
      m_resourceCounts.outerCubicTessVertexCount |
      m_resourceCounts.midpointFanTessVertexCount);

  *tessBaseVertex = allocateTessellationVertices(flush, *tessVertexCount);
  pushTessellationData(flush, *tessVertexCount, *tessBaseVertex);
}

void PathDraw::pushTessellationData(
    RenderContext::LogicalFlush* flush,
    uint32_t tessVertexCount,
    uint32_t tessLocation)
{
  RIVE_PROF_SCOPE_L(2)
  // Determine where to fill in forward and mirrored tessellations.
  uint32_t forwardTessVertexCount, forwardTessLocation, mirroredTessVertexCount,
      mirroredTessLocation;
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
      if (m_coverageType == CoverageType::clockwiseAtomic &&
          m_prepassTessLocation != 0) // With padding, this will only be
                                      // zero if it's not needed.
      {
        // The tessellation for borrowed coverage was allocated at a
        // different location than the forward tessellation, both with
        // "tessVertexCount" vertices.
        assert(needsBorrowedCoveragePrepass());
        forwardTessVertexCount = mirroredTessVertexCount = tessVertexCount;
        forwardTessLocation = tessLocation;
        mirroredTessLocation = m_prepassTessLocation + tessVertexCount;
      }
      else
      {
        // The reverse and forward tessellations are allocated
        // contiguously, with a combined vertex count of
        // "tessVertexCount". (tessVertexCount/2 vertices each.)
        assert(tessVertexCount % 2 == 0);
        forwardTessVertexCount = mirroredTessVertexCount = tessVertexCount / 2;
        forwardTessLocation = mirroredTessLocation = tessLocation +
                                                     tessVertexCount / 2;
      }
      break;
    case gpu::ContourDirections::forwardThenReverse:
      if (m_coverageType == CoverageType::clockwiseAtomic &&
          m_prepassTessLocation != 0) // With padding, this will only be
                                      // zero if it's not needed.
      {
        // The tessellation for borrowed coverage was allocated at a
        // different location than the forward tessellation, both with
        // "tessVertexCount" vertices.
        assert(needsBorrowedCoveragePrepass());
        forwardTessVertexCount = mirroredTessVertexCount = tessVertexCount;
        forwardTessLocation = m_prepassTessLocation;
        mirroredTessLocation = tessLocation + tessVertexCount;
      }
      else
      {
        // The reverse and forward tessellations are allocated
        // contiguously, with a combined vertex count of
        // "tessVertexCount". (tessVertexCount/2 vertices each.)
        assert(tessVertexCount % 2 == 0);
        forwardTessVertexCount = mirroredTessVertexCount = tessVertexCount / 2;
        forwardTessLocation = tessLocation;
        mirroredTessLocation = tessLocation + tessVertexCount;
      }
      break;
  }

  // Write out the TessVertexSpans and path contours.
  RenderContext::TessellationWriter tessWriter(
      flush,
      m_pathID,
      m_contourDirections,
      forwardTessVertexCount,
      forwardTessLocation,
      mirroredTessVertexCount,
      mirroredTessLocation);

  if (m_triangulator != nullptr)
  {
    iterateOuterCubics(&tessWriter);
  }
  else
  {
    pushMidpointFanTessellationData(&tessWriter);
  }
}

void PathDraw::pushMidpointFanTessellationData(
    RenderContext::TessellationWriter* tessWriter)
{
  RIVE_PROF_SCOPE_L(3)
  const RawPath& rawPath = m_pathRef->getRawPath();
  RawPath::Iter startOfContour = rawPath.begin();
  // The second and later pieces of chopped cubics, and their cusp joins, come
  // after all the path's other curves and rotations, in order.
  size_t extraCurveIdx = m_firstExtraCurveIdx;
  size_t extraRotationIdx = m_firstExtraRotationIdx;
  for (size_t i = 0; i < m_resourceCounts.contourCount; ++i)
  {
    // Push a contour and curve records.
    const ContourInfo& contour = m_contours[i];
    assert(startOfContour.verb() == PathVerb::move);
    assert(isStroke() || contour.closed); // Fills are always closed.
    RIVE_DEBUG_CODE(m_pendingStrokeJoinCount = isStrokeOrFeather()
                                                   ? contour.strokeJoinCount
                                                   : 0;)
    RIVE_DEBUG_CODE(
        m_pendingStrokeCapCount = contour.strokeCapSegmentCount != 0 ? 2 : 0;)

    const Vec2D* pts = startOfContour.rawPtsPtr();
    size_t curveIdx = contour.firstCurveIdx;
    size_t rotationIdx = contour.firstRotationIdx;
    RIVE_DEBUG_CODE(size_t contourFirstExtraCurveIdx = extraCurveIdx;)
    RIVE_DEBUG_CODE(size_t contourFirstExtraRotationIdx = extraRotationIdx;)
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
        StrokeCap cap = !contour.closed
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
    uint32_t contourIDWithFlags = m_contourFlags |
                                  tessWriter->pushContour(
                                      contour.midpoint,
                                      isStroke(),
                                      contour.closed,
                                      contour.paddingVertexCount);

    // When we don't have round joins, the number of segments per join is
    // constant. (Round joins have a variable number of segments per join,
    // depending on the angle.)
    uint32_t fixedJoinSegmentCount = 0;
    if (isFeatheredFill())
    {
      fixedJoinSegmentCount = feather_join_segment_count(
          m_polarSegmentsPerRadian);
    }
    else if (isStroke() && !roundJoinStroked)
    {
      fixedJoinSegmentCount = numSegmentsInFixedJoin(
          m_coverageType,
          m_strokeJoin);
    }

    // Convert all curves in the contour to cubics and push them to the GPU.
    const int styleFlags = style_flags(isStrokeOrFeather(), roundJoinStroked);
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
            joinTangent = find_join_tangent(
                iter.linePts() + 1,
                end.rawPtsPtr(),
                contour.closed,
                pts);
            joinSegmentCount = fixedJoinSegmentCount;
            RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
          }
          else
          {
            // End with a 180-degree join that looks like the stroke
            // cap.
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
            // Emulate the start cap as a 180-degree join before the
            // first stroke.
            pushEmulatedStrokeCapAsJoinBeforeCubic(
                tessWriter,
                cubic.data(),
                contour.strokeCapSegmentCount,
                contourIDWithFlags | emulatedCapAsJoinFlags);
            needsFirstEmulatedCapAsJoin = false;
          }
          tessWriter->pushCubic(
              cubic.data(),
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
          uint8_t numChops = chopKey >> 1;
          ChopBuffer localChopBuffer;
          if (numChops != 0)
          {
            auto [t0, t1] = m_chopVertices.pop_front();
            const float t[2] = {t0, t1};
            chopCubic(p, chopKey, t, &localChopBuffer);
            p = localChopBuffer.data();
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
          // Push chops before the final one. The first piece is in
          // the cubic's own slots.
          const bool areChopsCusps = (chopKey & 1) != 0;
          size_t pieceCurveIdx = curveIdx++;
          size_t pieceRotationIdx = rotationIdx++;
          for (uint8_t piece = 0; piece < numChops; ++piece, p += 3)
          {
            uint32_t parametricSegmentCount = m_parametricSegmentCounts
                [pieceCurveIdx];
            uint32_t polarSegmentCount = m_polarSegmentCounts[pieceRotationIdx];
            RIVE_DEBUG_CODE(--m_pendingRotationCount;)

            Vec2D chopJoinTangent;
            uint32_t chopJoinSegmentCount;
            uint32_t chopJoinTypeFlags;
            if (areChopsCusps)
            {
              // A cusp turns 180 degrees which gets drawn with a
              // cusp join.
              size_t cuspJoinIdx = extraRotationIdx++;
              chopJoinTangent = m_tangentPairs[cuspJoinIdx][1];
              chopJoinSegmentCount = m_polarSegmentCounts[cuspJoinIdx];
              chopJoinTypeFlags = CUSP_JOIN_CONTOUR_FLAG;
              RIVE_DEBUG_CODE(--m_pendingRotationCount;)
            }
            else
            {
              // A chop at an inflection or at 180 degrees
              // continues smoothly, so it needs no join.
              chopJoinTangent = joinTangent;
              chopJoinSegmentCount = 1;
              chopJoinTypeFlags = joinTypeFlags;
            }

            tessWriter->pushCubic(
                p,
                m_contourDirections,
                chopJoinTangent,
                parametricSegmentCount,
                polarSegmentCount,
                chopJoinSegmentCount,
                contourIDWithFlags | chopJoinTypeFlags);
            RIVE_DEBUG_CODE(--m_pendingCurveCount;)
            pieceCurveIdx = extraCurveIdx++;
            pieceRotationIdx = extraRotationIdx++;
          }
          // Push the final chop, with a join.
          uint32_t
              parametricSegmentCount = m_parametricSegmentCounts[pieceCurveIdx];
          uint32_t polarSegmentCount = m_polarSegmentCounts[pieceRotationIdx];
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
              joinTangent = find_join_tangent(
                  iter.cubicPts() + 3,
                  end.rawPtsPtr(),
                  contour.closed,
                  pts);
              joinSegmentCount = fixedJoinSegmentCount;
            }
            RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
          }
          else
          {
            // End with a 180-degree join that looks like the stroke
            // cap.
            joinTangent = -find_ending_tangent(pts, end.rawPtsPtr());
            joinTypeFlags = emulatedCapAsJoinFlags;
            joinSegmentCount = contour.strokeCapSegmentCount;
            RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
          }
          tessWriter->pushCubic(
              p,
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
          uint32_t
              parametricSegmentCount = m_parametricSegmentCounts[curveIdx++];
          tessWriter->pushCubic(
              iter.cubicPts(),
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
      Vec2D p0 = pts[0], left = {p0.x - 1, p0.y}, right = {p0.x + 1, p0.y};
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
        else if (isStrokeOrFeather())
        {
          joinTangent = find_starting_tangent(pts, end.rawPtsPtr());
          joinSegmentCount = fixedJoinSegmentCount;
          RIVE_DEBUG_CODE(--m_pendingStrokeJoinCount;)
        }
        tessWriter->pushCubic(
            cubic.data(),
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
    assert(
        extraCurveIdx - contourFirstExtraCurveIdx == contour.extraCurveCount);
    assert(
        extraRotationIdx - contourFirstExtraRotationIdx ==
        contour.extraRotationCount);

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
  RIVE_PROF_SCOPE_L(3)
  // Reverse the cubic and push it with zero parametric and polar segments,
  // and a 180-degree join tangent. This results in a solitary join,
  // positioned immediately before the provided cubic, that looks like the
  // desired stroke cap.
  assert(strokeCapSegmentCount >= 2);
  tessWriter->pushCubic(
      std::array{cubic[3], cubic[2], cubic[1], cubic[0]}.data(),
      m_contourDirections,
      math::findCubicTan0(cubic),
      0,
      0,
      strokeCapSegmentCount,
      contourIDWithFlags);
  RIVE_DEBUG_CODE(--m_pendingStrokeCapCount;)
  RIVE_DEBUG_CODE(--m_pendingEmptyStrokeCountForCaps;)
}

void PathDraw::iterateOuterCubics(RenderContext::TessellationWriter* tessWriter)
{
  RIVE_PROF_SCOPE_L(2)
  Vec2D chops[kMaxCurveSubdivisions * 3 + 1];
  const RawPath& rawPath = m_pathRef->getRawPath();
  assert(!rawPath.empty());
  wangs_formula::VectorXform vectorXform(m_paintMatrix);
  size_t patchCount = 0;
  size_t contourCount = 0;
  Vec2D p0 = {0, 0};
  // A straight edge produces a single-segment cubic patch. In depthStencil we
  // draw only the patch interior (not the AA border), and that interior is
  // degenerate for a single segment, so we skip these patches entirely.
  const bool skipFlatOuterCubics = m_coverageType == CoverageType::depthStencil;
  // Only used on the "emit" pass (tessWriter != nullptr).
  uint32_t contourIDWithFlags = 0;
  for (const auto [verb, pts] : rawPath)
  {
    switch (verb)
    {
      case PathVerb::move:
        if (contourCount != 0 && pts[-1] != p0 && !skipFlatOuterCubics)
        {
          if (tessWriter != nullptr)
          {
            tessWriter->pushCubic(
                convert_line_to_cubic(pts[-1], p0).data(),
                m_contourDirections,
                {0, 0},
                OuterCubicPatchSegmentSpan,
                1,
                /*bowtieSegmentCount=*/1,
                contourIDWithFlags |
                    CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
          }
          ++patchCount;
        }
        if (tessWriter != nullptr)
        {
          contourIDWithFlags = m_contourFlags | tessWriter->pushContour(
                                                    {0, 0},
                                                    isStroke(),
                                                    /*closed=*/true,
                                                    0);
        }
        p0 = pts[0];
        ++contourCount;
        break;
      case PathVerb::line:
        if (!skipFlatOuterCubics)
        {
          if (tessWriter != nullptr)
          {
            tessWriter->pushCubic(
                convert_line_to_cubic(pts).data(),
                m_contourDirections,
                {0, 0},
                OuterCubicPatchSegmentSpan,
                1,
                /*bowtieSegmentCount=*/1,
                contourIDWithFlags |
                    CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
          }
          ++patchCount;
        }
        break;
      case PathVerb::quad:
        RIVE_UNREACHABLE();
      case PathVerb::cubic:
      {
        uint32_t numSubdivisions;
        if (tessWriter == nullptr)
        {
          numSubdivisions = find_outer_cubic_subdivision_count(
              pts,
              vectorXform);
          m_numChops.push_back(numSubdivisions);
        }
        else
        {
          numSubdivisions = m_numChops.pop_front();
        }
        assert(numSubdivisions >= 1);

        const size_t numGlueTriangles = numSubdivisions - 1;
        const size_t numGlueTriStrips = (numGlueTriangles + 2) / 3;
        const size_t numPatches = numSubdivisions + numGlueTriStrips;
        patchCount += numPatches;

        if (tessWriter != nullptr)
        {
          if (numSubdivisions == 1)
          {
            tessWriter->pushCubic(
                pts,
                m_contourDirections,
                {0, 0},
                OuterCubicPatchSegmentSpan,
                1,
                /*bowtieSegmentCount=*/1,
                contourIDWithFlags |
                    CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
          }
          else
          {
            RIVE_DEBUG_CODE(size_t pushedPatchCount = 0;)
            // Passing nullptr for the 'tValues' causes it to chop
            // the cubic uniformly in T.
            math::chopCubicAt(pts, chops, nullptr, numSubdivisions - 1);
            const Vec2D* chop = chops;
            for (size_t i = 0; i < numSubdivisions; ++i)
            {
              tessWriter->pushCubic(
                  chop,
                  m_contourDirections,
                  {0, 0},
                  OuterCubicPatchSegmentSpan,
                  1,
                  /*bowtieSegmentCount=*/1,
                  contourIDWithFlags |
                      CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
              chop += 3;
              RIVE_DEBUG_CODE(++pushedPatchCount;)
            }
            // Fill the gap between the sub-cubics by connecting
            // their endpoints with a simple fan. Each fan blade is
            // cubic patch with RETROFIT_TRI_STRIP_CONTOUR_FLAG (a
            // standard strip of up to 3 triangles, retrofitted into
            // a cubic patch), anchored at the first vertex
            // (chops[0]).
            for (size_t a = 1; a < numSubdivisions; a += 3)
            {
              const auto segmentsRemaining = numSubdivisions - a;
              // Flip the triangle strip orthogonally to the fan
              // blade in order to generate more efficient (less
              // "sliver") triangles.
              const size_t b = segmentsRemaining == 1 ? a : a + 1;
              StackVector<Vec2D, 5> strip;
              strip.push_back(chops[b * 3]);
              strip.push_back(chops[(b + 1) * 3]);
              if (segmentsRemaining > 1)
                strip.push_back(chops[(b - 1) * 3]);
              if (segmentsRemaining > 2)
                strip.push_back(chops[(b + 2) * 3]);
              strip.push_back(chops[0]);
              tessWriter->pushRetrofitCubicTriStrip(
                  strip.data(),
                  strip.size(),
                  m_contourDirections,
                  contourIDWithFlags);
              RIVE_DEBUG_CODE(++pushedPatchCount;)
            }
            assert(pushedPatchCount == numPatches);
          }
        }
        break;
      }
      case PathVerb::close:
        break;
    }
  }
  Vec2D lastPt = rawPath.points().back();
  if (contourCount != 0 && lastPt != p0 && !skipFlatOuterCubics)
  {
    if (tessWriter != nullptr)
    {
      tessWriter->pushCubic(
          convert_line_to_cubic(lastPt, p0).data(),
          m_contourDirections,
          {0, 0},
          OuterCubicPatchSegmentSpan,
          1,
          /*bowtieSegmentCount=*/1,
          contourIDWithFlags | CULL_EXCESS_TESSELLATION_SEGMENTS_CONTOUR_FLAG);
    }
    ++patchCount;
  }

  if (tessWriter == nullptr)
  {
    // We also draw each "grout" triangle using an outerCubic patch.
    patchCount += m_triangulator->groutList().count();

    if (m_coverageType == CoverageType::depthStencil)
    {
      // depthStencil fills the path interior by smuggling its triangles
      // in with outerCubic patches (rather than a distinct
      // triangle-buffer draw).
      patchCount += m_triangulator->retrofitCubicPatchCount(m_pathFillRule);
    }

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
              ? patchCount * OuterCubicPatchSegmentSpanPlusBowtie * 2
              : patchCount * OuterCubicPatchSegmentSpanPlusBowtie;
      if (m_coverageType != CoverageType::depthStencil)
      {
        // In depthStencil, the interior triangles are smuggled in with
        // outerCubic patches (counted above) instead of being drawn
        // from the triangle buffer.
        m_resourceCounts.maxTriangleVertexCount += m_triangulator
                                                       ->maxVertexCount(
                                                           m_pathFillRule);
      }
    }
  }
  else
  {
    // Submit grout triangles, retrofitted into outerCubic patches.
    for (auto* node = m_triangulator->groutList().head(); node;
         node = node->fNext)
    {
      tessWriter->pushRetrofitCubicTriStrip(
          node->fPts,
          3,
          m_contourDirections,
          contourIDWithFlags);
      ++patchCount;
    }
    if (m_coverageType == CoverageType::depthStencil)
    {
      // Smuggle the interior triangles in with outerCubic patches.
      patchCount += m_triangulator->polysToRetrofitCubicPatches(
          m_pathFillRule,
          gpu::WindingFaces::all,
          [&](const Vec2D* strip, size_t cornerCount) {
            tessWriter->pushRetrofitCubicTriStrip(
                strip,
                cornerCount,
                m_contourDirections,
                contourIDWithFlags);
          });
    }
    assert(contourCount == m_resourceCounts.contourCount);
    assert(patchCount == m_resourceCounts.maxTessellatedSegmentCount);
    assert(
        patchCount * OuterCubicPatchSegmentSpanPlusBowtie * 2 ==
            m_resourceCounts.outerCubicTessVertexCount ||
        patchCount * OuterCubicPatchSegmentSpanPlusBowtie ==
            m_resourceCounts.outerCubicTessVertexCount);
  }
}

ImageRectDraw::ImageRectDraw(
    RenderContext* context,
    IAABB pixelBounds,
    const Mat2D& matrix,
    BlendMode blendMode,
    float additiveness,
    rcp<Texture> imageTexture,
    rcp<const Gradient> gradient,
    const ImageSampler imageSampler,
    ColorInt modulatedColor,
    const Mat2D& imageMatrix,
    const Mat2D& gradientMatrix,
    const Mat2D& inverseGradientTransform)
    : Draw(
          pixelBounds,
          matrix,
          &imageMatrix,
          blendMode,
          additiveness,
          std::move(imageTexture),
          imageSampler,
          Type::imageRect)
    , m_modulatedColor(modulatedColor)
    , m_gradientMatrix(gradientMatrix)
    , m_inverseGradientTransform(inverseGradientTransform)
    , m_gradientRef(gradient.release())
{
  // If we support image paints for paths, the client should draw a
  // rectangular path with an image paint instead of using this draw.
  assert(!context->frameSupportsImagePaintForPaths());
  m_resourceCounts.imageRectCount = 1;
}

bool ImageRectDraw::allocateResources(RenderContext::LogicalFlush* flush)
{
  if (!Draw::allocateResources(flush))
  {
    return false;
  }

  if (m_gradientRef != nullptr)
  {
    if (!flush->allocateGradient(m_gradientRef, &m_rampLocation))
    {
      return false;
    }
  }

  return true;
}

void ImageRectDraw::releaseRefs()
{
  Draw::releaseRefs();
  safe_unref(m_gradientRef);
}

gpu::DrawBatch* ImageRectDraw::pushToRenderContext(
    RenderContext::LogicalFlush* flush,
    int signedSubpassIdx,
    uint32_t zIndex)
{
  assert(signedSubpassIdx == 0);
  return &flush->pushImageRectDraw(this, subpassDrawContents(0), zIndex);
}

ImageMeshDrawBase::ImageMeshDrawBase(
    IAABB pixelBounds,
    const Mat2D& matrix,
    BlendMode blendMode,
    float additiveness,
    rcp<Texture> imageTexture,
    const ImageSampler imageSampler,
    rcp<RenderBuffer> vertexBuffer,
    rcp<RenderBuffer> uvBuffer,
    rcp<RenderBuffer> indexBuffer,
    uint32_t indexCount)
    : Draw(
          pixelBounds,
          matrix,
          nullptr,
          blendMode,
          additiveness,
          std::move(imageTexture),
          imageSampler,
          Type::imageMesh)
    , m_vertexBufferRef(vertexBuffer.release())
    , m_uvBufferRef(uvBuffer.release())
    , m_indexBufferRef(indexBuffer.release())
    , m_indexCount(indexCount)
{
  assert(m_vertexBufferRef != nullptr);
  assert(m_uvBufferRef != nullptr);
  assert(m_indexBufferRef != nullptr);
}

void ImageMeshDrawBase::releaseRefs()
{
  Draw::releaseRefs();
  m_vertexBufferRef->unref();
  m_uvBufferRef->unref();
  m_indexBufferRef->unref();
}

ImageMeshDraw::ImageMeshDraw(
    IAABB pixelBounds,
    const Mat2D& matrix,
    BlendMode blendMode,
    float additiveness,
    rcp<Texture> imageTexture,
    const ImageSampler imageSampler,
    rcp<RenderBuffer> vertexBuffer,
    rcp<RenderBuffer> uvBuffer,
    rcp<RenderBuffer> indexBuffer,
    uint32_t indexCount,
    ColorInt modulatedColor,
    const Vec2D& uvTranslate,
    const Vec2D& uvScale)
    : ImageMeshDrawBase(
          pixelBounds,
          matrix,
          blendMode,
          additiveness,
          std::move(imageTexture),
          imageSampler,
          std::move(vertexBuffer),
          std::move(uvBuffer),
          std::move(indexBuffer),
          indexCount)
    , m_modulatedColor(modulatedColor)
    , m_uvTranslate(uvTranslate)
    , m_uvScale(uvScale)
{
  m_resourceCounts.imageMeshCount = 1;
}

gpu::DrawBatch* ImageMeshDraw::pushToRenderContext(
    RenderContext::LogicalFlush* flush,
    int signedSubpassIdx,
    uint32_t zIndex)
{
  assert(signedSubpassIdx == 0);
  return &flush->pushImageMeshDraw(this, subpassDrawContents(0), zIndex);
}

ImageMeshInstancedDraw::ImageMeshInstancedDraw(
    IAABB pixelBounds,
    const Mat2D& matrix,
    rcp<Texture> imageTexture,
    const ImageSampler imageSampler,
    rcp<RenderBuffer> vertexBuffer,
    rcp<RenderBuffer> uvBuffer,
    rcp<RenderBuffer> indexBuffer,
    uint32_t indexCount,
    rcp<ImageMeshInstances> instances,
    ColorInt modulatedColor,
    float modulatedOpacity)
    : ImageMeshDrawBase(
          pixelBounds,
          matrix,
          BlendMode::srcOver,
          0.0f, // Additiveness is per instance here.
          std::move(imageTexture),
          imageSampler,
          std::move(vertexBuffer),
          std::move(uvBuffer),
          std::move(indexBuffer),
          indexCount)
    , m_instancesRef(instances.release())
    , m_modulatedColor(modulatedColor)
    , m_modulatedOpacity(modulatedOpacity)
{
  assert(m_instancesRef != nullptr);
  assert(m_instancesRef->count() > 0);
  assert(!m_instancesRef->isEditing());
  m_resourceCounts.imageMeshCount = m_instancesRef->count();
  RIVE_DEBUG_CODE(m_editCount = m_instancesRef->editCount();)
}

gpu::DrawBatch* ImageMeshInstancedDraw::pushToRenderContext(
    RenderContext::LogicalFlush* flush,
    int signedSubpassIdx,
    uint32_t zIndex)
{
  assert(signedSubpassIdx == 0);
  return &flush->pushImageMeshInstancedDraw(
      this,
      subpassDrawContents(0),
      zIndex);
}

void ImageMeshInstancedDraw::releaseRefs()
{
  ImageMeshDrawBase::releaseRefs();
  m_instancesRef->unref();
}

ClipReset::ClipReset(
    RenderContext* context,
    uint32_t previousClipID,
    gpu::DrawContents previousClipDrawContents,
    ResetAction resetAction)
    : Draw(
          context->getClipContentBounds(previousClipID),
          Mat2D{},
          nullptr,
          BlendMode::srcOver,
          /*additiveness =*/0,
          nullptr,
          ImageSampler::LinearClamp(),
          Type::stencilClipReset)
    , m_previousClipID(previousClipID)
{
  constexpr static gpu::DrawContents
      FILL_RULE_FLAGS = gpu::DrawContents::nonZeroFill |
                        gpu::DrawContents::evenOddFill |
                        gpu::DrawContents::clockwiseFill;
  m_combinedDrawContents |= previousClipDrawContents & FILL_RULE_FLAGS;
  switch (resetAction)
  {
    case ResetAction::intersectPreviousClip:
      m_combinedDrawContents |= gpu::DrawContents::activeClip;
      [[fallthrough]];
    case ResetAction::clearPreviousClip:
      m_combinedDrawContents |= gpu::DrawContents::clipUpdate;
      break;
  }
  m_resourceCounts.maxTriangleVertexCount = 6;
}

gpu::DrawBatch* ClipReset::pushToRenderContext(
    RenderContext::LogicalFlush* flush,
    int signedSubpassIdx,
    uint32_t zIndex)
{
  assert(signedSubpassIdx == 0);
  return &flush->pushClipResetDraw(this, subpassDrawContents(0), zIndex);
}
} // namespace rive::gpu
