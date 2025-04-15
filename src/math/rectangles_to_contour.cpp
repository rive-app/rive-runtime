#include "rive/math/rectangles_to_contour.hpp"
#include <algorithm>

using namespace rive;

using RectEvent = RectanglesToContour::RectEvent;

class SpanOffset
{
public:
    size_t start;
    size_t size;

    Span<RectEvent> toSpan(Span<RectEvent> base)
    {
        return Span<RectEvent>(base.data() + start, size);
    }
};

// Returns SpanOffset relative to result so it can be called in succession
// (re-allocating) prior to accessing data.
static SpanOffset sortRectEvents(const std::vector<AABB>& rects,
                                 std::vector<RectEvent>& result,
                                 uint8_t axisA,
                                 uint8_t axisB)
{
    size_t index = 0;

    size_t resultStart = result.size();

    for (const AABB& rect : rects)
    {
        for (size_t pointIndex = 0; pointIndex < 2; pointIndex++)
        {
            Vec2D e = rect[pointIndex];
            RectEvent event;
            event.type = (uint8_t)pointIndex;
            event.index = index;
            event.y = axisB == 0 ? e[axisA] : e[axisB];
            event.x = axisB == 0 ? e[axisB] : e[axisA];
            event.size = pointIndex == 0 ? rect[1][axisB] - e[axisB]
                                         : e[axisB] - rect[0][axisB];
            result.push_back(event);
        }
        ++index;
    }

    std::sort(result.begin() + resultStart,
              result.end(),
              [&](const RectEvent& a, const RectEvent& b) {
                  return a.getValue(axisB) < b.getValue(axisB);
              });

    std::sort(result.begin() + resultStart,
              result.end(),
              [&](const RectEvent& a, const RectEvent& b) {
                  return a.getValue(axisA) < b.getValue(axisA);
              });

    return {resultStart, result.size() - resultStart};
}

bool RectanglesToContour::isRectIncluded(size_t rectIndex)
{
    return (m_rectInclusionBitset[rectIndex / 8] & (1 << (rectIndex % 8))) != 0;
}

void RectanglesToContour::markRectIncluded(size_t rectIndex, bool isIt)
{
    if (isIt)
    {
        m_rectInclusionBitset[rectIndex / 8] |= 1 << (rectIndex % 8);
    }
    else
    {
        m_rectInclusionBitset[rectIndex / 8] &= ~(1 << (rectIndex % 8));
    }
}

void RectanglesToContour::subdivideRectangles()
{
    m_subdividedRects.clear();
    m_uniquePoints.clear();
    m_rectEvents.clear();

    if (m_rects.empty())
    {
        return;
    }

    auto evOffset = sortRectEvents(m_rects, m_rectEvents, 0, 1);
    auto ehOffset = sortRectEvents(m_rects, m_rectEvents, 1, 0);

    auto ev = evOffset.toSpan(m_rectEvents);
    auto eh = ehOffset.toSpan(m_rectEvents);

    size_t inclusionByteSize = m_rects.size() / 8 + 1;
    m_rectInclusionBitset.resize(inclusionByteSize);
    memset(m_rectInclusionBitset.data(), 0, inclusionByteSize);
    markRectIncluded(ev[0].index, true);

    int opened = 0;
    float beginY = 0;
    std::vector<AABB> result;

    for (size_t i = 0; i < ev.size() - 1; ++i)
    {
        const auto& eventV = ev[i];
        markRectIncluded(eventV.index, eventV.type == 0);
        const auto& next = ev[i + 1];
        float beginX = eventV.x;
        float endX = next.x;
        float deltaX = next.x - eventV.x;
        if (deltaX == 0)
        {
            continue;
        }

        for (size_t j = 0; j < eh.size(); ++j)
        {
            const auto& eventH = eh[j];
            if (isRectIncluded(eventH.index))
            {
                if (eventH.type == 0)
                {
                    ++opened;
                    if (opened == 1)
                    {
                        beginY = eventH.y;
                    }
                }
                else
                {
                    --opened;
                    size_t n = 1;
                    while (j + n < eh.size() &&
                           !isRectIncluded(eh[j + n].index))
                    {
                        ++n;
                    }
                    const auto* next =
                        (j + n < eh.size()) ? &eh[j + n] : nullptr;
                    if (!next || (opened == 0 && next->y != eventH.y))
                    {
                        float endY = eventH.y;
                        m_subdividedRects.push_back(
                            AABB(beginX, beginY, endX, endY));
                    }
                }
            }
        }
    }
}

void RectanglesToContour::reset() { m_rects.clear(); }

void RectanglesToContour::addRect(const AABB& rect)
{
    if (!m_rects.empty())
    {
        auto& last = m_rects.back();
        if (last.minY == rect.minY && last.maxY == rect.maxY &&
            last.maxX == rect.minX)
        {
            m_rects.pop_back();
            m_rects.emplace_back(
                AABB(last.minX, last.minY, rect.maxX, last.maxY));
            return;
        }
    }
    m_rects.push_back(rect);
}

// Build contours and append them into contourPoints delineated by
// contourOffsets.
void extractPolygons(std::vector<ContourPoint>& contourPoints,
                     std::vector<size_t>& contourOffsets,
                     EdgeMap& edgesH,
                     EdgeMap& edgesV)
{

    while (!edgesH.empty())
    {
        size_t contourStartOffset = contourPoints.size();
        Vec2D start = edgesH.begin()->first;
        edgesH.erase(start);

        auto first = ContourPoint(start, 0);
        contourPoints.push_back(first);

        while (true)
        {
            auto& curr = contourPoints.back();
            if (curr.dir == 0)
            {
                if (edgesV.find(curr.vec) != edgesV.end())
                {
                    auto next = edgesV[curr.vec];
                    edgesV.erase(curr.vec);
                    contourPoints.emplace_back(next, 1);
                }
                else
                {
                    break;
                }
            }
            else
            {
                if (edgesH.find(curr.vec) != edgesH.end())
                {
                    auto next = edgesH[curr.vec];
                    edgesH.erase(curr.vec);
                    contourPoints.emplace_back(next, 0);
                }
                else
                {
                    break;
                }
            }

            // Remove the last point if the first and last in the contour are
            // the same.
            if (first == contourPoints.back())
            {
                contourPoints.pop_back();
                break;
            }
        }

        contourOffsets.push_back(contourPoints.size());
        for (size_t index = contourStartOffset; index < contourPoints.size();
             index++)
        {
            Vec2D vertex = contourPoints[index].vec;
            edgesH.erase(vertex);
            edgesV.erase(vertex);
        }
    }
}

void RectanglesToContour::computeContours()
{
    subdivideRectangles();
    for (const auto& rect : m_subdividedRects)
    {
        addUniquePoint(Vec2D(rect.minX, rect.minY));
        addUniquePoint(Vec2D(rect.maxX, rect.minY));
        addUniquePoint(Vec2D(rect.maxX, rect.maxY));
        addUniquePoint(Vec2D(rect.minX, rect.maxY));
    }

    m_sortedPointsX.clear();
    m_sortedPointsY.clear();
    for (auto pt : m_uniquePoints)
    {
        m_sortedPointsX.push_back(pt);
        m_sortedPointsY.push_back(pt);
    }
    std::sort(m_sortedPointsX.begin(),
              m_sortedPointsX.end(),
              [](const Vec2D& a, const Vec2D& b) {
                  return a.x < b.x || (a.x == b.x && a.y < b.y);
              });
    std::sort(m_sortedPointsY.begin(),
              m_sortedPointsY.end(),
              [](const Vec2D& a, const Vec2D& b) {
                  return a.y < b.y || (a.y == b.y && a.x < b.x);
              });

    // std::unordered_map isn't guaranteed to reserve memory, so this clear
    // is more in prep for when we allow using allocators. We could also
    // experiment with a simple linear search in a vector as usually there
    // are not a lot of edges to traverse.
    m_edgesH.clear();
    m_edgesV.clear();

    size_t i = 0;
    while (i < m_sortedPointsY.size())
    {
        float currY = m_sortedPointsY[i].y;
        while (i < m_sortedPointsY.size() && m_sortedPointsY[i].y == currY)
        {
            m_edgesH[m_sortedPointsY[i]] = m_sortedPointsY[i + 1];
            m_edgesH[m_sortedPointsY[i + 1]] = m_sortedPointsY[i];
            i += 2;
        }
    }

    i = 0;
    while (i < m_sortedPointsX.size())
    {
        float currX = m_sortedPointsX[i].x;
        while (i < m_sortedPointsX.size() && m_sortedPointsX[i].x == currX)
        {
            m_edgesV[m_sortedPointsX[i]] = m_sortedPointsX[i + 1];
            m_edgesV[m_sortedPointsX[i + 1]] = m_sortedPointsX[i];
            i += 2;
        }
    }

    m_contourPoints.clear();
    m_contourOffsets.clear();
    extractPolygons(m_contourPoints, m_contourOffsets, m_edgesH, m_edgesV);
}

void RectanglesToContour::addUniquePoint(const Vec2D& point)
{
    if (!m_uniquePoints.insert(point).second)
    {
        m_uniquePoints.erase(point);
    }
}

ContourItr& ContourItr::operator++()
{
    m_contourIndex++;
    return *this;
}
Contour ContourItr::operator*() const
{
    return m_rectanglesToContour->contour(m_contourIndex);
}

Contour RectanglesToContour::contour(size_t index) const
{
    assert(index < m_contourOffsets.size());
    size_t end = m_contourOffsets[index];
    size_t start = index == 0 ? 0 : m_contourOffsets[index - 1];
    const ContourPoint* data = m_contourPoints.data() + start;
    size_t size = end - start;
    return Contour(Span<const ContourPoint>(data, size));
}

ContourPointItr& ContourPointItr::operator++()
{
    Vec2D currentPoint = m_pointIndex < m_contour.size() ? *(*this) : Vec2D();
    m_pointIndex++;
    while (m_pointIndex < m_contour.size())
    {
        auto nextPoint = m_contour[m_pointIndex].vec;
        if (nextPoint != currentPoint)
        {
            break;
        }
        m_pointIndex++;
    }
    return *this;
}

Vec2D ContourPointItr::operator*() const { return m_contour[m_pointIndex].vec; }
