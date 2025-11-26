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
    auto parentShapePaint = parentPaint();
    if (parentShapePaint != nullptr)
    {
        parentShapePaint->invalidateEffects(this);
    }
    invalidateEffect();
}

// This is called from outside StrokeEffect.
// This does the actual invalidation work.
// This should NOT be called from inside StrokeEffect
// directly (it is called indirectly via invalidateEffectFromLocal)
void StrokeEffect::invalidateEffect()
{
    auto parentShapePaint = parentPaint();
    if (parentShapePaint != nullptr)
    {
        parentShapePaint->parent()->addDirt(ComponentDirt::Paint);
    }
}