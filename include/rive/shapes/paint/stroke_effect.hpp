#ifndef _RIVE_STROKE_EFFECT_HPP_
#define _RIVE_STROKE_EFFECT_HPP_

#include "rive/rive_types.hpp"
#include "rive/shape_paint_type.hpp"
#include <unordered_map>

namespace rive
{
class Factory;
class RenderPath;
class RawPath;
class ShapePaint;
class ShapePaintPath;
class EffectsContainer;

// A EffectPath is a reference that any StrokeEffect holds to identify each
// ShapePaint that is being affected by it.
class EffectPath
{
public:
    virtual ~EffectPath() = default;
    virtual void invalidateEffect() {}
    virtual ShapePaintPath* path() { return nullptr; }
};

// A PathProvider is any object that will be used to map a unique EffectPath.
// It allows to disambiguate path effect updates and invalidations without
// modifying any other instance that an effect is affecting. ShapePaints are
// PathProviders, but also TargetEffects. A TargetEffect needs to create a
// PathProvider proxy because the same ShapePaint can be targetting multiple
// times the same group effect and invalidating one doesn't need to invalidate
// the other one.
class PathProvider
{};

class StrokeEffect
{
public:
    virtual ~StrokeEffect();
    virtual void updateEffect(PathProvider* pathProvider,
                              const ShapePaintPath* source,
                              const ShapePaint* shapePaint) = 0;
    virtual void invalidateEffect(PathProvider* pathProvider);
    virtual EffectsContainer* parentPaint() = 0;
    virtual void addPathProvider(PathProvider* component)
    {
        m_effectPaths[component] = createEffectPath();
    }

    virtual ShapePaintPath* effectPath(PathProvider* pathProvider);
    virtual void invalidateEffectFromLocal();

protected:
    virtual EffectPath* createEffectPath() { return new EffectPath(); }
    std::unordered_map<PathProvider*, EffectPath*> m_effectPaths;
};
} // namespace rive
#endif