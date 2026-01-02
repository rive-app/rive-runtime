#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"

using namespace rive;

// This is only called from within a StrokeEffect.
// Whenever a StrokeEffect needs to invalidate ITSELF
// this method should be used (not invalidateEffect).
// It calls invalidateEffects on its parent ShapePaint
// which can result in invalidating other StrokeEffects.
// It also calls invalidateEffect() to do the invalidation
// work on itself
void StrokeEffect::invalidateEffectFromLocal()
{
    for (auto& effectPath : m_effectPaths)
    {
        effectPath.second->invalidateEffect();
    }
    auto parentShapePaint = parentPaint();
    if (parentShapePaint != nullptr)
    {
        parentShapePaint->invalidateEffects(this);
    }
}

// This is called from outside StrokeEffect.
// This does the actual invalidation work.
// This should NOT be called from inside StrokeEffect
// directly (it is called indirectly via invalidateEffectFromLocal)
void StrokeEffect::invalidateEffect(PathProvider* pathProvider)
{

    if (pathProvider)
    {
        auto effectPathIt = m_effectPaths.find(pathProvider);
        if (effectPathIt != m_effectPaths.end())
        {
            effectPathIt->second->invalidateEffect();
        }
    }
    else
    {
        for (auto& effectPath : m_effectPaths)
        {
            effectPath.second->invalidateEffect();
        }
    }
}

StrokeEffect::~StrokeEffect()
{
    for (auto& effectPath : m_effectPaths)
    {
        delete effectPath.second;
    }
}

ShapePaintPath* StrokeEffect::effectPath(PathProvider* pathProvider)
{
    auto it = m_effectPaths.find(pathProvider);
    if (it != m_effectPaths.end())
    {
        return it->second->path();
    }
    return nullptr;
}