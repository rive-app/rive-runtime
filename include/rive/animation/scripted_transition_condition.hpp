#ifndef _RIVE_SCRIPTED_TRANSITION_CONDITION_HPP_
#define _RIVE_SCRIPTED_TRANSITION_CONDITION_HPP_
#include "rive/generated/animation/scripted_transition_condition_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptedTransitionCondition : public ScriptedTransitionConditionBase,
                                    public ScriptedObject
{
public:
    ~ScriptedTransitionCondition();
    virtual void disposeScriptInputs() override;
    bool evaluate(const StateMachineInstance* stateMachineInstance,
                  StateMachineLayerInstance* layerInstance) const override;
    bool evaluateStateful(const StateMachineInstance* stateMachineInstance,
                          StateMachineLayerInstance* layerInstance) const;
    uint32_t assetId() override { return scriptAssetId(); }
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return false;
    }
    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::transitionCondition;
    }
    Component* component() override { return nullptr; }
    StatusCode import(ImportStack& importStack) override;
    Core* clone() const override;
    ScriptedObject* cloneScriptedObject(DataBindContainer*) const override;
    void addProperty(CustomProperty* prop) override;
};
} // namespace rive

#endif