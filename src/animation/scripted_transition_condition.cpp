#include "rive/animation/scripted_transition_condition.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"

using namespace rive;

// Note: performStateful is the actual instance of the ScriptedListenerAction
// that will run the script. perform itself will look for the map between the
// stateless and the stateful instances of this class.
bool ScriptedTransitionCondition::evaluateStateful(
    const StateMachineInstance* stateMachineInstance,
    StateMachineLayerInstance* layerInstance) const
{
    bool result = false;
#ifdef WITH_RIVE_SCRIPTING
    if (m_state)
    {
        // Stack: []
        rive_lua_pushRef(m_state, m_self);
        // Stack: [self]
        lua_getfield(m_state, -1, "evaluate");

        // Stack: [self, field]
        lua_insert(m_state, -2); // Swap self and field

        // Stack: [field, self]
        if (static_cast<lua_Status>(rive_lua_pcall(m_state, 1, 1)) == LUA_OK)
        {
            if (lua_isboolean(m_state, -1))
            {
                result = lua_toboolean(m_state, -1);
            }
            // Stack: [result]
            rive_lua_pop(m_state, 1);
        }
        else
        {
            // Stack: [status]
            rive_lua_pop(m_state, 1);
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
    for (auto prop : m_customProperties)
    {
        auto clonedValue = prop->clone()->as<CustomProperty>();
        twin->addProperty(clonedValue);
    }
    return twin;
}

ScriptedObject* ScriptedTransitionCondition::cloneScriptedObject() const
{
    auto clonedScriptedObject = clone()->as<ScriptedTransitionCondition>();
    clonedScriptedObject->reinit();
    return clonedScriptedObject;
}