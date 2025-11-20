#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "libhydrogen.h"
#include "rive/importers/script_asset_importer.hpp"
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
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/scripted/scripted_layout.hpp"
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

bool OptionalScriptedMethods::verifyImplementation(
    ScriptProtocol scriptProtocol,
    LuaState* luaState)
{
#ifdef WITH_RIVE_SCRIPTING
    auto state = luaState->state;
    lua_pushvalue(state, -1);
    if (static_cast<lua_Status>(rive_lua_pcall(state, 0, 1)) != LUA_OK)
    {
        fprintf(stderr, "Verifying implementation pcall failed\n");
        rive_lua_pop(state, 1);
        return false;
    }
    if (static_cast<lua_Type>(lua_type(state, -1)) != LUA_TTABLE)
    {
        fprintf(stderr, "Verifying implementation not a table?\n");
        rive_lua_pop(state, 1);
        return false;
    }
    m_implementedMethods = 0;

    if (scriptProtocol == ScriptProtocol::node ||
        scriptProtocol == ScriptProtocol::layout ||
        scriptProtocol == ScriptProtocol::converter ||
        scriptProtocol == ScriptProtocol::pathEffect)
    {
        if (static_cast<lua_Type>(lua_getfield(state, -1, "update")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_updatesBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "advance")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_advancesBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "pointerDown")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_wantsPointerDownBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "pointerUp")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_wantsPointerUpBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "pointerMove")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_wantsPointerMoveBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "pointerCanceled")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_wantsPointerCancelBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "pointerExit")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_wantsPointerExitBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "init")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_initsBit;
        }
        rive_lua_pop(state, 1);
    }
    if (scriptProtocol == ScriptProtocol::layout)
    {
        if (static_cast<lua_Type>(lua_getfield(state, -1, "measure")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_measuresBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "draw")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_drawsBit;
        }
        rive_lua_pop(state, 1);
    }
    else if (scriptProtocol == ScriptProtocol::node)
    {
        if (static_cast<lua_Type>(lua_getfield(state, -1, "draw")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_drawsBit;
        }
        rive_lua_pop(state, 1);
    }
    rive_lua_pop(state, 1);
    return true;
#else
    return false;
#endif
}

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
    if (!m_initted)
    {
        m_scriptProtocol = object->scriptProtocol();
        verifyImplementation(m_scriptProtocol, vm());
        m_initted = true;
    }
    object->implementedMethods(implementedMethods());
    return object->scriptInit(vm());
#else
    return false;
#endif
}

bool ScriptAsset::decode(SimpleArray<uint8_t>& data, Factory* factory)
{
#ifdef WITH_RIVE_SCRIPTING
    m_verified = false;
    // Don't move here as the script asset importer needs to keep the bytecode
    // around for verification.
    m_bytecode = SimpleArray<uint8_t>(data);
#endif
    return true;
}

bool ScriptAsset::bytecode(Span<uint8_t> bytecode, Span<uint8_t> signature)
{
#ifdef WITH_RIVE_SCRIPTING
    if (signature.size() != hydro_sign_BYTES)
    {
        return false;
    }
    if (hydro_sign_verify(signature.data(),
                          bytecode.data(),
                          bytecode.size(),
                          "rive",
                          g_scriptVerificationPublicKey) != 0)
    {
        // Forged.
        m_verified = false;
        return false;
    }
    m_verified = true;
    m_bytecode = SimpleArray<uint8_t>(bytecode);
#endif
    return true;
}