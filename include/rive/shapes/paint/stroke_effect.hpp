#ifndef _RIVE_STROKE_EFFECT_HPP_
#define _RIVE_STROKE_EFFECT_HPP_

#include "rive/rive_types.hpp"
#include "rive/shape_paint_type.hpp"

namespace rive
{
class Factory;
class RenderPath;
class RawPath;
class ShapePaint;
class ShapePaintPath;

class StrokeEffect
{
public:
    virtual ~StrokeEffect() {}
    virtual void updateEffect(const ShapePaintPath* source,
                              ShapePaintType shapePaintType) = 0;
    virtual ShapePaintPath* effectPath() = 0;
    virtual void invalidateEffect();
    virtual ShapePaint* parentPaint() = 0;

protected:
    virtual void invalidateEffectFromLocal();
};
} // namespace rive
#endif