#ifndef _RIVE_SCRIPTED_LISTENER_ACTION_HPP_
#define _RIVE_SCRIPTED_LISTENER_ACTION_HPP_
#include "rive/generated/animation/scripted_listener_action_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptedListenerAction : public ScriptedListenerActionBase,
                               public ScriptedObject
{
public:
    ~ScriptedListenerAction();
    virtual void disposeScriptInputs() override;
    void perform(StateMachineInstance* stateMachineInstance,
                 Vec2D position,
                 Vec2D previousPosition,
                 int pointerId) const override;
    void performStateful(StateMachineInstance* stateMachineInstance,
                         Vec2D position,
                         Vec2D previousPosition,
                         int pointerId) const;

    uint32_t assetId() override { return scriptAssetId(); }
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return false;
    }
    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::listenerAction;
    }
    Component* component() override { return nullptr; }
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    ScriptedObject* cloneScriptedObject(DataBindContainer*) const override;
    void addProperty(CustomProperty* prop) override;
};
} // namespace rive

#endif