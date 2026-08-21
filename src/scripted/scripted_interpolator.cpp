#include "rive/component_dirt.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_interpolator.hpp"

using namespace rive;

#ifdef WITH_RIVE_SCRIPTING

// Mirrors the Dart `_interpolatorBoilerplate` defaults: identity for
// `transform`, linear interpolation for `transformValue`. Used whenever the
// Lua side isn't ready (no VM, no `m_self`), the script doesn't define the
// method, or the pcall fails — so the runtime degrades to standard linear
// behavior instead of producing garbage.
static inline float defaultTransformValue(float from, float to, float factor)
{
    return from + (to - from) * factor;
}

float ScriptedInterpolator::transform(float factor) const
{
    if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
    {
        return factor;
    }
    float result = factor;
    // callNumberMethod wants a non-const ScriptedObject*; transform() is
    // const because it doesn't mutate the C++ object — only the script side.
    m_vm->callNumberMethod(const_cast<ScriptedInterpolator*>(this),
                           m_self,
                           "transform",
                           &factor,
                           1,
                           &result);
    return result;
}

float ScriptedInterpolator::transformValue(float valueFrom,
                                           float valueTo,
                                           float factor)
{
    if (m_vm == nullptr || !m_vm->valid() || m_self == 0)
    {
        return defaultTransformValue(valueFrom, valueTo, factor);
    }
    float args[3] = {valueFrom, valueTo, factor};
    float result = 0.0f;
    if (!m_vm->callNumberMethod(this,
                                m_self,
                                "transformValue",
                                args,
                                3,
                                &result))
    {
        return defaultTransformValue(valueFrom, valueTo, factor);
    }
    return result;
}

#else

float ScriptedInterpolator::transform(float factor) const { return factor; }

float ScriptedInterpolator::transformValue(float valueFrom,
                                           float valueTo,
                                           float factor)
{
    return valueFrom + (valueTo - valueFrom) * factor;
}

#endif

void ScriptedInterpolator::addProperty(CustomProperty* prop)
{
    auto* scriptInput = ScriptInput::from(prop);
    if (scriptInput != nullptr)
    {
        scriptInput->scriptedObject(this);
    }
    CustomPropertyContainer::addProperty(prop);
}

StatusCode ScriptedInterpolator::import(ImportStack& importStack)
{
    auto result = registerReferencer(importStack);
    if (result != StatusCode::Ok)
    {
        return result;
    }
    return Super::import(importStack);
}

// Mirrors ScriptedListenerAction::clone — preserve the file asset reference
// across the clone so the new instance vends the same script bytecode.
Core* ScriptedInterpolator::clone() const
{
    auto* twin = ScriptedInterpolatorBase::clone()->as<ScriptedInterpolator>();
    if (m_fileAsset != nullptr)
    {
        twin->setAsset(m_fileAsset);
    }
    return twin;
}

// Used by LinearAnimationInstance::statefulInterpolator() to vend a per-(LAI,
// keyframe) Lua instance lazily. `dataBindContainer` is the
// ArtboardInstance — it owns any cloned data binds.
ScriptedObject* ScriptedInterpolator::cloneScriptedObject(
    DataBindContainer* dataBindContainer) const
{
    auto* cloned = clone()->as<ScriptedInterpolator>();
    cloneProperties(cloned, dataBindContainer);
    cloned->reinit();
    return cloned;
}
