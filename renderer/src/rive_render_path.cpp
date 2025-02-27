/*
 * Copyright 2022 Rive
 */

#include "rive_render_path.hpp"

#include "rive/math/bezier_utils.hpp"
#include "rive/math/simd.hpp"
#include "rive/renderer/gpu.hpp"
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

    bool isIdentity = matrix == Mat2D();
    RawPath::Iter transformedPathIter =
        m_rawPath.addPath(riveRenderPath->m_rawPath,
                          isIdentity ? nullptr : &matrix);
    if (!isIdentity)
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

void RiveRenderPath::addRawPath(const RawPath& path)
{
    m_rawPath.addPath(path, nullptr);
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

// Chops the cubic definfed by p[4] at 'numChops' locations, each defined by
// the next position where the tangent rotates by 'rotationMatrix'. Adds each
// segment to 'path'.
static void chop_cubic_at_uniform_rotation(RawPath* path,
                                           const Vec2D p[4],
                                           const Vec2D tangents[2],
                                           int numChops,
                                           const Mat2D& rotationMatrix)
{
    math::CubicCoeffs coeffs(p);
    float2 tangent = simd::load2f(&tangents[0]);
    float4 rotation = simd::load4f(rotationMatrix.values());
    const Vec2D* remainingCubic = p;
    float remainingT = 0;
    Vec2D chops[10];
    for (int i = 0; i < numChops; i += 4)
    {
        // Load up to 4 quadratic equations to solve.
        int numChopsRemaining = std::min(numChops - i, 4);
        float4 tangentsX, tangentsY;
        for (int j = 0; j < numChopsRemaining; ++j)
        {
            float4 m = rotation * tangent.xxyy;
            tangent = m.xy + m.zw;
            tangentsX[j & 3] = tangent.x;
            tangentsY[j & 3] = tangent.y;
        }

        // Solve for the T values where the tangent of the curve is equal to
        // each tangent.
        float4 a = coeffs.A.x * tangentsY - coeffs.A.y * tangentsX;
        float4 b_over_2 = coeffs.B.x * tangentsY - coeffs.B.y * tangentsX;
        float4 c = coeffs.C.x * tangentsY - coeffs.C.y * tangentsX;
        float4 discr_over_4 = b_over_2 * b_over_2 - a * c;
        float4 q = simd::sqrt(discr_over_4);
        q = -b_over_2 - simd::copysign(q, b_over_2);
        // Since C == tan0:
        //  * c/q == 0 when tangent == tan0
        //  * c/q is the root where tangent == tan0
        //  * c/q is the root we need at each subsequent step
        float4 roots = c / q;

        // Filter out any roots that fell out of order due to fp32 precision
        // issues, or are too close together for fp32 precision.
        float t[4];
        int numT = 0;
        float maxT = remainingT;
        for (int j = 0; j < numChopsRemaining; ++j)
        {
            constexpr float MIN_SPACING = 1e-4f;
            if (roots[j] > maxT + MIN_SPACING && roots[j] < 1 - MIN_SPACING)
            {
                t[numT++] = maxT = roots[j];
            }
        }

        // Chop the curve at the t values we just found. Add all but the final
        // chop to the path. Update remainingCubic[4] & remainingT to the final
        // chop.
        for (int j = 0; j < numT; j += 2)
        {
            // Localize the t values from p[4] to remainingCubic[4].
            float2 localT = simd::clamp((simd::load2f(t + j) - remainingT) /
                                            (1 - remainingT),
                                        float2(0),
                                        float2(1));
            if (j + 1 < numT)
            {
                // Two chops.
                math::chop_cubic_at(remainingCubic,
                                    chops,
                                    localT[0],
                                    localT[1]);
                path->cubic(chops[1], chops[2], chops[3]);
                path->cubic(chops[4], chops[5], chops[6]);
                remainingCubic = chops + 6;
                remainingT = t[j + 1];
            }
            else
            {
                // Only one chop is left.
                math::chop_cubic_at(remainingCubic, chops, localT[0]);
                path->cubic(chops[1], chops[2], chops[3]);
                remainingCubic = chops + 3;
                remainingT = t[j];
            }
        }
    }

    // Finally, add whatever is left over after chopping.
    path->cubic(remainingCubic[1], remainingCubic[2], remainingCubic[3]);
}

rcp<RiveRenderPath> RiveRenderPath::makeSoftenedCopyForFeathering(
    float feather,
    float matrixMaxScale)
{
    // Since curvature is what breaks 1-dimensional feathering along the normal
    // vector, chop into segments that rotate no more than a certain threshold.
    constexpr static int POLAR_JOIN_PRECISION = 2;
    float r_ = feather * (FEATHER_TEXTURE_STDDEVS / 2) * matrixMaxScale * .25f;
    float polarSegmentsPerRadian =
        math::calc_polar_segments_per_radian<POLAR_JOIN_PRECISION>(r_);
    float rotationBetweenJoins = 1 / polarSegmentsPerRadian;
    if (rotationBetweenJoins < gpu::FEATHER_POLAR_SEGMENT_MIN_ANGLE)
    {
        // Once we cross the FEATHER_POLAR_SEGMENT_MIN_ANGLE threshold, we start
        // needing fewer polar joins again. Mirror at this point and begin
        // adding back space between the joins.
        // TODO: This formula is founded entirely in what feels good visually.
        // It has almost no mathematical method. We can probably improve it.
        rotationBetweenJoins =
            gpu::FEATHER_POLAR_SEGMENT_MIN_ANGLE +
            math::pow3(
                (gpu::FEATHER_POLAR_SEGMENT_MIN_ANGLE - rotationBetweenJoins) *
                5.f);
    }
    // Our math that flattens feathered curves relies on curves not rotating
    // more than 90 degrees.
    rotationBetweenJoins = std::min(rotationBetweenJoins, math::PI / 2);
    Mat2D rotationMatrix = Mat2D::fromRotation(rotationBetweenJoins);
    Mat2D reverseRotationMatrix = Mat2D::fromRotation(-rotationBetweenJoins);

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
                // more than 180 degrees. chop_cubic_at_uniform_rotation()
                // requires them to not have inflections or rotate more than 180
                // degrees.
                float T[4];
                Vec2D chops[(std::size(T) + 1) * 3 + 1]; // 4 chops will produce
                                                         // 16 cubic vertices.
                bool areCusps;
                int n = math::find_cubic_convex_180_chops(pts, T, &areCusps);
                assert(n <= 2);
                if (areCusps)
                {
                    // Cross through cusps with short lines to avoid unstable
                    // math. Large cusp padding empirically gets better results.
                    constexpr static float CUSP_PADDING = 1e-2f;
                    for (int i = 0; i < n; ++i)
                    {
                        // If the cusps are extremely close together, don't
                        // allow the straddle points to cross.
                        float minT = i == 0 ? 0.f : (T[i - 1] + T[i]) * .5f;
                        float maxT = i + 1 == n ? 1.f : (T[i + 1] + T[i]) * .5f;
                        T[i * 2 + 0] = fmaxf(T[i] - CUSP_PADDING, minT);
                        T[i * 2 + 1] = fminf(T[i] + CUSP_PADDING, maxT);
                    }
                    n *= 2;
                }
                math::chop_cubic_at(pts, chops, T, n);
                Vec2D* p = chops;
                for (int i = 0; i <= n; ++i, p += 3)
                {
                    if (areCusps && (i & 1) == 1)
                    {
                        // This cubic contains an actual cusp. Cross through it
                        // with a line.
                        featheredPath.line(p[3]);
                        continue;
                    }
                    Vec2D tangents[2];
                    math::find_cubic_tangents(p, tangents);
                    float rotation =
                        math::measure_angle_between_vectors(tangents[0],
                                                            tangents[1]);
                    int numChops =
                        static_cast<int>(rotation / rotationBetweenJoins);
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
                    chop_cubic_at_uniform_rotation(
                        &featheredPath,
                        p,
                        tangents,
                        numChops,
                        turn >= 0 ? rotationMatrix : reverseRotationMatrix);
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
} // namespace rive
