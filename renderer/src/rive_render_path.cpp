/*
 * Copyright 2022 Rive
 */

#include "rive_render_path.hpp"

#include "rive/math/bezier_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/math/wangs_formula.hpp"
#include "shaders/constants.glsl"

namespace rive
{

RiveRenderPath::RiveRenderPath(FillRule fillRule, RawPath& rawPath)
{
    m_fillRule = fillRule;
    m_rawPath.swap(rawPath);
    m_rawPath.pruneEmptySegments();
}

void RiveRenderPath::rewind()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.rewind();
    m_dirt = kAllDirt;
}

void RiveRenderPath::moveTo(float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.moveTo(x, y);
    m_dirt = kAllDirt;
}

void RiveRenderPath::lineTo(float x, float y)
{
    assert(m_rawPathMutationLockCount == 0);

    // Make sure to start a new contour, even if this line is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {x, y};
    if (m_rawPath.points().back() != p1)
    {
        m_rawPath.line(p1);
    }

    m_dirt = kAllDirt;
}

void RiveRenderPath::cubicTo(float ox,
                             float oy,
                             float ix,
                             float iy,
                             float x,
                             float y)
{
    assert(m_rawPathMutationLockCount == 0);

    // Make sure to start a new contour, even if this cubic is empty.
    m_rawPath.injectImplicitMoveIfNeeded();

    Vec2D p1 = {ox, oy};
    Vec2D p2 = {ix, iy};
    Vec2D p3 = {x, y};
    if (m_rawPath.points().back() != p1 || p1 != p2 || p2 != p3)
    {
        m_rawPath.cubic(p1, p2, p3);
    }

    m_dirt = kAllDirt;
}

void RiveRenderPath::close()
{
    assert(m_rawPathMutationLockCount == 0);
    m_rawPath.close();
    m_dirt = kAllDirt;
}

void RiveRenderPath::addRenderPath(RenderPath* path, const Mat2D& matrix)
{
    assert(m_rawPathMutationLockCount == 0);
    RiveRenderPath* riveRenderPath = static_cast<RiveRenderPath*>(path);
    RawPath::Iter transformedPathIter =
        m_rawPath.addPath(riveRenderPath->m_rawPath, &matrix);
    if (matrix != Mat2D())
    {
        // Prune any segments that became empty after the transform.
        m_rawPath.pruneEmptySegments(transformedPathIter);
    }
    m_dirt = kAllDirt;
}

void RiveRenderPath::addRenderPathBackwards(RenderPath* path,
                                            const Mat2D& transform)
{
    RiveRenderPath* riveRenderPath = static_cast<RiveRenderPath*>(path);
    RawPath::Iter transformedPathIter =
        m_rawPath.addPathBackwards(riveRenderPath->m_rawPath, &transform);
    if (transform != Mat2D())
    {
        // Prune any segments that became empty after the transform.
        m_rawPath.pruneEmptySegments(transformedPathIter);
    }
    m_dirt = kAllDirt;
}

const AABB& RiveRenderPath::getBounds() const
{
    if (m_dirt & kPathBoundsDirt)
    {
        m_bounds = m_rawPath.bounds();
        m_dirt &= ~kPathBoundsDirt;
    }
    return m_bounds;
}

float RiveRenderPath::getCoarseArea() const
{
    if (m_dirt & kPathCoarseAreaDirt)
    {
        m_coarseArea = m_rawPath.computeCoarseArea();
        m_dirt &= ~kPathCoarseAreaDirt;
    }
    return m_coarseArea;
}

bool RiveRenderPath::isClockwiseDominant(const Mat2D& viewMatrix) const
{
    float matrixDeterminant =
        viewMatrix[0] * viewMatrix[3] - viewMatrix[2] * viewMatrix[1];
    return getCoarseArea() * matrixDeterminant >= 0;
}

uint64_t RiveRenderPath::getRawPathMutationID() const
{
    static std::atomic<uint64_t> uniqueIDCounter = 0;
    if (m_dirt & kRawPathMutationIDDirt)
    {
        m_rawPathMutationID = ++uniqueIDCounter;
        m_dirt &= ~kRawPathMutationIDDirt;
    }
    return m_rawPathMutationID;
}

// When a blurred shape curves away from the convolution matrix, the curvature
// makes the blur softer, which does not happen naturally in feathering.
//
// To simulate the softening effect from curving away, we flatten curves
// proportionaly to curvature. This works really well for gaussian feathers, but
// we may also split the curve and recurse if there is enough flattening to
// become noticeable.
//
// TODO: Move this work to the GPU.
static void add_softened_cubic_for_feathering(RawPath* featheredPath,
                                              const Vec2D p[4],
                                              float feather,
                                              float matrixMaxScale,
                                              int maxDepth = 3)
{
    float2 p0 = simd::load2f(p), p1 = simd::load2f(p + 1),
           p2 = simd::load2f(p + 2), p3 = simd::load2f(p + 3);
    math::CubicCoeffs coeffs(p);

    // Find the point of maximum height on the cubic.
    float maxHeightT;
    float height = math::find_cubic_max_height(p, &maxHeightT);

    // Measure curvature across one standard deviation of the feather.
    // ("feather" is 2 std devs.)
    float desiredSpread = feather * .5f;

    // The feather gets dimmer with curvature. Find a dimming factor based on
    // the strength of curvature at maximum height.
    float theta = math::measure_cubic_local_curvature(p,
                                                      coeffs,
                                                      maxHeightT,
                                                      desiredSpread);
    float dimming = 1 - theta * (1 / math::PI);

    // Always dim a little bit in order to avoid artifacts on tight cusps.
    // FIXME: This is unfortunate. There must be a better way to handle cusps.
    dimming = fminf(dimming, .925f);

    // Find a new height such that the center of the feather (currently 50%
    // opacity) is reduced to "50% * dimming".
    float desiredOpacityOnCenter = .5f * dimming;
    float x = gpu::inverse_gaussian_integral(desiredOpacityOnCenter) - .5f;
    float newHeight = height + feather * FEATHER_TEXTURE_STDDEVS * x;

    if (maxDepth > 0 && (height - newHeight) * matrixMaxScale > 8)
    {
        // The curve would be flattened too much. Chop at max height and
        // recurse.
        Vec2D pp[7];
        math::chop_cubic_at(p, pp, maxHeightT);
        add_softened_cubic_for_feathering(featheredPath,
                                          pp,
                                          feather,
                                          matrixMaxScale,
                                          maxDepth - 1);
        add_softened_cubic_for_feathering(featheredPath,
                                          pp + 3,
                                          feather,
                                          matrixMaxScale,
                                          maxDepth - 1);
        return;
    }

    // Flatten the curve down to "newHeight". (Height scales linearly as we lerp
    // the control points to "flatLinePoints".)
    float4 flatLinePoints =
        simd::mix(p0.xyxy, p3.xyxy, float4{1.f / 3, 1.f / 3, 2.f / 3, 2.f / 3});
    float softness = height != 0 ? 1 - newHeight / height : 1;
    // Do the "min" first so softness is 1 if anything went NaN.
    softness = fmaxf(0, fminf(softness, 1));
    assert(softness >= 0 && softness <= 1);
    float4 softenedPoints = simd::unchecked_mix(simd::join(p1, p2),
                                                flatLinePoints,
                                                float4(softness));
    featheredPath->cubic(math::bit_cast<Vec2D>(softenedPoints.xy),
                         math::bit_cast<Vec2D>(softenedPoints.zw),
                         p[3]);
}

rcp<RiveRenderPath> RiveRenderPath::makeSoftenedCopyForFeathering(
    float feather,
    float matrixMaxScale)
{
    RawPath featheredPath;
    // Reserve a generous amount of space upfront so we hopefully don't have to
    // reallocate -- enough for each verb to be chopped 4 times.
    featheredPath.reserve(m_rawPath.verbs().size() * 4,
                          m_rawPath.points().size() * 4);
    for (auto [verb, pts] : m_rawPath)
    {
        switch (verb)
        {
            case PathVerb::move:
                featheredPath.move(pts[0]);
                break;
            case PathVerb::line:
                featheredPath.line(pts[1]);
                break;
            case PathVerb::cubic:
            {
                // Start by chopping all cubics so they are convex and rotate no
                // more than 90 degrees. The stroke algorithm requires them not
                // to have inflections
                float T[4];
                Vec2D chops[(std::size(T) + 1) * 3 + 1]; // 4 chops will produce
                                                         // 16 cubic vertices.
                bool areCusps;
                // A generous cusp padding looks better empirically.
                constexpr static float CUSP_PADDING = 1e-2f;
                int n = math::find_cubic_convex_90_chops(pts,
                                                         T,
                                                         CUSP_PADDING,
                                                         &areCusps);
                math::chop_cubic_at(pts, chops, T, n);
                Vec2D* p = chops;
                for (int i = 0; i <= n; ++i, p += 3)
                {
                    if (areCusps && (i & 1))
                    {
                        // If the chops are straddling cusps, odd-numbered chops
                        // are the ones that pass through a cusp.
                        featheredPath.line(p[3]);
                    }
                    else
                    {
                        add_softened_cubic_for_feathering(&featheredPath,
                                                          p,
                                                          feather,
                                                          matrixMaxScale);
                    }
                }
                break;
            }
            case PathVerb::close:
                featheredPath.close();
                break;
            case PathVerb::quad:
                RIVE_UNREACHABLE();
        }
    }
    return make_rcp<RiveRenderPath>(m_fillRule, featheredPath);
}

void RiveRenderPath::setDrawCache(gpu::RiveRenderPathDraw* drawCache,
                                  const Mat2D& mat,
                                  rive::RiveRenderPaint* riveRenderPaint) const
{
    CacheElements& cache =
        m_cachedElements[riveRenderPaint->getIsStroked() ? CACHE_STROKED
                                                         : CACHE_FILLED];

    cache.draw = drawCache;

    cache.xx = mat.xx();
    cache.xy = mat.xy();
    cache.yx = mat.yx();
    cache.yy = mat.yy();

    if (riveRenderPaint->getIsStroked())
    {
        m_cachedThickness = riveRenderPaint->getThickness();
        m_cachedJoin = riveRenderPaint->getJoin();
        m_cachedCap = riveRenderPaint->getCap();
    }
    m_cachedFeather = riveRenderPaint->getFeather();
}

gpu::DrawUniquePtr RiveRenderPath::getDrawCache(
    const Mat2D& matrix,
    const RiveRenderPaint* paint,
    FillRule fillRule,
    TrivialBlockAllocator* allocator,
    const gpu::RenderContext::FrameDescriptor& frameDesc,
    gpu::InterlockMode interlockMode) const
{
    const CacheElements& cache =
        m_cachedElements[paint->getIsStroked() ? CACHE_STROKED : CACHE_FILLED];

    if (cache.draw == nullptr)
    {
        return nullptr;
    }

    if (paint->getIsStroked())
    {
        if (m_cachedThickness != paint->getThickness())
        {
            return nullptr;
        }

        if (m_cachedJoin != paint->getJoin())
        {
            return nullptr;
        }

        if (m_cachedCap != paint->getCap())
        {
            return nullptr;
        }
    }

    if (m_cachedFeather != paint->getFeather())
    {
        return nullptr;
    }

    if (matrix.xx() != cache.xx || matrix.xy() != cache.xy ||
        matrix.yx() != cache.yx || matrix.yy() != cache.yy)
    {
        return nullptr;
    }

    return gpu::DrawUniquePtr(
        allocator->make<gpu::RiveRenderPathDraw>(*cache.draw,
                                                 matrix.tx(),
                                                 matrix.ty(),
                                                 ref_rcp(this),
                                                 fillRule,
                                                 paint,
                                                 frameDesc,
                                                 interlockMode));
}
} // namespace rive
