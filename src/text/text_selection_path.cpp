#ifdef WITH_RIVE_TEXT
#include "rive/text/text_selection_path.hpp"
#include "rive/text/text.hpp"
#include "rive/shapes/path.hpp"

using namespace rive;

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
    for (auto contour : m_rectanglesToContour)
    {
        addRoundedPath(contour, cornerRadius, rawPath);
    }
}

void TextSelectionPath::addRoundedPath(const Contour& contour,
                                       float radius,
                                       RawPath& rawPath)
{
    bool isClockwise = contour.isClockwise();
    size_t length = contour.size();
    if (length < 2)
    {
        return;
    }

    Vec2D firstPoint = contour.point(0, !isClockwise);
    Vec2D point = firstPoint;

    if (radius > 0.0f)
    {
        Vec2D prev = contour.point(length - 1, !isClockwise);
        Vec2D pos = point;

        Vec2D toPrev = prev - pos;
        float toPrevLength = toPrev.length();
        toPrev.x /= toPrevLength;
        toPrev.y /= toPrevLength;

        Vec2D next = contour.point(1, !isClockwise);

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
        Vec2D point = contour.point(i, !isClockwise);

        if (radius > 0.0f)
        {
            Vec2D prev = contour.point(i - 1, !isClockwise);

            Vec2D pos = point;
            Vec2D toPrev = prev - pos;

            float toPrevLength = toPrev.length();
            toPrev.x /= toPrevLength;
            toPrev.y /= toPrevLength;

            Vec2D next = contour.point((i + 1) % length, !isClockwise);

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
#endif