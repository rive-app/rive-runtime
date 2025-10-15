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
    ScriptedObject* scriptedObject() { return ScriptedObject::from(parent()); }
    void initScriptedValue() override {}
    bool validateForScriptInit() override { return true; }
    void fire(const CallbackData& value) override
    {
        Super::fire(value);
        scriptedObject()->trigger(name());
    }
};
} // namespace rive

#endif