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
public:
    ScriptedObject* scriptedObject() { return ScriptedObject::from(parent()); }
    void initScriptedValue() override
    {
        auto obj = scriptedObject();
        if (obj)
        {
            obj->setBooleanInput(name(), propertyValue());
        }
    }
    bool validateForScriptInit() override { return true; }
};
} // namespace rive

#endif