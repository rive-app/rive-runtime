#include "rive/animation/scripted_transition_condition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"
#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif

using namespace rive;

ScriptedTransitionCondition::~ScriptedTransitionCondition()
{
    disposeScriptInputs();
}

void ScriptedTransitionCondition::disposeScriptInputs()
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

// Note: evaluateStateful is the actual instance of the
// ScriptedTransitionCondition that will run the script. evaluate itself will
// look for the map between the stateless and the stateful instances of this
// class.
bool ScriptedTransitionCondition::evaluateStateful(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    bool result = false;
#ifdef WITH_RIVE_SCRIPTING
    if (m_vm != nullptr)
    {
        lua_State* L = state();
        // Stack: []
        rive_lua_pushRef(L, m_self);
        // Stack: [self]
        lua_getfield(L, -1, "evaluate");

        // Stack: [self, field]
        lua_insert(L, -2); // Swap self and field

        // Stack: [field, self]
        if (static_cast<lua_Status>(rive_lua_pcall(L, 1, 1)) == LUA_OK)
        {
            if (lua_isboolean(L, -1))
            {
                result = lua_toboolean(L, -1);
            }
            // Stack: [result]
            rive_lua_pop(L, 1);
        }
        else
        {
            // Stack: [status]
            rive_lua_pop(L, 1);
        }
    }
#endif
    return result;
}

bool ScriptedTransitionCondition::evaluate(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
#ifdef WITH_RIVE_SCRIPTING
    auto scriptedObject = stateMachineInstance->scriptedObject(this);
    if (scriptedObject != nullptr)
    {
        auto statefulListenerAction =
            static_cast<ScriptedTransitionCondition*>(scriptedObject);
        return statefulListenerAction->evaluateStateful(stateMachineInstance,
                                                        layerInstance);
    }
#endif
    return false;
}

StatusCode ScriptedTransitionCondition::import(ImportStack& importStack)
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

Core* ScriptedTransitionCondition::clone() const
{
    ScriptedTransitionCondition* twin = ScriptedTransitionConditionBase::clone()
                                            ->as<ScriptedTransitionCondition>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

ScriptedObject* ScriptedTransitionCondition::cloneScriptedObject(
    DataBindContainer* dataBindContainer) const
{
    auto clonedScriptedObject = clone()->as<ScriptedTransitionCondition>();
    cloneProperties(clonedScriptedObject, dataBindContainer);
    clonedScriptedObject->reinit();
    return clonedScriptedObject;
}

void ScriptedTransitionCondition::addProperty(CustomProperty* prop)
{
    auto scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}