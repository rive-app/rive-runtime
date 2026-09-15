#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/target_effect.hpp"
#include "rive/artboard.hpp"
#include "rive/shapes/paint/group_effect.hpp"

using namespace rive;

StatusCode TargetEffect::onAddedClean(CoreContext* context)
{
    auto effectsContainer = EffectsContainer::from(parent());
    if (!effectsContainer)
    {
        return StatusCode::InvalidObject;
    }
    effectsContainer->addStrokeEffect(this);

    auto groupTarget = context->resolve(targetId());
    if (groupTarget == nullptr || !groupTarget->is<GroupEffect>())
    {
        return StatusCode::MissingObject;
    }
#ifdef WITH_RIVE_EDITOR
    setGroupEffectForEditor(groupTarget->as<GroupEffect>());
#else
    m_groupEffect = groupTarget->as<GroupEffect>();
#endif
    auto* g = groupEffect();
    g->addTargetEffect(this);
    for (auto& effectPath : m_effectPaths)
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPath.second);
        g->addPathProvider(targetEffectPath->pathProviderProxy());
    }

    return StatusCode::Ok;
}

void TargetEffect::updateEffect(PathProvider* pathProvider,
                                const ShapePaintPath* source,
                                const ShapePaint* shapePaint)
{
    auto* g = groupEffect();
    if (!g)
    {
        return;
    }
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPathIt->second);
        g->updateEffect(targetEffectPath->pathProviderProxy(),
                        source,
                        shapePaint);
    }
}

ShapePaintPath* TargetEffect::effectPath(PathProvider* pathProvider)
{
    auto* g = groupEffect();
    if (!g)
    {
        return nullptr;
    }
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPathIt->second);
        return g->lastEffectPath(targetEffectPath->pathProviderProxy());
    }
    return nullptr;
}

void TargetEffect::addPathProvider(PathProvider* component)
{
    StrokeEffect::addPathProvider(component);

    auto effectPathIt = m_effectPaths.find(component);
    if (effectPathIt != m_effectPaths.end())
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPathIt->second);
        if (auto* g = groupEffect())
        {
            g->addPathProvider(targetEffectPath->pathProviderProxy());
        }
    }
}

EffectsContainer* TargetEffect::parentPaint()
{
    return EffectsContainer::from(parent());
}

EffectPath* TargetEffect::createEffectPath() { return new TargetEffectPath(); }

void TargetEffect::invalidateEffect(PathProvider* pathProvider)
{
    auto* g = groupEffect();
    if (!g)
    {
        return;
    }
    if (pathProvider)
    {
        auto effectPathIt = m_effectPaths.find(pathProvider);
        if (effectPathIt != m_effectPaths.end())
        {
            auto targetEffectPath =
                static_cast<TargetEffectPath*>(effectPathIt->second);
            g->invalidateEffect(targetEffectPath->pathProviderProxy());
        }
    }
    else
    {
        for (auto& effectPath : m_effectPaths)
        {
            auto targetEffectPath =
                static_cast<TargetEffectPath*>(effectPath.second);
            g->invalidateEffect(targetEffectPath->pathProviderProxy());
        }
    }
}
