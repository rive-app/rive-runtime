#ifndef _RIVE_STROKE_EFFECT_HPP_
#define _RIVE_STROKE_EFFECT_HPP_

#include "rive/rive_types.hpp"

namespace rive
{
class Factory;
class RenderPath;
class MetricsPath;

class StrokeEffect
{
public:
    virtual ~StrokeEffect() {}
    virtual RenderPath* effectPath(MetricsPath* source, Factory*) = 0;
    virtual void invalidateEffect() = 0;
};
} // namespace rive
#endif