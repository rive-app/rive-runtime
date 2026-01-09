#ifndef _RIVE_SCRIPT_INPUT_BOOLEAN_HPP_
#define _RIVE_SCRIPT_INPUT_BOOLEAN_HPP_
#include "rive/generated/script_input_boolean_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptInputBoolean : public ScriptInputBooleanBase, public ScriptInput
{
protected:
    void propertyValueChanged() override;

public:
    ~ScriptInputBoolean();
    void initScriptedValue() override
    {
        ScriptInput::initScriptedValue();
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setBooleanInput(name(), propertyValue());
        }
    }
    bool validateForScriptInit() override { return true; }
    StatusCode import(ImportStack& importStack) override;
    StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif