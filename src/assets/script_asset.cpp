#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/file.hpp"
#include "rive/script_input_artboard.hpp"
#include "rive/script_input_boolean.hpp"
#include "rive/script_input_color.hpp"
#include "rive/script_input_number.hpp"
#include "rive/script_input_string.hpp"
#include "rive/script_input_trigger.hpp"
#include "rive/script_input_viewmodel_property.hpp"
#include "rive/scripted/scripted_object.hpp"

using namespace rive;

ScriptInput* ScriptInput::from(Core* component)
{
    switch (component->coreType())
    {
        case ScriptInputArtboard::typeKey:
            return component->as<ScriptInputArtboard>();
        case ScriptInputBoolean::typeKey:
            return component->as<ScriptInputBoolean>();
        case ScriptInputColor::typeKey:
            return component->as<ScriptInputColor>();
        case ScriptInputNumber::typeKey:
            return component->as<ScriptInputNumber>();
        case ScriptInputString::typeKey:
            return component->as<ScriptInputString>();
        case ScriptInputTrigger::typeKey:
            return component->as<ScriptInputTrigger>();
        case ScriptInputViewModelProperty::typeKey:
            return component->as<ScriptInputViewModelProperty>();
    }
    return nullptr;
}

void ScriptInput::initScriptedValue() {}

bool ScriptAsset::initScriptedObject(ScriptedObject* object)
{
#ifdef WITH_RIVE_SCRIPTING
    if (vm() == nullptr)
    {
        return false;
    }
    return initScriptedObjectWith(object);
#else
    return false;
#endif
}

bool ScriptAsset::initScriptedObjectWith(ScriptedObject* object)
{
#if defined(WITH_RIVE_SCRIPTING) && defined(WITH_RIVE_TOOLS)
    if (vm() == nullptr || generatorFunctionRef() == 0)
    {
        return false;
    }
    auto vmState = vm()->state;
    auto pushedType = rive_lua_pushRef(vmState, generatorFunctionRef());
    if (static_cast<lua_Type>(pushedType) != LUA_TFUNCTION)
    {
        fprintf(stderr,
                "ScriptAsset::initScriptedObjectWith: did not push a function "
                "at generatorFunctionRef, instead it pushed a %d\n ",
                pushedType);
    }
    return object->scriptInit(vm());
#else
    return false;
#endif
}