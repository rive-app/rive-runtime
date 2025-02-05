/*
 * Copyright 2022 Rive
 */

#include "rive_render_path.hpp"

#include "rive/math/bezier_utils.hpp"
#include "rive/math/simd.hpp"
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
                                              float rotationBetweenJoins,
                                              float totalRotation)
{
    math::CubicCoeffs coeffs(p);

    // If we rotate too much _and_ the endpoints are further apart than 1
    // standard deviation of each other, chop and recurse.
    // ("feather" is 2 standard deviations, so (feather^2)/4 == one_stddev^2.)
    if (abs(totalRotation) > abs(rotationBetweenJoins) + 1e-2f &&
        math::length_squared(p[3] - p[0]) > feather * feather * .25f)
    {
        // The cubic rotates more than rotationBetweenJoins. Find a boundary
        // of rotationBetweenJoins toward the center to chop on.
        float chopTheta = ceilf(totalRotation / (2 * rotationBetweenJoins)) *
                          rotationBetweenJoins;
        Vec2D tan0 = math::find_cubic_tan0(p);
        float2 chopTan =
            math::bit_cast<float2>(Mat2D::fromRotation(chopTheta) * tan0);

        // Solve for T where the tangent of the curve is equal to chopTan.
        float a = simd::cross(coeffs.A, chopTan);
        float b_over_2 = simd::cross(coeffs.B, chopTan);
        float c = simd::cross(coeffs.C, chopTan);
        float discr_over_4 = b_over_2 * b_over_2 - a * c;
        float q = sqrtf(discr_over_4);
        q = -b_over_2 - copysignf(q, b_over_2);
        float2 roots = float2{q, c} / float2{a, q};
        float t =
            fabsf(roots.x - .5f) < fabsf(roots.y - .5f) ? roots.x : roots.y;
        if (t > 0 && t < 1)
        {
            // Chop and recurse.
            Vec2D pp[7];
            math::chop_cubic_at(p, pp, t);
            add_softened_cubic_for_feathering(featheredPath,
                                              pp,
                                              feather,
                                              rotationBetweenJoins,
                                              totalRotation * .5f);
            add_softened_cubic_for_feathering(featheredPath,
                                              pp + 3,
                                              feather,
                                              rotationBetweenJoins,
                                              totalRotation * .5f);
            return;
        }
    }

    // Find the point of maximum height on the cubic.
    float maxHeightT;
    float height = math::find_cubic_max_height(p, &maxHeightT);

    // Measure curvature across one standard deviation of the feather.
    // ("feather" is 2 std devs.)
    float desiredSpread = feather * .5f;

    // The feather gets softer with curvature. Find a dimming factor based on
    // the strength of curvature at maximum height.
    float theta = math::measure_cubic_local_curvature(p,
                                                      coeffs,
                                                      maxHeightT,
                                                      desiredSpread);
    float dimming = 1 - theta * (1 / math::PI);

    // Soften the feather by reducing the curve height. Find a new height such
    // that the center of the feather (currently 50% opacity) is reduced to
    // "50% * dimming".
    float desiredOpacityOnCenter = .5f * dimming;
    float x = gpu::inverse_gaussian_integral(desiredOpacityOnCenter) - .5f;
    float softenedHeight = height + feather * FEATHER_TEXTURE_STDDEVS * x;

    // Flatten the curve down to "softenedHeight". (Height scales linearly as we
    // lerp the control points to "flatLinePoints".)
    float4 flatLinePoints =
        simd::mix(simd::load2f(p).xyxy,
                  simd::load2f(p + 3).xyxy,
                  float4{1.f / 3, 1.f / 3, 2.f / 3, 2.f / 3});
    float softness = height != 0 ? 1 - softenedHeight / height : 1;
    // Do the "min" first so softness is 1 if anything went NaN.
    softness = fmaxf(0, fminf(softness, 1));
    assert(softness >= 0 && softness <= 1);
    float4 softenedPoints = simd::unchecked_mix(simd::load4f(p + 1), // [p1, p2]
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
    // Since curvature is what breaks 1-dimensional feathering along the normal
    // vector, chop into segments that rotate no more than a certain threshold.
    float r_ = feather * (FEATHER_TEXTURE_STDDEVS / 2) * matrixMaxScale * .25f;
    float polarSegmentsPerRadian =
        math::calc_polar_segments_per_radian<gpu::kPolarPrecision>(r_);
    float rotationBetweenJoins = std::max(1 / polarSegmentsPerRadian,
                                          gpu::FEATHER_POLAR_SEGMENT_MIN_ANGLE);

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
                        Vec2D tangents[2];
                        math::find_cubic_tangents(p, tangents);
                        // Determine which the direction the curve turns.
                        // NOTE: Since the curve does not inflect, we can just
                        // check F'(.5) x F''(.5).
                        // NOTE: F'(.5) x F''(.5) has the same sign as
                        // (p2 - p0) x (p3 - p1).
                        float turn = Vec2D::cross(p[2] - p[0], p[3] - p[1]);
                        if (turn == 0)
                        {
                            // This is the case for joins and cusps where points
                            // are co-located.
                            turn = Vec2D::cross(tangents[0], tangents[1]);
                        }
                        float totalRotation = copysignf(
                            math::measure_angle_between_vectors(tangents[0],
                                                                tangents[1]),
                            turn);
                        add_softened_cubic_for_feathering(
                            &featheredPath,
                            p,
                            feather,
                            copysignf(rotationBetweenJoins, totalRotation),
                            totalRotation);
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
