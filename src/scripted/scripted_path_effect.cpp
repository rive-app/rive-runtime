#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

void ScriptedEffectPath::invalidateEffect() { m_path.rewind(); }

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedPathEffect::scriptInit(LuaState* state)
{
    ScriptedObject::scriptInit(state);
    addScriptedDirt(ComponentDirt::Paint, true);
    return true;
}

void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      ShapePaintType shapePaintType)
{
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
        if (m_state == nullptr)
        {
            return;
        }
        auto state = m_state->state;
        rive_lua_pushRef(state, m_self);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "update")) !=
            LUA_TFUNCTION)
        {
            fprintf(stderr, "expected update to be a function\n");
        }
        else
        {
            lua_pushvalue(state, -2);
            lua_newrive<ScriptedPathData>(state, source->rawPath());
            lua_pushstring(state,
                           shapePaintType == ShapePaintType::stroke ? "stroke"
                                                                    : "fill");
            if (static_cast<lua_Status>(rive_lua_pcall(state, 3, 1)) != LUA_OK)
            {
                fprintf(stderr, "update function failed\n");
            }
            else
            {
                auto scriptedPath = (ScriptedPath*)lua_touserdata(state, -1);
                auto rawPath = path->mutableRawPath();
                rawPath->addPath(scriptedPath->rawPath);
            }
            rive_lua_pop(state, 1);
        }
        rive_lua_pop(state, 1);
    }
}
#else
void ScriptedPathEffect::updateEffect(PathProvider* pathProvider,
                                      const ShapePaintPath* source,
                                      ShapePaintType shapePaintType)
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
    if ((flags & AdvanceFlags::AdvanceNested) == 0)
    {
        elapsedSeconds = 0;
    }
    return scriptAdvance(elapsedSeconds);
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
    }
}