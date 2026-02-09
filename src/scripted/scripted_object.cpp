#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/script_input_artboard.hpp"
#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/scripted_transition_condition.hpp"
#include "rive/scripted/scripted_data_converter.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"
#include "rive/scripted/scripted_path_effect.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/data_bind/data_bind.hpp"

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
        case ScriptedListenerAction::typeKey:
            return object->as<ScriptedListenerAction>();
        case ScriptedTransitionCondition::typeKey:
            return object->as<ScriptedTransitionCondition>();
    }
    return nullptr;
}

#ifdef WITH_RIVE_SCRIPTING
void ScriptedObject::setArtboardInput(std::string name, Artboard* artboard)
{
    lua_State* L = state();
    if (L == nullptr || scriptAsset() == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    auto artboardInstance = artboard->instance();
    artboardInstance->frameOrigin(false);
    lua_newrive<ScriptedArtboard>(L,
                                  L,
                                  ref_rcp(scriptAsset()->file()),
                                  std::move(artboardInstance),
                                  nullptr,
                                  dataContext());
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setBooleanInput(std::string name, bool value)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    lua_pushboolean(L, value);
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setNumberInput(std::string name, float value)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    lua_pushnumber(L, value);
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setIntegerInput(std::string name, int value)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    lua_pushunsigned(L, value);
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setStringInput(std::string name, std::string value)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    lua_pushstring(L, value.c_str());
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::setViewModelInput(std::string name,
                                       ViewModelInstanceValue* value)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    switch (value->coreType())
    {
        case ViewModelInstanceViewModelBase::typeKey:
        {
            auto viewModel = value->as<ViewModelInstanceViewModel>();
            auto vmi = viewModel->referenceViewModelInstance();
            if (vmi == nullptr)
            {
                fprintf(stderr,
                        "riveLuaPushViewModelInstanceValue - passed in a "
                        "ViewModelInstanceViewModel with no associated "
                        "ViewModelInstance.\n");
                return;
            }

            lua_newrive<ScriptedViewModel>(L,
                                           L,
                                           ref_rcp(vmi->viewModel()),
                                           vmi);
            break;
        }
        default:
            break;
    }
    lua_setfield(L, -2, name.c_str());
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

void ScriptedObject::trigger(std::string name)
{
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    rive_lua_pushRef(L, m_self);
    if (static_cast<lua_Type>(lua_getfield(L, -1, name.c_str())) !=
        LUA_TFUNCTION)
    {
        rive_lua_pop(L, 2);
        return;
    }
    lua_pushvalue(L, -2);
    rive_lua_pcall(L, 1, 0);
    rive_lua_pop(L, 1);
    addScriptedDirt(ComponentDirt::ScriptUpdate);
}

bool ScriptedObject::scriptAdvance(float elapsedSeconds)
{
    lua_State* L = state();
    if (!advances() || L == nullptr)
    {
        return false;
    }
    rive_lua_pushRef(L, m_self);
    lua_getfield(L, -1, "advance");
    lua_pushvalue(L, -2);
    lua_pushnumber(L, elapsedSeconds);
    if (static_cast<lua_Status>(rive_lua_pcall(L, 2, 1)) != LUA_OK)
    {
        rive_lua_pop(L, 2);
        return false;
    }
    bool result = lua_toboolean(L, -1);
    rive_lua_pop(L, 2);
    return result;
}

void ScriptedObject::scriptUpdate()
{
    lua_State* L = state();
    if (!updates() || L == nullptr)
    {
        return;
    }
    // Stack: []
    rive_lua_pushRef(L, m_self);
    // Stack: [self]
    lua_getfield(L, -1, "update");
    // Stack: [self, field] Swap self and field
    lua_insert(L, -2);
    // Stack: [field, self]
    if (static_cast<lua_Status>(rive_lua_pcall(L, 1, 0)) != LUA_OK)
    {
        rive_lua_pop(L, 1);
    }
}

bool ScriptedObject::scriptInit(ScriptingVM* vm)
{
    lua_State* L = vm ? vm->state() : nullptr;
    // Clean up old references if reinitializing
    if (m_vm != nullptr)
    {
        lua_State* oldState = state();
        if (m_self != 0)
        {
            lua_unref(oldState, m_self);
            m_self = 0;
        }
        if (m_context != 0)
        {
            lua_unref(oldState, m_context);
            m_context = 0;
        }
    }
    for (auto prop : m_customProperties)
    {
        auto scriptInput = ScriptInput::from(prop);
        if (scriptInput && !scriptInput->validateForScriptInit())
        {
            rive_lua_pop(L, 1);
            return false;
        }
    }
    if (static_cast<lua_Status>(rive_lua_pcall(L, 0, 1)) != LUA_OK)
    {
        rive_lua_pop(L, 1);
        return false;
    }
    if (static_cast<lua_Type>(lua_type(L, -1)) != LUA_TTABLE)
    {
        rive_lua_pop(L, 1);
        return false;
    }
    else
    {
        // Stack: [self]
        lua_newrive<ScriptedContext>(L, this);
        // Stack: [self, ScriptedContext]
        m_context = lua_ref(L, -1);
        rive_lua_pop(L, 1);
        // Stack: [self]
        m_self = lua_ref(L, -1);
#ifdef WITH_RIVE_TOOLS
        m_vm = ref_rcp(vm); // Increment refcount for shared ownership
#else
        m_vm = vm;
#endif
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
            lua_getfield(L, -1, "init");
            // Stack: [self, field]
            lua_pushvalue(L, -2);
            // Stack: [self, field, self]
            rive_lua_pushRef(L, m_context);
            // Stack: [self, field, self, ScriptedContext]
            auto pCallResult = rive_lua_pcall(L, 2, 1);
            if (static_cast<lua_Status>(pCallResult) != LUA_OK)
            {
                lua_unref(L, m_self);
                lua_unref(L, m_context);
                // Stack: [self, status]
                rive_lua_pop(L, 2);
                m_vm = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            if (!lua_toboolean(L, -1))
            {
                lua_unref(L, m_self);
                lua_unref(L, m_context);
                // Pop boolean and self table
                // Stack: [self, result]
                rive_lua_pop(L, 2);
                m_vm = nullptr;
                m_self = 0;
                m_context = 0;
                return false;
            }
            else
            {
                // Stack: [self, result]
                rive_lua_pop(L, 1);
                assert(static_cast<lua_Type>(lua_type(L, -1)) == LUA_TTABLE);
                // Stack: [self]
                rive_lua_pop(L, 1);
            }
        }
        else
        {
            // Init function doesn't exist, just pop the self table
            // Stack: [self]
            rive_lua_pop(L, 1);
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

    lua_State* L = state();
    if (L != nullptr)
    {
        lua_unref(L, m_self);
        lua_unref(L, m_context);
#ifdef TESTING
        // Force GC to collect any ScriptedArtboard instances created via
        // instance()
        lua_gc(L, LUA_GCCOLLECT, 0);
#endif
    }
    m_vm = nullptr;
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

void ScriptedObject::cloneProperties(CustomPropertyContainer* twin,
                                     DataBindContainer* dataBindContainer) const
{

    for (auto prop : m_customProperties)
    {
        auto clonedValue = prop->clone()->as<CustomProperty>();
        twin->addProperty(clonedValue);
        auto input = ScriptInput::from(prop);
        if (input != nullptr)
        {
            auto dataBind = input->dataBind();
            if (dataBind)
            {
                auto dataBindClone = static_cast<DataBind*>(dataBind->clone());
                dataBindClone->file(dataBind->file());
                if (dataBind->converter() != nullptr)
                {
                    dataBindClone->converter(
                        dataBind->converter()->clone()->as<DataConverter>());
                }
                dataBindClone->target(clonedValue);
                dataBindContainer->addDataBind(dataBindClone);
            }
        }
    }
}