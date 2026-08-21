#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

void ScriptedEffectPath::invalidateEffect() { m_path.rewind(); }

#ifdef WITH_RIVE_SCRIPTING

void ScriptedPathEffect::didHydrateScriptInputs()
{
    m_isAdvanceActive = true;
    addScriptedDirt(ComponentDirt::Paint, true);
}

void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      const ShapePaint* shapePaint)
{
    if (!updates())
    {
        return;
    }

    auto effectPathIt = m_effectPaths.find(pathProvider);
    if (effectPathIt != m_effectPaths.end())
    {
        auto trimEffectPath =
            static_cast<ScriptedEffectPath*>(effectPathIt->second);
        auto path = trimEffectPath->path();
        if (path->hasRenderPath())
        {
            // Previous result hasn't been invalidated, it's still good.
            return;
        }

        path->rewind(source->isLocal(), source->fillRule());
        if (m_vm == nullptr || !m_vm->valid())
        {
            return;
        }
        setInUpdatePhase(true);
        RawPath effectPath;
        if (!m_vm->callPathEffectUpdate(this,
                                        m_self,
                                        *source->rawPath(),
                                        shapePaint,
                                        &effectPath))
        {
            fprintf(stderr, "update function failed\n");
        }
        else
        {
            // Through ShapePaintPath, which prunes what the script returned.
            path->addPath(effectPath);
        }
        setInUpdatePhase(false);
    }
}
#else
void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      const ShapePaint* shapePaint)
{}
#endif

StatusCode ScriptedPathEffect::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->addScriptedObject(this);
    return StatusCode::Ok;
}

StatusCode ScriptedPathEffect::onAddedClean(CoreContext* context)
{
    auto effectsContainer = EffectsContainer::from(parent());
    if (!effectsContainer)
    {
        return StatusCode::InvalidObject;
    }
    effectsContainer->addStrokeEffect(this);

    return StatusCode::Ok;
}

bool ScriptedPathEffect::advanceComponent(float elapsedSeconds,
                                          AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    if (!m_isAdvanceActive)
    {
        return false;
    }
    m_isAdvanceActive = false;
    if (!enums::is_flag_set(flags, AdvanceFlags::AdvanceNested))
    {
        elapsedSeconds = 0;
    }
    auto advanced = scriptAdvance(elapsedSeconds);
    if (advanced)
    {
        m_isAdvanceActive = true;
    }
    return advanced;
}

bool ScriptedPathEffect::addScriptedDirt(ComponentDirt value, bool recurse)
{
    return Component::addDirt(value, recurse);
}

void ScriptedPathEffect::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedPathEffect::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedPathEffect::clone() const
{
    ScriptedPathEffect* twin =
        ScriptedPathEffectBase::clone()->as<ScriptedPathEffect>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

void ScriptedPathEffect::markNeedsUpdate()
{
    if (inUpdatePhase())
    {
        return;
    }
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

EffectsContainer* ScriptedPathEffect::parentPaint()
{
    return EffectsContainer::from(parent());
}

EffectPath* ScriptedPathEffect::createEffectPath()
{
    return new ScriptedEffectPath();
}

void ScriptedPathEffect::buildDependencies()
{
    Super::buildDependencies();
    if (parent())
    {
        parent()->addDependent(this);
    }
}

void ScriptedPathEffect::update(ComponentDirt value)
{
    Super::update(value);

    if (hasDirt(value, ComponentDirt::ScriptUpdate))
    {
        invalidateEffectFromLocal();
        m_isAdvanceActive = true;
    }
}