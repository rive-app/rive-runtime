#ifdef WITH_RIVE_SCRIPTING
#ifdef WITH_RIVE_SCRIPTING_LUAU
// Only for the ScriptingVM to ScriptBackend conversion in backend().
#include "rive/lua/scripting_vm.hpp"
#endif
#include "libhydrogen.h"
#include "rive/importers/text_asset_importer.hpp"
#endif
#include "rive/assets/script_asset.hpp"
#include "rive/signed_content_header.hpp"
#include "rive/file.hpp"
#ifdef WITH_RIVE_SCRIPTING_WASM
#include "rive/wasm/wasm_scripting_vm.hpp"
#endif
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

bool ScriptInput::validateForColdScriptInit()
{
    return validateForScriptInit();
}

bool ScriptInput::hydrateScriptInput()
{
    initScriptedValue();
    return true;
}

bool ScriptInput::validateHydrationPrerequisites() { return true; }

#ifdef WITH_RIVE_SCRIPTING
ScriptBackend* ScriptAsset::backend()
{
    if (m_file == nullptr)
    {
        return nullptr;
    }
#ifdef WITH_RIVE_SCRIPTING_WASM
    if (m_wasmBackend != nullptr)
    {
        return m_wasmBackend;
    }
    if (m_file->wasmScriptingVM() != nullptr)
    {
        return m_file->wasmScriptingVM();
    }
#endif
#ifdef WITH_RIVE_SCRIPTING_LUAU
    return m_file->scriptingVM();
#else
    return nullptr;
#endif
}
#endif

bool ScriptAsset::initScriptedObject(ScriptedObject* object)
{
#ifdef WITH_RIVE_SCRIPTING
    if (backend() == nullptr)
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
        // Force re-verification on next init so that method detection (e.g.
        // draw) reflects the newly compiled script.
        m_initted = false;
    }
}
#endif

bool ScriptAsset::initScriptedObjectWith(ScriptedObject* object)
{
#if defined(WITH_RIVE_SCRIPTING)
    auto scriptBackend = backend();
    if (scriptBackend == nullptr)
    {
        return false;
    }

    int ref = scriptBackend->resolveGeneratorRef(generatorFunctionRef());
    if (ref == 0)
    {
        fprintf(stderr,
                "ScriptAsset doesn't have a generator function %s\n",
                name().c_str());
        return false;
    }

    if (!m_initted)
    {
        // The editor detected the implemented methods (by executing the
        // generator in its workspace) and serialized the bitfield; the runtime
        // never detects. Files predating this property decode to the all-bits
        // default (2097151), so they behave as "implements everything" and rely
        // on graceful dispatch (each callback no-ops when the method isn't
        // actually a function on the returned table).
        OptionalScriptedMethods::implementedMethods(
            serializedImplementedMethods() & methodMask);
#ifdef WITH_RIVE_TOOLS
        // Bit 15 was the removed drawCanvas callback: the editor detected it
        // on this script, so its canvas work never runs until moved into
        // draw. Legacy all-bits exports stay silent; the shipping runtime
        // stays quiet entirely, the editor console carries the migration.
        static const uint32_t kRetiredDrawCanvasBit = 1 << 15;
        static const uint32_t kAllMethodsDefault = (1 << 21) - 1;
        if (serializedImplementedMethods() != kAllMethodsDefault &&
            (serializedImplementedMethods() & kRetiredDrawCanvasBit) != 0)
        {
            fprintf(stderr,
                    "rive: script implements drawCanvas, which is no longer "
                    "called; move its body into draw\n");
        }
#endif
        m_initted = true;
    }
    object->implementedMethods(implementedMethods());
    return object->ensureScriptInitialized(scriptBackend, ref);
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
    SignedContentHeader header(Span<const uint8_t>(data.data(), data.size()));
    if (!header.isValid())
    {
        return false;
    }

    // Store just the bytecode (without header) for later verification and use.
    auto bytecode = header.content();
    m_bytecode = SimpleArray<uint8_t>(bytecode.data(), bytecode.size());
#endif
    return true;
}

bool ScriptAsset::bytecode(Span<uint8_t> data)
{
#ifdef WITH_RIVE_SCRIPTING
    SignedContentHeader header(Span<const uint8_t>(data.data(), data.size()));
    if (!header.isValid())
    {
        m_verified = false;
        return false;
    }

    auto bytecode = header.content();

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