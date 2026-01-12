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
    m_groupEffect = groupTarget->as<GroupEffect>();
    m_groupEffect->addTargetEffect(this);
    for (auto& effectPath : m_effectPaths)
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPath.second);
        m_groupEffect->addPathProvider(targetEffectPath->pathProviderProxy());
    }

    return StatusCode::Ok;
}

void TargetEffect::updateEffect(PathProvider* pathProvider,
                                const ShapePaintPath* source,
                                const ShapePaint* shapePaint)
{
    if (!m_groupEffect)
    {
        return;
    }
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPathIt->second);
        m_groupEffect->updateEffect(targetEffectPath->pathProviderProxy(),
                                    source,
                                    shapePaint);
    }
}

ShapePaintPath* TargetEffect::effectPath(PathProvider* pathProvider)
{
    if (!m_groupEffect)
    {
        return nullptr;
    }
    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto targetEffectPath =
            static_cast<TargetEffectPath*>(effectPathIt->second);
        return m_groupEffect->lastEffectPath(
            targetEffectPath->pathProviderProxy());
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
        if (m_groupEffect)
        {
            m_groupEffect->addPathProvider(
                targetEffectPath->pathProviderProxy());
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
    if (pathProvider)
    {
        auto effectPathIt = m_effectPaths.find(pathProvider);
        if (effectPathIt != m_effectPaths.end())
        {
            auto targetEffectPath =
                static_cast<TargetEffectPath*>(effectPathIt->second);
            m_groupEffect->invalidateEffect(
                targetEffectPath->pathProviderProxy());
        }
    }
    else
    {
        for (auto& effectPath : m_effectPaths)
        {
            auto targetEffectPath =
                static_cast<TargetEffectPath*>(effectPath.second);
            m_groupEffect->invalidateEffect(
                targetEffectPath->pathProviderProxy());
        }
    }
}