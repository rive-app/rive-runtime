#ifndef _RIVE_SCRIPT_INPUT_NUMBER_HPP_
#define _RIVE_SCRIPT_INPUT_NUMBER_HPP_
#include "rive/generated/script_input_number_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptInputNumber : public ScriptInputNumberBase, public ScriptInput
{
protected:
    void propertyValueChanged() override;

public:
    ~ScriptInputNumber();
    void initScriptedValue() override
    {
        ScriptInput::initScriptedValue();
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setNumberInput(name(), propertyValue());
        }
    }
    StatusCode onAddedClean(CoreContext* context) override;
    bool validateForScriptInit() override { return true; }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif