#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/shapes/paint/stroke.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedPathEffect::scriptInit(LuaState* state)
{
    ScriptedObject::scriptInit(state);
    addScriptedDirt(ComponentDirt::Paint, true);
    return true;
}

void ScriptedPathEffect::updateEffect(const ShapePaintPath* source,
                                      ShapePaintType shapePaintType)
{
    if (m_path.hasRenderPath())
    {
        // Previous result hasn't been invalidated, it's still good.
        return;
    }

    m_path.rewind(source->isLocal(), source->fillRule());
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
            auto rawPath = m_path.mutableRawPath();
            rawPath->addPath(scriptedPath->rawPath);
        }
        rive_lua_pop(state, 1);
    }
    rive_lua_pop(state, 1);
}
#else
void ScriptedPathEffect::updateEffect(const ShapePaintPath* source,
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
    if (!parent()->is<ShapePaint>())
    {
        return StatusCode::InvalidObject;
    }

    parent()->as<ShapePaint>()->addStrokeEffect(this);

    return StatusCode::Ok;
}

bool ScriptedPathEffect::advanceComponent(float elapsedSeconds,
                                          AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    return scriptAdvance(elapsedSeconds);
}

bool ScriptedPathEffect::addScriptedDirt(ComponentDirt value, bool recurse)
{
    if (parent() != nullptr)
    {
        auto stroke = parent()->as<ShapePaint>();
        stroke->parent()->addDirt(ComponentDirt::Paint);
        stroke->invalidateEffects(this);
    }
    return Component::addDirt(value, recurse);
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

ShapePaintPath* ScriptedPathEffect::effectPath() { return &m_path; }

Core* ScriptedPathEffect::clone() const
{
    ScriptedPathEffect* twin =
        ScriptedPathEffectBase::clone()->as<ScriptedPathEffect>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    for (auto prop : m_customProperties)
    {
        auto clonedValue = prop->clone()->as<CustomProperty>();
        twin->addProperty(clonedValue);
        auto scriptInput = ScriptInput::from(clonedValue);
        if (scriptInput != nullptr)
        {
            scriptInput->scriptedObject(twin);
        }
    }
    return twin;
}

void ScriptedPathEffect::invalidateEffect()
{
    m_path.rewind();
    addScriptedDirt(ComponentDirt::Paint, true);
}

void ScriptedPathEffect::markNeedsUpdate()
{
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}