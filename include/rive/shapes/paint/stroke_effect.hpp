#ifndef _RIVE_STROKE_EFFECT_HPP_
#define _RIVE_STROKE_EFFECT_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
class Factory;
class RenderPath;
class RawPath;
class ShapePaintPath;

class StrokeEffect
{
public:
    virtual ~StrokeEffect() {}
    virtual void updateEffect(const ShapePaintPath* source) = 0;
    virtual ShapePaintPath* effectPath() = 0;
    virtual void invalidateEffect() = 0;
};
} // namespace rive
#endif