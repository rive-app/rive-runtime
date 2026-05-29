#ifdef WITH_RIVE_SCRIPTING
#include "rive/lua/rive_lua_libs.hpp"
#endif
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
    lua_State* L = state();
    if (L == nullptr || m_self == 0)
    {
        return factor;
    }
    // Stack: []
    rive_lua_pushRef(L, m_self);
    // Stack: [self]
    lua_getfield(L, -1, "transform");
    // Stack: [self, transform-or-nil]
    if (static_cast<lua_Type>(lua_type(L, -1)) != LUA_TFUNCTION)
    {
        // Method not implemented in user script — fall back to identity.
        rive_lua_pop(L, 2);
        return factor;
    }
    lua_pushvalue(L, -2);
    // Stack: [self, transform, self]
    lua_pushnumber(L, factor);
    // Stack: [self, transform, self, factor]
    // pcall_with_context wants a non-const ScriptedObject*; transform() is
    // const because it doesn't mutate the C++ object — only the Lua VM.
    if (static_cast<lua_Status>(
            rive_lua_pcall_with_context(L,
                                        const_cast<ScriptedInterpolator*>(this),
                                        2,
                                        1)) != LUA_OK)
    {
        // Stack: [self, errorMessage]
        rive_lua_pop(L, 2);
        return factor;
    }
    // Stack: [self, result]
    float result = static_cast<float>(lua_tonumber(L, -1));
    rive_lua_pop(L, 2);
    return result;
}

float ScriptedInterpolator::transformValue(float valueFrom,
                                           float valueTo,
                                           float factor)
{
    lua_State* L = state();
    if (L == nullptr || m_self == 0)
    {
        return defaultTransformValue(valueFrom, valueTo, factor);
    }
    // Stack: []
    rive_lua_pushRef(L, m_self);
    // Stack: [self]
    lua_getfield(L, -1, "transformValue");
    // Stack: [self, transformValue]
    lua_pushvalue(L, -2);
    // Stack: [self, transformValue, self]
    lua_pushnumber(L, valueFrom);
    lua_pushnumber(L, valueTo);
    lua_pushnumber(L, factor);
    // Stack: [self, transformValue, self, from, to, factor]
    if (static_cast<lua_Status>(rive_lua_pcall_with_context(L, this, 4, 1)) !=
        LUA_OK)
    {
        // Stack: [self, errorMessage]
        rive_lua_pop(L, 2);
        return defaultTransformValue(valueFrom, valueTo, factor);
    }
    // Stack: [self, result]
    float result = static_cast<float>(lua_tonumber(L, -1));
    rive_lua_pop(L, 2);
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
