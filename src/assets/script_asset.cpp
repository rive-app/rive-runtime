#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#include "libhydrogen.h"
#include "rive/importers/script_asset_importer.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/bytecode_header.hpp"
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
#include "rive/data_bind/data_bind.hpp"

using namespace rive;

ScriptInput::~ScriptInput()
{
    if (m_ownsDataBind && m_dataBind)
    {
        delete m_dataBind;
    }
}

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

#ifdef WITH_RIVE_SCRIPTING
bool OptionalScriptedMethods::verifyImplementation(ScriptedObject* object,
                                                   lua_State* state)
{
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

    auto scriptProtocol = object->scriptProtocol();
    if (scriptProtocol == ScriptProtocol::node ||
        scriptProtocol == ScriptProtocol::layout ||
        scriptProtocol == ScriptProtocol::converter ||
        scriptProtocol == ScriptProtocol::pathEffect ||
        scriptProtocol == ScriptProtocol::listenerAction ||
        scriptProtocol == ScriptProtocol::transitionCondition)
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
        if (static_cast<lua_Type>(lua_getfield(state, -1, "resize")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_resizesBit;
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
    else if (scriptProtocol == ScriptProtocol::converter)
    {
        if (static_cast<lua_Type>(lua_getfield(state, -1, "convert")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_dataConvertsBit;
        }
        rive_lua_pop(state, 1);
        if (static_cast<lua_Type>(lua_getfield(state, -1, "reverseConvert")) ==
            LUA_TFUNCTION)
        {
            m_implementedMethods |= m_dataReverseConvertsBit;
        }
        rive_lua_pop(state, 1);
    }
    rive_lua_pop(state, 1);
    return true;
}

ScriptingVM* ScriptAsset::scriptingVM()
{
    if (m_file == nullptr)
    {
        return nullptr;
    }
    return m_file->scriptingVM();
}

lua_State* ScriptAsset::vm()
{
    auto scriptingVM = this->scriptingVM();
    return scriptingVM ? scriptingVM->state() : nullptr;
}
#endif

bool ScriptAsset::initScriptedObject(ScriptedObject* object)
{
#ifdef WITH_RIVE_SCRIPTING
    if (scriptingVM() == nullptr)
    {
        return false;
    }
    return initScriptedObjectWith(object);
#else
    return false;
#endif
}

#if defined(WITH_RIVE_SCRIPTING)
void ScriptAsset::registrationComplete(int ref)
{
    if (isModule())
    {
        m_scriptRegistered = true;
    }
    else
    {
        generatorFunctionRef(ref);
    }
}
#endif

bool ScriptAsset::initScriptedObjectWith(ScriptedObject* object)
{
#if defined(WITH_RIVE_SCRIPTING)
    auto scriptVM = scriptingVM();
    if (scriptVM == nullptr)
    {
        return false;
    }
    lua_State* state = scriptVM->state();

    int ref = 0;
#ifdef WITH_RIVE_TOOLS
    // Edit-time mode: generatorFunctionRef() is a key to look up the actual
    // ref. Runtime mode: generatorFunctionRef() is the actual ref directly.

    // Note that the editor can actually host both edit-time scripting contexts
    // (where the editor owns the VM) and runtime ones (for artboards it's just
    // displaying as part of the UI). This path works for both cases as in the
    // latter hasGeneratorRef will return false.
    ScriptingContext* context =
        static_cast<ScriptingContext*>(lua_getthreaddata(state));
    if (context != nullptr && context->hasGeneratorRef(generatorFunctionRef()))
    {
        ref = context->getGeneratorRef(generatorFunctionRef());
    }
    else
#endif
    {
        // Runtime mode: generatorFunctionRef is the actual ref
        ref = generatorFunctionRef();
    }

    if (ref == 0)
    {
        fprintf(stderr,
                "ScriptAsset doesn't have a generator function %s\n",
                name().c_str());
        return false;
    }
    rive_lua_pushRef(state, ref);

    if (!m_initted)
    {
        if (!verifyImplementation(object, state))
        {
            fprintf(stderr,
                    "ScriptAsset failed to verify method implementation %s\n",
                    name().c_str());
            rive_lua_pop(state, 1);
            return false;
        }
        m_initted = true;
    }
    object->implementedMethods(implementedMethods());
    return object->scriptInit(scriptVM);
#else
    return false;
#endif
}

bool ScriptAsset::decode(SimpleArray<uint8_t>& data, Factory* factory)
{
#ifdef WITH_RIVE_SCRIPTING
    m_verified = false;

    // For in-band bytecode, isSigned should always be false (signature is
    // stored separately for aggregate verification).
    BytecodeHeader header(Span<const uint8_t>(data.data(), data.size()));
    if (!header.isValid())
    {
        return false;
    }

    // Store just the bytecode (without header) for later verification and use.
    auto bytecode = header.bytecode();
    m_bytecode = SimpleArray<uint8_t>(bytecode.data(), bytecode.size());
#endif
    return true;
}

bool ScriptAsset::bytecode(Span<uint8_t> data)
{
#ifdef WITH_RIVE_SCRIPTING
    BytecodeHeader header(Span<const uint8_t>(data.data(), data.size()));
    if (!header.isValid())
    {
        m_verified = false;
        return false;
    }

    auto bytecode = header.bytecode();

    if (!header.isSigned())
    {
        // Unsigned bytecode - mark as unverified
        m_verified = false;
        m_bytecode = SimpleArray<uint8_t>(bytecode.data(), bytecode.size());
        return true;
    }

    auto signature = header.signature();
    if (hydro_sign_verify(signature.data(),
                          bytecode.data(),
                          bytecode.size(),
                          "RiveCode",
                          g_scriptVerificationPublicKey) != 0)
    {
        // Forged.
        m_verified = false;
        return false;
    }
    m_verified = true;
    m_bytecode = SimpleArray<uint8_t>(bytecode.data(), bytecode.size());
#endif
    return true;
}