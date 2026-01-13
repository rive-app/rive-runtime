#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/script_input_artboard.hpp"
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

    rive_lua_pushRef(m_state, m_self);
    auto artboardInstance = artboard->instance();
    artboardInstance->frameOrigin(false);
    lua_newrive<ScriptedArtboard>(m_state,
                                  m_state,
                                  ref_rcp(scriptAsset()->file()),
                                  std::move(artboardInstance));
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setBooleanInput(std::string name, bool value)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    lua_pushboolean(m_state, value);
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setNumberInput(std::string name, float value)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    lua_pushnumber(m_state, value);
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setIntegerInput(std::string name, int value)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    lua_pushunsigned(m_state, value);
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setStringInput(std::string name, std::string value)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    lua_pushstring(m_state, value.c_str());
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setViewModelInput(std::string name,
                                       ViewModelInstanceValue* value)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
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

            lua_newrive<ScriptedViewModel>(m_state,
                                           m_state,
                                           ref_rcp(vmi->viewModel()),
                                           vmi);
            break;
        }
        default:
            break;
    }
    lua_setfield(m_state, -2, name.c_str());
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::trigger(std::string name)
{
    if (m_state == nullptr)
    {
        return;
    }
    rive_lua_pushRef(m_state, m_self);
    if (static_cast<lua_Type>(lua_getfield(m_state, -1, name.c_str())) !=
        LUA_TFUNCTION)
    {
        rive_lua_pop(m_state, 2);
        return;
    }
    lua_pushvalue(m_state, -2);
    rive_lua_pcall(m_state, 1, 0);
    rive_lua_pop(m_state, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

#ifdef WITH_RIVE_TOOLS
bool ScriptedObject::hasValidVM()
{
#ifdef WITH_RIVE_SCRIPTING
    if (!m_state)
    {
        return false;
    }
    if (asset())
    {
        auto scriptAsset = asset()->as<ScriptAsset>();
        if (!scriptAsset->file() || !scriptAsset->file()->hasVM())
        {
            m_state = nullptr;
            return false;
        }
    }
#endif
    return true;
}
#endif

bool ScriptedObject::scriptAdvance(float elapsedSeconds)
{
    if (!advances() || m_state == nullptr)
    {
        return false;
    }
#ifdef WITH_RIVE_TOOLS
    if (!hasValidVM())
    {
        return false;
    }
#endif
    rive_lua_pushRef(m_state, m_self);
    lua_getfield(m_state, -1, "advance");
    lua_pushvalue(m_state, -2);
    lua_pushnumber(m_state, elapsedSeconds);
    if (static_cast<lua_Status>(rive_lua_pcall(m_state, 2, 1)) != LUA_OK)
    {
        rive_lua_pop(m_state, 2);
        return false;
    }
    bool result = lua_toboolean(m_state, -1);
    rive_lua_pop(m_state, 2);
    return result;
}

void ScriptedObject::scriptUpdate()
{
    if (!updates() || m_state == nullptr)
    {
        return;
    }

#ifdef WITH_RIVE_TOOLS
    if (!hasValidVM())
    {
        return;
    }
#endif
    // Stack: []
    rive_lua_pushRef(m_state, m_self);
    // Stack: [self]
    lua_getfield(m_state, -1, "update");
    // Stack: [self, field] Swap self and field
    lua_insert(m_state, -2);
    // Stack: [field, self]
    if (static_cast<lua_Status>(rive_lua_pcall(m_state, 1, 0)) != LUA_OK)
    {
        rive_lua_pop(m_state, 1);
    }
}

bool ScriptedObject::scriptInit(lua_State* state)
{
    // Clean up old references if reinitializing
    if (m_state != nullptr && (m_self != 0 || m_context != 0))
    {
        if (m_self != 0)
        {
            lua_unref(m_state, m_self);
            m_self = 0;
        }
        if (m_context != 0)
        {
            lua_unref(m_state, m_context);
            m_context = 0;
        }
    }
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
        // Stack: [self]
        lua_newrive<ScriptedContext>(state, this);
        // Stack: [self, ScriptedContext]
        m_context = lua_ref(state, -1);
        rive_lua_pop(state, 1);
        // Stack: [self]
        m_self = lua_ref(state, -1);
        m_state = state;
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
            // Stack: [self]
            lua_getfield(state, -1, "init");
            // Stack: [self, field]
            lua_pushvalue(state, -2);
            // Stack: [self, field, self]
            rive_lua_pushRef(state, m_context);
            // Stack: [self, field, self, ScriptedContext]
            auto pCallResult = rive_lua_pcall(state, 2, 1);
            if (static_cast<lua_Status>(pCallResult) != LUA_OK)
            {
                lua_unref(state, m_self);
                lua_unref(state, m_context);
                // Stack: [self, status]
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
                // Stack: [self, result]
                rive_lua_pop(state, 2);
                m_state = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            else
            {
                // Stack: [self, result]
                rive_lua_pop(state, 1);
                assert(static_cast<lua_Type>(lua_type(state, -1)) ==
                       LUA_TTABLE);
                // Stack: [self]
                rive_lua_pop(state, 1);
            }
        }
        else
        {
            // Init function doesn't exist, just pop the self table
            // Stack: [self]
            rive_lua_pop(state, 1);
        }
    }
    return true;
}

void ScriptedObject::disposeScriptInputs()
{
    for (auto prop : m_customProperties)
    {
        auto scriptInput = ScriptInput::from(prop);
        if (scriptInput != nullptr)
        {
            scriptInput->scriptedObject(nullptr);
        }
    }
    m_customProperties.clear();
}

void ScriptedObject::scriptDispose()
{
    disposeScriptInputs();

    if (m_state != nullptr)
    {
        lua_unref(m_state, m_self);
        lua_unref(m_state, m_context);
#ifdef TESTING
        // Force GC to collect any ScriptedArtboard instances created via
        // instance()
        lua_gc(m_state, LUA_GCCOLLECT, 0);
#endif
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

void ScriptedObject::disposeScriptInputs() {}
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