#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/data_bind/data_bind.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif

using namespace rive;

ScriptedListenerAction::~ScriptedListenerAction() { disposeScriptInputs(); }

void ScriptedListenerAction::disposeScriptInputs()
{
    auto props = m_customProperties;
    ScriptedObject::disposeScriptInputs();
    for (auto prop : props)
    {
        auto scriptInput = ScriptInput::from(prop);
        if (scriptInput != nullptr)
        {
            // ScriptedDataConverters need to delete their own inputs
            // because they are not components
            delete scriptInput;
        }
    }
}

// Note: performStateful is the actual instance of the ScriptedListenerAction
// that will run the script. perform itself will look for the map between the
// stateless and the stateful instances of this class.
void ScriptedListenerAction::performStateful(
    StateMachineInstance* stateMachineInstance,
    Vec2D position,
    Vec2D previousPosition,
    int pointerId) const
{
#ifdef WITH_RIVE_SCRIPTING
    lua_State* L = state();
    if (L == nullptr)
    {
        return;
    }
    // Stack: []
    rive_lua_pushRef(L, m_self);
    // Stack: [self]
    lua_getfield(L, -1, "perform");

    // Stack: [self, field]
    lua_pushvalue(L, -2);

    // Stack: [self, field, self]
    lua_newrive<ScriptedPointerEvent>(L, pointerId, position);

    // Stack: [self, field, self, pointerEvent]
    if (static_cast<lua_Status>(rive_lua_pcall(L, 2, 0)) == LUA_OK)
    {
        rive_lua_pop(L, 1);
    }
    else
    {
        rive_lua_pop(L, 2);
    }
#endif
}

void ScriptedListenerAction::perform(StateMachineInstance* stateMachineInstance,
                                     Vec2D position,
                                     Vec2D previousPosition,
                                     int pointerId) const
{
#ifdef WITH_RIVE_SCRIPTING
    auto scriptedObject = stateMachineInstance->scriptedObject(this);
    if (scriptedObject != nullptr)
    {
        auto statefulListenerAction =
            static_cast<ScriptedListenerAction*>(scriptedObject);
        statefulListenerAction->performStateful(stateMachineInstance,
                                                position,
                                                previousPosition,
                                                pointerId);
    }
#endif
}

StatusCode ScriptedListenerAction::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }

    auto stateMachineImporter =
        importStack.latest<StateMachineImporter>(StateMachine::typeKey);
    if (stateMachineImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    stateMachineImporter->addScriptedObject(this);
    return Super::import(importStack);
}

Core* ScriptedListenerAction::clone() const
{
    ScriptedListenerAction* twin =
        ScriptedListenerActionBase::clone()->as<ScriptedListenerAction>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

ScriptedObject* ScriptedListenerAction::cloneScriptedObject(
    DataBindContainer* dataBindContainer) const
{
    auto clonedScriptedObject = clone()->as<ScriptedListenerAction>();
    cloneProperties(clonedScriptedObject, dataBindContainer);
    clonedScriptedObject->reinit();
    return clonedScriptedObject;
}
void ScriptedListenerAction::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}