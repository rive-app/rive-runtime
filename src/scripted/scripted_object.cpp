#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/scripted/scripted_object.hpp"

using namespace rive;

ScriptedObject* ScriptedObject::from(Core* object)
{
    switch (object->coreType())
    {
        case ScriptedDataConverter::typeKey:
            return object->as<ScriptedDataConverter>();
        case ScriptedDrawable::typeKey:
            return object->as<ScriptedDrawable>();
        case ScriptedLayout::typeKey:
            return object->as<ScriptedLayout>();
        case ScriptedPathEffect::typeKey:
            return object->as<ScriptedPathEffect>();
    }
    return nullptr;
}

#ifdef WITH_RIVE_SCRIPTING
void ScriptedObject::setArtboardInput(std::string name, Artboard* artboard)
{
    if (m_state == nullptr || scriptAsset() == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    lua_newrive<ScriptedArtboard>(state,
                                  ref_rcp(scriptAsset()->file()),
                                  artboard->instance());
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setBooleanInput(std::string name, bool value)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    lua_pushboolean(state, value);
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setNumberInput(std::string name, float value)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    lua_pushnumber(state, value);
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setIntegerInput(std::string name, int value)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    lua_pushunsigned(state, value);
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setStringInput(std::string name, std::string value)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    lua_pushstring(state, value.c_str());
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setViewModelInput(std::string name,
                                       ViewModelInstanceValue* value)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    switch (value->coreType())
    {
        case ViewModelInstanceViewModelBase::typeKey:
        {
            auto vm = value->as<ViewModelInstanceViewModel>();
            auto vmi = vm->referenceViewModelInstance();
            if (vmi == nullptr)
            {
                fprintf(stderr,
                        "riveLuaPushViewModelInstanceValue - passed in a "
                        "ViewModelInstanceViewModel with no associated "
                        "ViewModelInstance.\n");
                return;
            }

            lua_newrive<ScriptedViewModel>(state,
                                           state,
                                           ref_rcp(vmi->viewModel()),
                                           vmi);
            break;
        }
        default:
            break;
    }
    lua_setfield(state, -2, name.c_str());
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::trigger(std::string name)
{
    if (m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    if (static_cast<lua_Type>(lua_getfield(state, -1, name.c_str())) !=
        LUA_TFUNCTION)
    {
        rive_lua_pop(state, 2);
        return;
    }
    lua_pushvalue(state, -2);
    rive_lua_pcall(state, 1, 0);
    rive_lua_pop(state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

bool ScriptedObject::scriptAdvance(float elapsedSeconds)
{
    if (!advances() || m_state == nullptr)
    {
        return false;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    if (static_cast<lua_Type>(lua_getfield(state, -1, "advance")) !=
        LUA_TFUNCTION)
    {
        rive_lua_pop(state, 2);
        return false;
    }
    lua_pushvalue(state, -2);
    lua_pushnumber(state, elapsedSeconds);
    if (static_cast<lua_Status>(rive_lua_pcall(state, 2, 1)) != LUA_OK)
    {
        rive_lua_pop(state, 2);
        return false;
    }
    bool result = lua_toboolean(state, -1);
    rive_lua_pop(state, 2);
    return result;
}

void ScriptedObject::scriptUpdate()
{
    if (!updates() || m_state == nullptr)
    {
        return;
    }
    auto state = m_state->state;
    rive_lua_pushRef(state, m_self);
    if (static_cast<lua_Type>(lua_getfield(state, -1, "update")) !=
        LUA_TFUNCTION)
    {
        rive_lua_pop(state, 2);
        return;
    }
    lua_pushvalue(state, -2);
    if (static_cast<lua_Status>(rive_lua_pcall(state, 1, 0)) != LUA_OK)
    {
        rive_lua_pop(state, 1);
    }
    rive_lua_pop(state, 1);
}

bool ScriptedObject::scriptInit(LuaState* luaState)
{
    auto state = luaState->state;
    for (auto prop : m_customProperties)
    {
        auto scriptInput = ScriptInput::from(prop);
        if (scriptInput && !scriptInput->validateForScriptInit())
        {
            rive_lua_pop(state, 1);
            return false;
        }
    }
    if (static_cast<lua_Status>(rive_lua_pcall(state, 0, 1)) != LUA_OK)
    {
        rive_lua_pop(state, 1);
        return false;
    }
    if (static_cast<lua_Type>(lua_type(state, -1)) != LUA_TTABLE)
    {
        rive_lua_pop(state, 1);
        return false;
    }
    else
    {
        lua_newrive<ScriptedContext>(state, this);
        m_context = lua_ref(state, -1);
        rive_lua_pop(state, 1);
        m_self = lua_ref(state, -1);
        m_state = luaState;
        for (auto prop : m_customProperties)
        {
            auto scriptInput = ScriptInput::from(prop);
            if (scriptInput)
            {
                scriptInput->initScriptedValue();
            }
        }
        if (inits())
        {
            if (static_cast<lua_Type>(lua_getfield(state, -1, "init")) !=
                LUA_TFUNCTION)
            {
                lua_unref(state, m_self);
                lua_unref(state, m_context);
                rive_lua_pop(state, 2);
                m_state = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            lua_pushvalue(state, -2);
            rive_lua_pushRef(state, m_context);
            auto pCallResult = rive_lua_pcall(state, 2, 1);
            if (static_cast<lua_Status>(pCallResult) != LUA_OK)
            {
                lua_unref(state, m_self);
                lua_unref(state, m_context);
                rive_lua_pop(state, 2);
                m_state = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            if (!lua_toboolean(state, -1))
            {
                lua_unref(state, m_self);
                lua_unref(state, m_context);
                // Pop boolean and self table
                rive_lua_pop(state, 2);
                m_state = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            else
            {
                rive_lua_pop(state, 1);
                assert(static_cast<lua_Type>(lua_type(state, -1)) ==
                       LUA_TTABLE);
                rive_lua_pop(state, 1);
            }
        }
        else
        {
            // Init function doesn't exist, just pop the self table
            rive_lua_pop(state, 1);
        }
    }
    return true;
}

void ScriptedObject::scriptDispose()
{
    if (m_state != nullptr)
    {
        lua_unref(m_state->state, m_self);
        lua_unref(m_state->state, m_context);
    }
    m_state = nullptr;
    m_self = 0;
}
#else
void ScriptedObject::setArtboardInput(std::string name, Artboard* artboard) {}

void ScriptedObject::setBooleanInput(std::string name, bool value) {}

void ScriptedObject::setIntegerInput(std::string name, int value) {}

void ScriptedObject::setNumberInput(std::string name, float value) {}

void ScriptedObject::setStringInput(std::string name, std::string value) {}

void ScriptedObject::setViewModelInput(std::string name,
                                       ViewModelInstanceValue* value)
{}

void ScriptedObject::trigger(std::string name) {}

bool ScriptedObject::scriptAdvance(float elapsedSeconds) { return false; }

void ScriptedObject::scriptUpdate() {}

void ScriptedObject::scriptDispose() {}
#endif

void ScriptedObject::reinit()
{
    if (scriptAsset() != nullptr)
    {
        scriptAsset()->initScriptedObject(this);
    }
}

ScriptAsset* ScriptedObject::scriptAsset() const
{
    return (ScriptAsset*)m_fileAsset.get();
}

void ScriptedObject::setAsset(rcp<FileAsset> asset)
{
    if (asset != nullptr && asset->is<ScriptAsset>())
    {
        FileAssetReferencer::setAsset(asset);
    }
}

void ScriptedObject::markNeedsUpdate() {}