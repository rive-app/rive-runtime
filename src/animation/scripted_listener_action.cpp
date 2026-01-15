#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/state_machine_instance.hpp"

using namespace rive;

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
    if (m_state == nullptr)
    {
        return;
    }
    // Stack: []
    rive_lua_pushRef(m_state, m_self);
    // Stack: [self]
    lua_getfield(m_state, -1, "perform");

    // Stack: [self, field]
    lua_pushvalue(m_state, -2);

    // Stack: [self, field, self]
    lua_newrive<ScriptedPointerEvent>(m_state, pointerId, position);

    // Stack: [self, field, self, pointerEvent]
    if (static_cast<lua_Status>(rive_lua_pcall(m_state, 2, 0)) == LUA_OK)
    {
        rive_lua_pop(m_state, 1);
    }
    else
    {
        rive_lua_pop(m_state, 2);
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
    for (auto prop : m_customProperties)
    {
        auto clonedValue = prop->clone()->as<CustomProperty>();
        twin->addProperty(clonedValue);
    }
    return twin;
}