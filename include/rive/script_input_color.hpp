#ifndef _RIVE_SCRIPT_INPUT_COLOR_HPP_
#define _RIVE_SCRIPT_INPUT_COLOR_HPP_
#include "rive/generated/script_input_color_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptInputColor : public ScriptInputColorBase, public ScriptInput
{
public:
    void initScriptedValue() override
    {
        ScriptInput::initScriptedValue();
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setIntegerInput(name(), propertyValue());
        }
    }
    bool validateForScriptInit() override { return true; }
    StatusCode import(ImportStack& importStack) override;
};
} // namespace rive

#endif