#ifndef _RIVE_TEXT_SELECTION_PATH_HPP_
#define _RIVE_TEXT_SELECTION_PATH_HPP_

#include "rive/math/rectangles_to_contour.hpp"
#include "rive/shapes/shape_paint_path.hpp"

namespace rive
{
class TextSelectionPath : public ShapePaintPath
{
public:
    TextSelectionPath(bool isLocal = true,
                      FillRule fillRule = FillRule::clockwise) :
        ShapePaintPath(isLocal, fillRule)
    {}

    void update(Span<AABB> rects, float cornerRadius);

private:
    RectanglesToContour m_rectanglesToContour;

    void addRoundedPath(const Contour& contour, float radius, RawPath& rawPath);
};
}; // namespace rive
#endif