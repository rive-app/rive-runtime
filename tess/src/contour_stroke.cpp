#include "rive/math/math_types.hpp"
#include "rive/tess/contour_stroke.hpp"
#include "rive/tess/segmented_contour.hpp"
#include "rive/math/vec2d.hpp"
#include <assert.h>
#include <algorithm>

using namespace rive;

static const int subdivisionArcLength = 4.0f;

void ContourStroke::reset()
{
    m_TriangleStrip.clear();
    m_Offsets.clear();
}

void ContourStroke::resetRenderOffset() { m_RenderOffset = 0; }

bool ContourStroke::nextRenderOffset(std::size_t& start, std::size_t& end)
{
    if (m_RenderOffset == m_Offsets.size())
    {
        return false;
    }
    start = m_RenderOffset == 0 ? 0 : m_Offsets[m_RenderOffset - 1];
    end = m_Offsets[m_RenderOffset++];
    return true;
}

void ContourStroke::extrude(const SegmentedContour* contour,
                            bool isClosed,
                            StrokeJoin join,
                            StrokeCap cap,
                            float strokeWidth)
{
    auto contourPoints = contour->contourPoints();
    std::vector<Vec2D> points(contourPoints.begin(), contourPoints.end());

    auto pointCount = points.size();
    if (pointCount < 2)
    {
        return;
    }
    auto startOffset = m_TriangleStrip.size();
    Vec2D lastPoint = points[0];
    Vec2D lastDiff = points[1] - lastPoint;

    float lastLength = lastDiff.length();
    Vec2D lastDiffNormalized = lastDiff / lastLength;

    Vec2D perpendicularStrokeDiff =
        Vec2D(lastDiffNormalized.y * -strokeWidth, lastDiffNormalized.x * strokeWidth);
    Vec2D lastA = lastPoint + perpendicularStrokeDiff;
    Vec2D lastB = lastPoint - perpendicularStrokeDiff;

    if (!isClosed)
    {
        switch (cap)
        {
            case StrokeCap::square:
            {
                Vec2D strokeDiff = lastDiffNormalized * strokeWidth;
                Vec2D squareA = lastA - strokeDiff;
                Vec2D squareB = lastB - strokeDiff;
                m_TriangleStrip.push_back(squareA);
                m_TriangleStrip.push_back(squareB);
                break;
            }
            case StrokeCap::round:
            {
                Vec2D capDirection = Vec2D(-lastDiffNormalized.y, lastDiffNormalized.x);
                float arcLength = std::abs(math::PI * strokeWidth);
                int steps = (int)std::ceil(arcLength / subdivisionArcLength);
                float angleTo = std::atan2(capDirection.y, capDirection.x);
                float inc = math::PI / steps;
                float angle = angleTo;
                // make sure to draw the full cap due triangle strip
                for (int j = 0; j <= steps; j++)
                {
                    m_TriangleStrip.push_back(lastPoint);
                    m_TriangleStrip.push_back(Vec2D(lastPoint.x + std::cos(angle) * strokeWidth,
                                                    lastPoint.y + std::sin(angle) * strokeWidth));
                    angle += inc;
                }
                break;
            }
            default:
                break;
        }
    }
    m_TriangleStrip.push_back(lastA);
    m_TriangleStrip.push_back(lastB);

    pointCount -= isClosed ? 1 : 0;
    std::size_t adjustedPointCount = isClosed ? pointCount + 1 : pointCount;

    for (std::size_t i = 1; i < adjustedPointCount; i++)
    {
        const Vec2D& point = points[i % pointCount];
        Vec2D diff, diffNormalized, next;
        float length;
        if (i < adjustedPointCount - 1 || isClosed)
        {
            diff = (next = points[(i + 1) % pointCount]) - point;
            length = diff.length();
            diffNormalized = diff / length;
        }
        else
        {
            diff = lastDiff;
            next = point;
            length = lastLength;
            diffNormalized = lastDiffNormalized;
        }

        // perpendicular dx
        float pdx0 = -lastDiffNormalized.y;
        float pdy0 = lastDiffNormalized.x;
        float pdx1 = -diffNormalized.y;
        float pdy1 = diffNormalized.x;

        // Compute bisector without a normalization by averaging perpendicular
        // diffs.
        Vec2D bisector((pdx0 + pdx1) * 0.5f, (pdy0 + pdy1) * 0.5f);
        float cross = Vec2D::cross(diff, lastDiff);
        float dot = Vec2D::dot(bisector, bisector);

        float lengthLimit = std::min(length, lastLength);
        bool bevelInner = false;
        bool bevel = join == StrokeJoin::miter ? dot < 0.1f : dot < 0.999f;

        // Scale bisector to match stroke size.
        if (dot > 0.000001f)
        {
            float scale = 1.0f / dot * strokeWidth;
            float limit = lengthLimit / strokeWidth;
            if (dot * limit * limit < 1.0f)
            {
                bevelInner = true;
            }
            bisector *= scale;
        }
        else
        {
            bisector *= strokeWidth;
        }

        if (!bevel)
        {
            Vec2D c = point + bisector;
            Vec2D d = point - bisector;

            if (!bevelInner)
            {
                // Normal mitered edge.
                m_TriangleStrip.push_back(c);
                m_TriangleStrip.push_back(d);
            }
            else if (cross <= 0)
            {
                // Overlap the inner (in this case right) edge (sometimes called
                // miter inner).
                Vec2D c1 = point + Vec2D(lastDiffNormalized.y * -strokeWidth,
                                         lastDiffNormalized.x * strokeWidth);
                Vec2D c2 =
                    point + Vec2D(diffNormalized.y * -strokeWidth, diffNormalized.x * strokeWidth);

                m_TriangleStrip.push_back(c1);
                m_TriangleStrip.push_back(d);
                m_TriangleStrip.push_back(c2);
                m_TriangleStrip.push_back(d);
            }
            else
            {
                // Overlap the inner (in this case left) edge (sometimes called
                // miter inner).
                Vec2D d1 = point - Vec2D(lastDiffNormalized.y * -strokeWidth,
                                         lastDiffNormalized.x * strokeWidth);
                Vec2D d2 =
                    point - Vec2D(diffNormalized.y * -strokeWidth, diffNormalized.x * strokeWidth);

                m_TriangleStrip.push_back(c);
                m_TriangleStrip.push_back(d1);
                m_TriangleStrip.push_back(c);
                m_TriangleStrip.push_back(d2);
            }
        }
        else
        {
            Vec2D ldPStroke =
                Vec2D(lastDiffNormalized.y * -strokeWidth, lastDiffNormalized.x * strokeWidth);
            Vec2D dPStroke = Vec2D(diffNormalized.y * -strokeWidth, diffNormalized.x * strokeWidth);
            if (cross <= 0)
            {
                // Bevel the outer (left in this case) edge.
                Vec2D a1;
                Vec2D a2;

                if (bevelInner)
                {
                    a1 = point + ldPStroke;
                    a2 = point + dPStroke;
                }
                else
                {
                    a1 = point + bisector;
                    a2 = a1;
                }

                Vec2D b = point - ldPStroke;
                Vec2D bn = point - dPStroke;

                m_TriangleStrip.push_back(a1);
                m_TriangleStrip.push_back(b);
                if (join == StrokeJoin::round)
                {
                    const Vec2D& pivot = bevelInner ? point : a1;
                    Vec2D toPrev = bn - point;
                    Vec2D toNext = b - point;
                    float angleFrom = std::atan2(toPrev.y, toPrev.x);
                    float angleTo = std::atan2(toNext.y, toNext.x);
                    if (angleTo > angleFrom)
                    {
                        angleTo -= math::PI * 2.0f;
                    }
                    float range = angleTo - angleFrom;
                    float arcLength = std::abs(range * strokeWidth);
                    int steps = std::ceil(arcLength / subdivisionArcLength);

                    float inc = range / steps;
                    float angle = angleTo - inc;
                    for (int j = 0; j < steps - 1; j++)
                    {
                        m_TriangleStrip.push_back(pivot);
                        m_TriangleStrip.emplace_back(
                            Vec2D(point.x + std::cos(angle) * strokeWidth,
                                  point.y + std::sin(angle) * strokeWidth));

                        angle -= inc;
                    }
                }
                m_TriangleStrip.push_back(a2);
                m_TriangleStrip.push_back(bn);
            }
            else
            {
                // Bevel the outer (right in this case) edge.
                Vec2D b1;
                Vec2D b2;
                if (bevelInner)
                {
                    b1 = point - ldPStroke;
                    b2 = point - dPStroke;
                }
                else
                {
                    b1 = point - bisector;
                    b2 = b1;
                }

                Vec2D a = point + ldPStroke;
                Vec2D an = point + dPStroke;

                m_TriangleStrip.push_back(a);
                m_TriangleStrip.push_back(b1);

                if (join == StrokeJoin::round)
                {
                    const Vec2D& pivot = bevelInner ? point : b1;
                    Vec2D toPrev = a - point;
                    Vec2D toNext = an - point;
                    float angleFrom = std::atan2(toPrev.y, toPrev.x);
                    float angleTo = std::atan2(toNext.y, toNext.x);
                    if (angleTo > angleFrom)
                    {
                        angleTo -= math::PI * 2.0f;
                    }

                    float range = angleTo - angleFrom;
                    float arcLength = std::abs(range * strokeWidth);
                    int steps = std::ceil(arcLength / subdivisionArcLength);
                    float inc = range / steps;

                    float angle = angleFrom + inc;
                    for (int j = 0; j < steps - 1; j++)
                    {
                        m_TriangleStrip.emplace_back(
                            Vec2D(point.x + std::cos(angle) * strokeWidth,
                                  point.y + std::sin(angle) * strokeWidth));
                        m_TriangleStrip.push_back(pivot);
                        angle += inc;
                    }
                }
                m_TriangleStrip.push_back(an);
                m_TriangleStrip.push_back(b2);
            }
        }

        lastPoint = point;
        lastDiff = diff;
        lastDiffNormalized = diffNormalized;
    }

    if (isClosed)
    {
        auto last = m_TriangleStrip.size() - 1;
        m_TriangleStrip[startOffset] = m_TriangleStrip[last - 1];
        m_TriangleStrip[startOffset + 1] = m_TriangleStrip[last];
    }
    else
    {
        switch (cap)
        {
            case StrokeCap::square:
            {
                auto l = m_TriangleStrip.size();

                Vec2D strokeDiff = lastDiffNormalized * strokeWidth;
                Vec2D squareA = m_TriangleStrip[l - 2] + strokeDiff;
                Vec2D squareB = m_TriangleStrip[l - 1] + strokeDiff;

                m_TriangleStrip.push_back(squareA);
                m_TriangleStrip.push_back(squareB);
                break;
            }
            case StrokeCap::round:
            {
                Vec2D capDirection = Vec2D(-lastDiffNormalized.y, lastDiffNormalized.x);
                float arcLength = std::abs(math::PI * strokeWidth);
                int steps = (int)std::ceil(arcLength / subdivisionArcLength);
                float angleTo = std::atan2(capDirection.y, capDirection.x);
                float inc = math::PI / steps;
                float angle = angleTo;
                // make sure to draw the full cap due triangle strip
                for (int j = 0; j <= steps; j++)
                {
                    m_TriangleStrip.push_back(lastPoint);
                    m_TriangleStrip.push_back(Vec2D(lastPoint.x + std::cos(angle) * strokeWidth,
                                                    lastPoint.y + std::sin(angle) * strokeWidth));
                    angle -= inc;
                }
                break;
            }
            default:
                break;
        }
    }

    m_Offsets.push_back(m_TriangleStrip.size());
}