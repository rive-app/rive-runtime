#ifndef _RIVE_COMMAND_PATH_HPP_
#define _RIVE_COMMAND_PATH_HPP_

#include "rive/math/mat2d.hpp"
#include "rive/math/path_types.hpp"
#include "rive/refcnt.hpp"

namespace rive
{
class RenderPath;

/// Abstract path used to build up commands used for rendering.
class CommandPath : public RefCnt<CommandPath>
{
public:
    virtual ~CommandPath() {}
    virtual void rewind() = 0;
    virtual void fillRule(FillRule value) = 0;
    virtual void addPath(CommandPath* path, const Mat2D& transform) = 0;

    virtual void moveTo(float x, float y) = 0;
    virtual void lineTo(float x, float y) = 0;
    virtual void cubicTo(float ox, float oy, float ix, float iy, float x, float y) = 0;
    virtual void close() = 0;

    virtual RenderPath* renderPath() = 0;

    // non-virtual helpers

    void addRect(float x, float y, float width, float height)
    {
        moveTo(x, y);
        lineTo(x + width, y);
        lineTo(x + width, y + height);
        lineTo(x, y + height);
        close();
    }

    void move(Vec2D v) { this->moveTo(v.x, v.y); }
    void line(Vec2D v) { this->lineTo(v.x, v.y); }
    void cubic(Vec2D a, Vec2D b, Vec2D c) { this->cubicTo(a.x, a.y, b.x, b.y, c.x, c.y); }
};
} // namespace rive
#endif
