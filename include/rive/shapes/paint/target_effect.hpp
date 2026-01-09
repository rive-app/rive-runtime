#ifndef _RIVE_TARGET_EFFECT_HPP_
#define _RIVE_TARGET_EFFECT_HPP_
#include "rive/generated/shapes/paint/target_effect_base.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include <stdio.h>
namespace rive
{
class GroupEffect;
class TargetEffectPath : public EffectPath
{
public:
    PathProvider* pathProviderProxy() { return &m_pathProviderProxy; }

private:
    PathProvider m_pathProviderProxy;
};
class TargetEffect : public TargetEffectBase, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;

    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      ShapePaintType shapePaintType) override;
    ShapePaintPath* effectPath(PathProvider* pathProvider) override;
    EffectsContainer* parentPaint() override;
    void addPathProvider(PathProvider* component) override;
    void invalidateEffect(PathProvider* component) override;

protected:
    virtual EffectPath* createEffectPath() override;

private:
    GroupEffect* m_groupEffect = nullptr;
};
} // namespace rive

#endif