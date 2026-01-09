#ifndef _RIVE_SCRIPT_INPUT_TRIGGER_HPP_
#define _RIVE_SCRIPT_INPUT_TRIGGER_HPP_
#include "rive/generated/script_input_trigger_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptInputTrigger : public ScriptInputTriggerBase, public ScriptInput
{
protected:
    void propertyValueChanged() override;

public:
    ~ScriptInputTrigger();
    bool validateForScriptInit() override { return true; }
    StatusCode import(ImportStack& importStack) override;
    StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif