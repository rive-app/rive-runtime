#include "rive/math/rectangles_to_contour.hpp"
#include <algorithm>
#include <unordered_map>

using namespace rive;

struct RectEvent
{
    int index = 0;
    float size = 0;
    int type = 0;
    float x = 0;
    float y = 0;

    float getValue(int axis) const { return axis == 0 ? x : y; }
};

std::vector<Rect> subdivideRectangles(const std::vector<Rect>& rects)
{
    if (rects.empty())
    {
        return rects;
    }

    std::vector<std::vector<std::vector<float>>> vrects;
    for (const auto& rect : rects)
    {
        vrects.push_back({{rect.left, rect.top}, {rect.right, rect.bottom}});
    }

    auto sortEvents = [&](int axisA, int axisB) -> std::vector<RectEvent> {
        std::vector<RectEvent> result;
        int index = 0;

        for (const auto& rect : vrects)
        {
            int mindex = 0;
            for (const auto& e : rect)
            {
                RectEvent event;
                event.type = mindex;
                event.index = index;
                event.y = axisB == 0 ? e[axisA] : e[axisB];
                event.x = axisB == 0 ? e[axisB] : e[axisA];
                event.size = mindex++ == 0 ? rect[1][axisB] - e[axisB]
                                           : e[axisB] - rect[0][axisB];
                result.push_back(event);
            }
            ++index;
        }

        std::sort(result.begin(),
                  result.end(),
                  [&](const RectEvent& a, const RectEvent& b) {
                      return a.getValue(axisB) < b.getValue(axisB);
                  });

        std::sort(result.begin(),
                  result.end(),
                  [&](const RectEvent& a, const RectEvent& b) {
                      return a.getValue(axisA) < b.getValue(axisA);
                  });

        return result;
    };

    auto ev = sortEvents(0, 1);
    auto eh = sortEvents(1, 0);
    std::vector<bool> included(vrects.size(), false);
    included[ev[0].index] = true;

    int opened = 0;
    float beginY = 0;
    std::vector<Rect> result;

    for (size_t i = 0; i < ev.size() - 1; ++i)
    {
        const auto& eventV = ev[i];
        included[eventV.index] = (eventV.type == 0);
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
            if (included[eventH.index])
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
                    while (j + n < eh.size() && !included[eh[j + n].index])
                    {
                        ++n;
                    }
                    const auto* next =
                        (j + n < eh.size()) ? &eh[j + n] : nullptr;
                    if (!next || (opened == 0 && next->y != eventH.y))
                    {
                        float endY = eventH.y;
                        result.push_back(
                            Rect::fromLTRB(beginX, beginY, endX, endY));
                    }
                }
            }
        }
    }

    return result;
}

void RectanglesToContour::addRect(const Rect& rect)
{
    if (!rects.empty())
    {
        auto& last = rects.back();
        if (last.top == rect.top && last.bottom == rect.bottom &&
            last.right == rect.left)
        {
            rects.pop_back();
            rects.emplace_back(
                Rect::fromLTRB(last.left, last.top, rect.right, last.bottom));
            return;
        }
    }
    rects.push_back(rect);
}

template <typename Compare>
std::vector<Vec2D> sortPoints(const std::unordered_set<Vec2D>& points,
                              Compare comp)
{
    std::vector<Vec2D> sortedPoints(points.begin(), points.end());
    std::sort(sortedPoints.begin(), sortedPoints.end(), comp);
    return sortedPoints;
}

std::vector<std::vector<Vec2D>> extractPolygons(
    std::unordered_map<Vec2D, Vec2D>& edgesH,
    std::unordered_map<Vec2D, Vec2D>& edgesV)
{
    std::vector<std::vector<Vec2D>> polygons;

    while (!edgesH.empty())
    {
        Vec2D start = edgesH.begin()->first;
        edgesH.erase(start);

        std::vector<PolygonPoint> polygon = {PolygonPoint(start, 0)};

        while (true)
        {
            auto& curr = polygon.back();
            if (curr.dir == 0)
            {
                if (edgesV.find(curr.vec) != edgesV.end())
                {
                    auto next = edgesV[curr.vec];
                    edgesV.erase(curr.vec);
                    polygon.emplace_back(next, 1);
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
                    polygon.emplace_back(next, 0);
                }
                else
                {
                    break;
                }
            }

            if (polygon.front() == polygon.back())
            {
                polygon.pop_back();
                break;
            }
        }

        std::vector<Vec2D> poly;
        std::transform(polygon.begin(),
                       polygon.end(),
                       std::back_inserter(poly),
                       [](const PolygonPoint& pp) { return pp.vec; });

        polygons.push_back(poly);
        for (const Vec2D& vertex : poly)
        {
            edgesH.erase(vertex);
            edgesV.erase(vertex);
        }
    }

    return polygons;
}

std::vector<std::vector<Vec2D>> RectanglesToContour::computeContours()
{
    auto subdividedRects = subdivideRectangles(rects);

    for (const auto& rect : subdividedRects)
    {
        addUniquePoint(Vec2D(rect.left, rect.top));
        addUniquePoint(Vec2D(rect.right, rect.top));
        addUniquePoint(Vec2D(rect.right, rect.bottom));
        addUniquePoint(Vec2D(rect.left, rect.bottom));
    }

    auto sortX = sortPoints(uniquePoints, [](const Vec2D& a, const Vec2D& b) {
        return a.x < b.x || (a.x == b.x && a.y < b.y);
    });

    auto sortY = sortPoints(uniquePoints, [](const Vec2D& a, const Vec2D& b) {
        return a.y < b.y || (a.y == b.y && a.x < b.x);
    });

    std::unordered_map<Vec2D, Vec2D> edgesH, edgesV;
    size_t i = 0;
    while (i < sortY.size())
    {
        float currY = sortY[i].y;
        while (i < sortY.size() && sortY[i].y == currY)
        {
            edgesH[sortY[i]] = sortY[i + 1];
            edgesH[sortY[i + 1]] = sortY[i];
            i += 2;
        }
    }

    i = 0;
    while (i < sortX.size())
    {
        float currX = sortX[i].x;
        while (i < sortX.size() && sortX[i].x == currX)
        {
            edgesV[sortX[i]] = sortX[i + 1];
            edgesV[sortX[i + 1]] = sortX[i];
            i += 2;
        }
    }

    return extractPolygons(edgesH, edgesV);
}

void RectanglesToContour::addUniquePoint(const Vec2D& point)
{
    if (!uniquePoints.insert(point).second)
    {
        uniquePoints.erase(point);
    }
}

std::vector<std::vector<Vec2D>> RectanglesToContour::makeSelectionContours(
    const std::vector<Rect>& rects)
{
    RectanglesToContour converter;

    // Add rectangles to the converter
    for (const auto& rect : rects)
    {
        converter.addRect(rect);
    }

    std::vector<std::vector<Vec2D>> renderContours;
    auto contours = converter.computeContours();

    // Process the contours to create renderContours
    for (const auto& contour : contours)
    {
        if (contour.empty())
        {
            continue;
        }

        std::vector<Vec2D> renderContour = {contour.front()};

        for (auto it = std::next(contour.begin()); it != contour.end(); ++it)
        {
            if (renderContour.back() == *it)
            {
                continue;
            }
            renderContour.push_back(*it);
        }

        renderContours.push_back(renderContour);
    }

    return renderContours;
}
