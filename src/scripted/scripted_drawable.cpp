#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/component_dirt.hpp"
#include "rive/scripted/scripted_drawable.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING
bool ScriptedDrawable::scriptInit(LuaState* state)
{
    ScriptedObject::scriptInit(state);
    addDirt(ComponentDirt::Paint);
    return true;
}

void ScriptedDrawable::draw(Renderer* renderer)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    renderer->save();
    renderer->transform(worldTransform());

    rive_lua_pushRef(state, m_self);
    if (static_cast<lua_Type>(lua_getfield(state, -1, "draw")) != LUA_TFUNCTION)
    {
        fprintf(stderr, "expected draw to be a function\n");
    }
    else
    {
        lua_pushvalue(state, -2);
        auto scriptedRenderer = lua_newrive<ScriptedRenderer>(state, renderer);
        if (static_cast<lua_Status>(rive_lua_pcall(state, 2, 0)) != LUA_OK)
        {
            rive_lua_pop(state, 1);
        }
        scriptedRenderer->end();
    }
    rive_lua_pop(state, 1);
    renderer->restore();
}
#else
void ScriptedDrawable::draw(Renderer* renderer) {}
#endif

void ScriptedDrawable::update(ComponentDirt value)
{
    Super::update(value);
    if ((value & ComponentDirt::ScriptUpdate) == ComponentDirt::ScriptUpdate)
    {
        scriptUpdate();
    }
}

Core* ScriptedDrawable::hitTest(HitInfo*, const Mat2D&) { return nullptr; }

StatusCode ScriptedDrawable::onAddedDirty(CoreContext* context)
{
    auto code = Super::onAddedDirty(context);
    if (code != StatusCode::Ok)
    {
        return code;
    }
    artboard()->addScriptedObject(this);
    return StatusCode::Ok;
}

bool ScriptedDrawable::advanceComponent(float elapsedSeconds,
                                        AdvanceFlags flags)
{
    if (elapsedSeconds == 0)
    {
        return false;
    }
    return scriptAdvance(elapsedSeconds);
}

void ScriptedDrawable::addChild(Component* child)
{
    Drawable::addChild(child);
    addPropertyChild(child);
}

bool ScriptedDrawable::addScriptedDirt(ComponentDirt value, bool recurse)
{
    return Drawable::addDirt(value, recurse);
}

StatusCode ScriptedDrawable::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

Core* ScriptedDrawable::clone() const
{
    ScriptedDrawable* twin =
        ScriptedDrawableBase::clone()->as<ScriptedDrawable>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}