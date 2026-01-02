#ifndef _RIVE_GROUP_EFFECT_HPP_
#define _RIVE_GROUP_EFFECT_HPP_
#include "rive/generated/shapes/paint/group_effect_base.hpp"
#include "rive/shapes/paint/effects_container.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include <stdio.h>
namespace rive
{
class TargetEffect;
class StrokeEffect;
class GroupEffect : public GroupEffectBase,
                    public EffectsContainer,
                    public StrokeEffect
{
public:
    void addTargetEffect(TargetEffect* effect);
    void invalidateEffects(StrokeEffect* effect) override;
    void invalidateEffects() override;
    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      ShapePaintType shapePaintType) override;
    EffectsContainer* parentPaint() override;
    void addPathProvider(PathProvider* component) override;
    void addStrokeEffect(StrokeEffect* effect) override;
    void invalidateEffect(PathProvider* effect) override;

private:
    std::vector<TargetEffect*> m_targetEffects;
};
} // namespace rive

#endif