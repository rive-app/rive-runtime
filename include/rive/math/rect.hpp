#ifndef _RIVE_RECT_HPP_
#define _RIVE_RECT_HPP_

namespace rive
{
struct Rect
{
    float left, top, right, bottom;

    Rect(float l, float t, float r, float b) :
        left(l), top(t), right(r), bottom(b)
    {}

    float width() const { return right - left; }
    float height() const { return bottom - top; }

    static Rect fromLTRB(float l, float t, float r, float b)
    {
        return Rect(l, t, r, b);
    }
};
} // namespace rive
#endif
