#include "rive/text/text_selection_path.hpp"
#include "rive/shapes/path.hpp"

using namespace rive;

// Crossing-number test of a point against a simple closed contour. The
// contours coming out of RectanglesToContour never touch each other, so
// probing with a vertex of another contour is safe.
static bool contourContains(const Contour& contour, Vec2D point)
{
    size_t size = contour.size();
    if (size < 3)
    {
        return false;
    }
    bool inside = false;
    for (size_t i = 0, j = size - 1; i < size; j = i++)
    {
        Vec2D a = contour.point(i);
        Vec2D b = contour.point(j);
        if ((a.y > point.y) != (b.y > point.y) &&
            point.x < (b.x - a.x) * (point.y - a.y) / (b.y - a.y) + a.x)
        {
            inside = !inside;
        }
    }
    return inside;
}

void TextSelectionPath::update(Span<AABB> rects, float cornerRadius)
{
    rewind();
    m_rectanglesToContour.reset();
    for (const AABB& rect : rects)
    {
        m_rectanglesToContour.addRect(rect);
    }
    m_rectanglesToContour.computeContours();

    RawPath& rawPath = *mutableRawPath();
    size_t count = m_rectanglesToContour.contourCount();
    for (size_t i = 0; i < count; i++)
    {
        Contour contour = m_rectanglesToContour.contour(i);
        if (contour.size() < 2)
        {
            continue;
        }
        // Wind outer contours clockwise and holes counter-clockwise so the
        // geometry reads the same under the clockwise, non-zero and even-odd
        // fill rules. Feathered fills are only drawn when the path is
        // clockwise, so this is what lets a feather apply here at all.
        Vec2D probe = contour.point(0);
        size_t depth = 0;
        for (size_t j = 0; j < count; j++)
        {
            if (j != i &&
                contourContains(m_rectanglesToContour.contour(j), probe))
            {
                depth++;
            }
        }
        addRoundedPath(contour, cornerRadius, rawPath, (depth & 1) == 0);
    }
}

void TextSelectionPath::addRoundedPath(const Contour& contour,
                                       float radius,
                                       RawPath& rawPath,
                                       bool clockwise)
{
    bool reversed = contour.isClockwise() != clockwise;
    size_t length = contour.size();
    if (length < 2)
    {
        return;
    }

    Vec2D firstPoint = contour.point(0, reversed);
    Vec2D point = firstPoint;

    if (radius > 0.0f)
    {
        Vec2D prev = contour.point(length - 1, reversed);
        Vec2D pos = point;

        Vec2D toPrev = prev - pos;
        float toPrevLength = toPrev.length();
        toPrev.x /= toPrevLength;
        toPrev.y /= toPrevLength;

        Vec2D next = contour.point(1, reversed);

        Vec2D toNext = next - pos;
        float toNextLength = toNext.length();
        toNext.x /= toNextLength;
        toNext.y /= toNextLength;

        float renderRadius = std::min(toPrevLength / 2.0f,
                                      std::min(toNextLength / 2.0f, radius));
        float idealDistance =
            Path::computeIdealControlPointDistance(toPrev,
                                                   toNext,
                                                   renderRadius);

        Vec2D translation = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);
        rawPath.moveTo(translation.x, translation.y);

        Vec2D outPoint =
            Vec2D::scaleAndAdd(pos, toPrev, renderRadius - idealDistance);

        Vec2D inPoint =
            Vec2D::scaleAndAdd(pos, toNext, renderRadius - idealDistance);

        Vec2D posNext = Vec2D::scaleAndAdd(pos, toNext, renderRadius);
        rawPath.cubicTo(outPoint.x,
                        outPoint.y,
                        inPoint.x,
                        inPoint.y,
                        posNext.x,
                        posNext.y);
    }
    else
    {
        rawPath.moveTo(point.x, point.y);
    }

    for (size_t i = 1; i < length; i++)
    {
        Vec2D point = contour.point(i, reversed);

        if (radius > 0.0f)
        {
            Vec2D prev = contour.point(i - 1, reversed);

            Vec2D pos = point;
            Vec2D toPrev = prev - pos;

            float toPrevLength = toPrev.length();
            toPrev.x /= toPrevLength;
            toPrev.y /= toPrevLength;

            Vec2D next = contour.point((i + 1) % length, reversed);

            Vec2D toNext = next - pos;
            float toNextLength = toNext.length();
            toNext.x /= toNextLength;
            toNext.y /= toNextLength;

            float renderRadius =
                std::min(toPrevLength / 2.0f,
                         std::min(toNextLength / 2.0f, radius));

            float idealDistance =
                Path::computeIdealControlPointDistance(toPrev,
                                                       toNext,
                                                       renderRadius);

            Vec2D translation = Vec2D::scaleAndAdd(pos, toPrev, renderRadius);

            rawPath.lineTo(translation.x, translation.y);

            Vec2D outPoint =
                Vec2D::scaleAndAdd(pos, toPrev, renderRadius - idealDistance);

            Vec2D inPoint =
                Vec2D::scaleAndAdd(pos, toNext, renderRadius - idealDistance);

            Vec2D posNext = Vec2D::scaleAndAdd(pos, toNext, renderRadius);
            rawPath.cubicTo(outPoint.x,
                            outPoint.y,
                            inPoint.x,
                            inPoint.y,
                            posNext.x,
                            posNext.y);
        }
        else
        {
            rawPath.lineTo(point.x, point.y);
        }
    }
    rawPath.close();
}