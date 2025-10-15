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
    ScriptedObject* scriptedObject() { return ScriptedObject::from(parent()); }
    void initScriptedValue() override
    {
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setIntegerInput(name(), propertyValue());
        }
    }
    bool validateForScriptInit() override { return true; }
};
} // namespace rive

#endif