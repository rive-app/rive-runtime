#ifndef _RIVE_EFFECTS_CONTAINER_HPP_
#define _RIVE_EFFECTS_CONTAINER_HPP_
#include "rive/refcnt.hpp"
#include "rive/shapes/path_flags.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/math/mat2d.hpp"
#include <vector>

namespace rive
{
class StrokeEffect;
class PathProvider;

class EffectsContainer
{

public:
    static EffectsContainer* from(Component* component);
    virtual void addStrokeEffect(StrokeEffect* effect);
    virtual void invalidateEffects(StrokeEffect* invalidatingEffect);
    virtual void invalidateEffects();
    ShapePaintPath* lastEffectPath(PathProvider*);
#ifdef TESTING
    StrokeEffect* effect()
    {
        return m_effects.size() > 0 ? m_effects.back() : nullptr;
    }
#endif

protected:
    std::vector<StrokeEffect*>* effects() { return &m_effects; }

private:
    std::vector<StrokeEffect*> m_effects;
};
} // namespace rive
#endif
