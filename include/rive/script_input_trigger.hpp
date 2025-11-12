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
public:
    bool validateForScriptInit() override { return true; }
    void fire(const CallbackData& value) override
    {
        Super::fire(value);
        scriptedObject()->trigger(name());
    }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif