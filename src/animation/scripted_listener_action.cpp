#include "rive/animation/scripted_listener_action.hpp"
#include "rive/animation/listener_invocation.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/importers/state_machine_importer.hpp"
#include "rive/data_bind/data_bind.hpp"

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
//
// Dispatch: if `performAction` is defined, it is called with (self,
// ScriptedInvocation) and legacy `perform` is skipped. Otherwise `perform`
// is called with (self, PointerEvent) — real pointer data or a placeholder
// ScriptedPointerEvent for non-pointer invocations.
void ScriptedListenerAction::performStateful(
    StateMachineInstance* stateMachineInstance,
    const ListenerInvocation& invocation) const
{
#ifdef WITH_RIVE_SCRIPTING
    (void)stateMachineInstance;
    if (m_vm == nullptr || !m_vm->valid())
    {
        return;
    }
    m_vm->callListenerPerform(const_cast<ScriptedListenerAction*>(this),
                              m_self,
                              invocation);
#else
    (void)stateMachineInstance;
    (void)invocation;
#endif
}

void ScriptedListenerAction::perform(StateMachineInstance* stateMachineInstance,
                                     const ListenerInvocation& invocation) const
{
#ifdef WITH_RIVE_SCRIPTING
    auto scriptedObject = stateMachineInstance->scriptedObject(this);
    if (scriptedObject != nullptr)
    {
        auto statefulListenerAction =
            static_cast<ScriptedListenerAction*>(scriptedObject);
        statefulListenerAction->performStateful(stateMachineInstance,
                                                invocation);
    }
#else
    (void)stateMachineInstance;
    (void)invocation;
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
