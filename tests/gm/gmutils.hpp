/*
 * Copyright 2022 Rive
 */

#ifndef _RIVE_GM_UTILS_HPP_
#define _RIVE_GM_UTILS_HPP_

#include "rive/math/aabb.hpp"
#include "rive/renderer.hpp"
#include "rive/factory.hpp"
#include "rive/refcnt.hpp"
#include "common/testing_window.hpp"
#include <utility>
#include <vector>

namespace rivegm
{

double GetCurrSeconds();

class AutoRestore
{
    rive::Renderer* m_Ren;

public:
    AutoRestore(rive::Renderer* ren, bool doSave = false) : m_Ren(ren)
    {
        if (doSave)
        {
            m_Ren->save();
        }
    }
    ~AutoRestore() { m_Ren->restore(); }
};

rive::rcp<rive::RenderImage> LoadImage(rive::Span<uint8_t>);

class Paint
{
    rive::rcp<rive::RenderPaint> m_Paint;

public:
    Paint() : m_Paint(TestingWindow::Get()->factory()->makeRenderPaint()) {}
    Paint(rive::ColorInt c) :
        m_Paint(TestingWindow::Get()->factory()->makeRenderPaint())
    {
        m_Paint->color(c);
    }

    rive::RenderPaint* get() const { return m_Paint.get(); }
    rive::RenderPaint* operator->() const { return m_Paint.get(); }
    operator rive::RenderPaint*() const { return m_Paint.get(); }
};

class Path
{
    rive::rcp<rive::RenderPath> m_Path;

public:
    Path() : m_Path(TestingWindow::Get()->factory()->makeEmptyRenderPath()) {}
    Path(Path&& path) : m_Path(std::move(path.m_Path)) {}
    Path(const Path&) = delete;

    Path& operator=(Path&& path)
    {
        m_Path = std::move(path.m_Path);
        return *this;
    }
    Path& operator=(const Path&) = delete;

    rive::RenderPath* get() const { return m_Path.get(); }
    rive::RenderPath* operator->() const { return m_Path.get(); }
    operator rive::RenderPath*() const { return m_Path.get(); }
};

enum class PathDirection
{
    clockwise,
    counterclockwise,

    cw = clockwise,
    ccw = counterclockwise,
};

void path_addOval(rive::RenderPath*,
                  rive::AABB bounds,
                  PathDirection = PathDirection::cw);

void draw_rect(rive::Renderer* r, rive::AABB rect, rive::RenderPaint*);
void draw_oval(rive::Renderer* r, rive::AABB rect, rive::RenderPaint*);
void draw_image(rive::Renderer*, rive::RenderImage*, rive::AABB);

rive::rcp<rive::RenderPath> renderPathFromRawPath(
    rive::RawPath& path,
    const rive::FillRule fillRule = rive::FillRule::nonZero);

class PathBuilder
{
public:
    static Path Rect(rive::AABB rect, PathDirection dir = PathDirection::cw)
    {
        return PathBuilder().addRect(rect, dir).detach();
    }
    static Path Oval(rive::AABB bounds, PathDirection dir = PathDirection::cw)
    {
        return PathBuilder().addOval(bounds, dir).detach();
    }
    static Path Circle(float x,
                       float y,
                       float r,
                       PathDirection dir = PathDirection::cw)
    {
        return PathBuilder().addCircle(x, y, r, dir).detach();
    }
    static Path RRect(rive::AABB rect, float radX, float radY)
    {
        return PathBuilder().addRRect(rect, radX, radY).detach();
    }
    static Path Polygon(const std::vector<rive::Vec2D>& pts,
                        bool doClose,
                        rive::FillRule fillRule = rive::FillRule::nonZero)
    {
        return PathBuilder()
            .addPolygon(pts, doClose)
            .fillRule(fillRule)
            .detach();
    }

    PathBuilder& fillRule(rive::FillRule f);
    PathBuilder& moveTo(float x, float y);
    PathBuilder& lineTo(float x, float y);
    PathBuilder& quadTo(float ox, float oy, float x, float y);
    PathBuilder& cubicTo(float ox,
                         float oy,
                         float ix,
                         float iy,
                         float x,
                         float y);
    PathBuilder& close();

    PathBuilder& addRect(rive::AABB rect, PathDirection = PathDirection::cw);
    PathBuilder& addOval(rive::AABB rect, PathDirection = PathDirection::cw);
    PathBuilder& addCircle(float x,
                           float y,
                           float r,
                           PathDirection = PathDirection::cw);
    PathBuilder& addRRect(rive::AABB rect, float radX, float radY);
    PathBuilder& addPolygon(const std::vector<rive::Vec2D>&, bool doClose);
    PathBuilder& polylineTo(const std::vector<rive::Vec2D>&);

    Path detach() { return std::exchange(m_Path, Path()); }

private:
    Path m_Path;
    float m_FirstX = 0;
    float m_FirstY = 0;
    float m_LastX = 0;
    float m_LastY = 0;
};

void path_add_star(Path& path, int count, float anglePhase, float dir);

} // namespace rivegm

#endif
