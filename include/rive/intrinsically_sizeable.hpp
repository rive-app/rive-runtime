#ifndef _RIVE_INTRINSICALLY_SIZEABLE_HPP_
#define _RIVE_INTRINSICALLY_SIZEABLE_HPP_

#include <stdint.h>
#include "rive/component.hpp"
#include "rive/layout/layout_measure_mode.hpp"
#include "rive/math/vec2d.hpp"

namespace rive
{
class IntrinsicallySizeable
{
public:
    virtual Vec2D measureLayout(float width,
                                LayoutMeasureMode widthMode,
                                float height,
                                LayoutMeasureMode heightMode)
    {
        return Vec2D();
    }

    virtual void controlSize(Vec2D size) {}
    static IntrinsicallySizeable* from(Component* component);
};
} // namespace rive
#endif