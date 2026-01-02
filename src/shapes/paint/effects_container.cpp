#include "rive/shapes/paint/effects_container.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/stroke.hpp"
#include "rive/shapes/paint/group_effect.hpp"

using namespace rive;

void EffectsContainer::addStrokeEffect(StrokeEffect* effect)
{
    m_effects.push_back(effect);
}

EffectsContainer* EffectsContainer::from(Component* component)
{
    if (component)
    {
        switch (component->coreType())
        {
            case ShapePaintBase::typeKey:
            case FillBase::typeKey:
            case StrokeBase::typeKey:
                return component->as<ShapePaint>();
            case GroupEffectBase::typeKey:
                return component->as<GroupEffect>();
        }
    }
    return nullptr;
}

void EffectsContainer::invalidateEffects(StrokeEffect* invalidatingEffect)
{
    auto found = invalidatingEffect == nullptr;
    for (auto& effect : m_effects)
    {
        if (found)
        {
            effect->invalidateEffect(nullptr);
        }
        if (invalidatingEffect && invalidatingEffect == effect)
        {
            found = true;
        }
    }
}

ShapePaintPath* EffectsContainer::lastEffectPath(PathProvider* pathProvider)
{
    if (m_effects.size() > 0)
    {
        size_t effectIndex = m_effects.size() - 1;
        while (effectIndex >= 0)
        {
            auto effect = m_effects[effectIndex]->effectPath(pathProvider);
            if (effect)
            {
                return effect;
            }
            if (effectIndex == 0)
            {
                break;
            }
            effectIndex -= 1;
        }
    }
    return nullptr;
}

void EffectsContainer::invalidateEffects() { invalidateEffects(nullptr); }