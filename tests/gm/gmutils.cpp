/*
 * Copyright 2022 Rive
 */

#include "gmutils.hpp"

#include "rive/math/mat2d.hpp"
#include "skia/include/core/SkMatrix.h"

#include <chrono>
#include <vector>

namespace rivegm
{

double GetCurrSeconds()
{
    auto now = std::chrono::steady_clock::now();
    std::chrono::duration<double> ns = now.time_since_epoch();
    return ns.count();
}

rive::Mat2D mat2d_fromSkMatrix(const SkMatrix& skMatrix)
{
    return rive::Mat2D(skMatrix.getScaleX(),
                       skMatrix.getSkewY(),
                       skMatrix.getSkewX(),
                       skMatrix.getScaleY(),
                       skMatrix.getTranslateX(),
                       skMatrix.getTranslateY());
}

// Approximates with 4 cubics
// see https://spencermortensen.com/articles/bezier-circle/
//
constexpr float C = 0.5519150244935105707435627f;
void path_addOval(rive::RenderPath* path, rive::AABB bounds, PathDirection dir)
{
    // precompute clockwise unit circle, starting and ending at {1, 0}
    constexpr rive::Vec2D unit[] = {
        {1, 0},
        {1, C},
        {C, 1}, // quadrant 1 ( 4:30)
        {0, 1},
        {-C, 1},
        {-1, C}, // quadrant 2 ( 7:30)
        {-1, 0},
        {-1, -C},
        {-C, -1}, // quadrant 3 (10:30)
        {0, -1},
        {C, -1},
        {1, -C}, // quadrant 4 ( 1:30)
        {1, 0},
    };

    const rive::Vec2D center = bounds.center();
    const float dx = center.x;
    const float dy = center.y;
    const float sx = bounds.width() * 0.5f;
    const float sy = bounds.height() * 0.5f;

    auto map = [dx, dy, sx, sy](rive::Vec2D p) {
        return rive::Vec2D(p.x * sx + dx, p.y * sy + dy);
    };

    if (dir == PathDirection::clockwise)
    {
        path->move(map(unit[0]));
        for (int i = 1; i <= 12; i += 3)
        {
            path->cubic(map(unit[i + 0]), map(unit[i + 1]), map(unit[i + 2]));
        }
    }
    else
    {
        path->move(map(unit[12]));
        for (int i = 11; i >= 0; i -= 3)
        {
            path->cubic(map(unit[i - 0]), map(unit[i - 1]), map(unit[i - 2]));
        }
    }
    path->close();
}

void draw_rect(rive::Renderer* r, rive::AABB rect, rive::RenderPaint* paint)
{
    Path path;
    path->moveTo(rect.minX, rect.minY);
    path->lineTo(rect.maxX, rect.minY);
    path->lineTo(rect.maxX, rect.maxY);
    path->lineTo(rect.minX, rect.maxY);
    path->close();

    r->drawPath(path, paint);
}

void draw_oval(rive::Renderer* r, rive::AABB bounds, rive::RenderPaint* paint)
{
    Path path;
    path_addOval(path.get(), bounds);
    r->drawPath(path, paint);
}

void draw_image(rive::Renderer* ren, rive::RenderImage* img, rive::AABB dst)
{
    if (!img)
    {
        return;
    }

    rive::Mat2D mat =
        rive::Mat2D::fromTranslate(dst.left(), dst.top()) *
        rive::Mat2D::fromScale(dst.width() / img->width(), dst.height() / img->height());

    ren->save();
    ren->transform(mat);
    ren->drawImage(img, rive::BlendMode::srcOver, 1);
    ren->restore();
}

PathBuilder& PathBuilder::fillRule(rive::FillRule f)
{
    m_Path->fillRule(f);
    return *this;
}

PathBuilder& PathBuilder::moveTo(float x, float y)
{
    m_Path->moveTo(x, y);
    m_FirstX = m_LastX = x;
    m_FirstY = m_LastY = y;
    return *this;
}

PathBuilder& PathBuilder::lineTo(float x, float y)
{
    m_Path->lineTo(x, y);
    m_LastX = x;
    m_LastY = y;
    return *this;
}

PathBuilder& PathBuilder::quadTo(float ox, float oy, float x, float y)
{
    m_Path->cubicTo(m_LastX + (ox - m_LastX) * (2 / 3.f),
                    m_LastY + (oy - m_LastY) * (2 / 3.f),
                    x + (ox - x) * (2 / 3.f),
                    y + (oy - y) * (2 / 3.f),
                    x,
                    y);
    m_LastX = x;
    m_LastY = y;
    return *this;
}

PathBuilder& PathBuilder::cubicTo(float ox, float oy, float ix, float iy, float x, float y)
{
    m_Path->cubicTo(ox, oy, ix, iy, x, y);
    m_LastX = x;
    m_LastY = y;
    return *this;
}

PathBuilder& PathBuilder::close()
{
    m_Path->close();
    m_LastX = m_FirstX;
    m_LastY = m_FirstY;
    return *this;
}

PathBuilder& PathBuilder::addRect(rive::AABB rect, PathDirection dir)
{
    float l = rect.left(), t = rect.top(), r = rect.right(), b = rect.bottom();
    moveTo(l, t);
    if (dir == PathDirection::cw)
    {
        lineTo(r, t);
    }
    else
    {
        lineTo(l, b);
    }
    lineTo(r, b);
    if (dir == PathDirection::cw)
    {
        lineTo(l, b);
    }
    else
    {
        lineTo(r, t);
    }
    return close();
}

PathBuilder& PathBuilder::addOval(rive::AABB bounds, PathDirection dir)
{
    path_addOval(m_Path, bounds, dir);
    return *this;
}

PathBuilder& PathBuilder::addCircle(float x, float y, float r, PathDirection dir)
{
    return addOval({x - r, y - r, x + r, y + r}, dir);
}

PathBuilder& PathBuilder::addRRect(rive::AABB rect, float radX, float radY)
{
    float l = rect.left(), t = rect.top(), r = rect.right(), b = rect.bottom();
    moveTo(l + radX, t);
    lineTo(r - radX, t);
    cubicTo(r - radX * (1 - C), t, r, t + radY * C, r, t + radY);
    lineTo(r, b - radY);
    cubicTo(r, b - radY * (1 - C), r - radX * (1 - C), b, r - radX, b);
    lineTo(l + radX, b);
    cubicTo(l + radX * C, b, l, b - radY * (1 - C), l, b - radY);
    lineTo(l, t + radY);
    cubicTo(l, t + radY * C, l + radX * C, t, l + radX, t);
    return close();
}

PathBuilder& PathBuilder::addPolygon(const std::vector<rive::Vec2D>& pts, bool doClose)
{
    moveTo(pts[0].x, pts[0].y);
    for (size_t i = 1; i < pts.size(); ++i)
    {
        lineTo(pts[i].x, pts[i].y);
    }
    if (doClose)
    {
        close();
    }
    return *this;
}

PathBuilder& PathBuilder::polylineTo(const std::vector<rive::Vec2D>& pts)
{
    for (size_t i = 1; i < pts.size(); ++i)
    {
        lineTo(pts[i].x, pts[i].y);
    }
    return *this;
}

rive::rcp<rive::RenderImage> LoadImage(rive::Span<uint8_t> bytes)
{
    return TestingWindow::Get()->factory()->decodeImage(bytes);
}

} // namespace rivegm
