#include "rive/shapes/paint/group_effect.hpp"
#include "rive/shapes/paint/target_effect.hpp"

using namespace rive;

void GroupEffect::updateEffect(PathProvider* pathProvider,
                               const ShapePaintPath* source,
                               ShapePaintType shapePaintType)
{
    auto path = source;
    for (auto& effect : *effects())
    {
        effect->updateEffect(pathProvider, path, shapePaintType);
        auto newPath = effect->effectPath(pathProvider);
        if (newPath)
        {
            path = newPath;
        }
    }
}

void GroupEffect::invalidateEffects(StrokeEffect* invalidatingEffect)
{
    for (auto& targetEffect : m_targetEffects)
    {
        targetEffect->invalidateEffectFromLocal();
    }
    EffectsContainer::invalidateEffects(invalidatingEffect);
}

void GroupEffect::invalidateEffects() { invalidateEffects(nullptr); }

void GroupEffect::addTargetEffect(TargetEffect* targetEffect)
{
    m_targetEffects.push_back(targetEffect);
}

EffectsContainer* GroupEffect::parentPaint() { return nullptr; }

void GroupEffect::addPathProvider(PathProvider* component)
{
    StrokeEffect::addPathProvider(component);
    for (auto& effect : *effects())
    {
        effect->addPathProvider(component);
    }
}

void GroupEffect::addStrokeEffect(StrokeEffect* effect)
{
    EffectsContainer::addStrokeEffect(effect);
    for (auto& effectPath : m_effectPaths)
    {
        effect->addPathProvider(effectPath.first);
    }
}
void GroupEffect::invalidateEffect(PathProvider* component)
{
    StrokeEffect::invalidateEffect(component);
    for (auto& effect : *effects())
    {
        effect->invalidateEffect(component);
    }
}