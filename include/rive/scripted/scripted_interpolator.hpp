#ifndef _RIVE_SCRIPTED_INTERPOLATOR_HPP_
#define _RIVE_SCRIPTED_INTERPOLATOR_HPP_
#include "rive/generated/scripted/scripted_interpolator_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include <stdio.h>
namespace rive
{
class ScriptedInterpolator : public ScriptedInterpolatorBase,
                             public ScriptedObject
{
public:
    float transformValue(float valueFrom, float valueTo, float factor) override;
    float transform(float factor) const override;
    uint32_t assetId() override { return scriptAssetId(); }
    bool addScriptedDirt(ComponentDirt value, bool recurse = false) override
    {
        return false;
    }
    ScriptProtocol scriptProtocol() override
    {
        return ScriptProtocol::interpolator;
    }
    Component* component() override { return nullptr; }

    /// Not a Component, so Artboard::m_Objects never takes this object's
    /// ScriptInputs and it has to free them itself -- the same rule
    /// ScriptedDataConverter, ScriptedListenerAction and
    /// ScriptedTransitionCondition already implement. Like them, the
    /// destructor calls it: ~ScriptedObject cannot, because a virtual call
    /// from a base destructor resolves to the base override.
    ~ScriptedInterpolator();
    void disposeScriptInputs() override;
    StatusCode import(ImportStack& importStack) override;

    // Cloning support so each (LinearAnimationInstance, InterpolatingKeyFrame)
    // pair can vend its own stateful Lua instance. Mirrors
    // ScriptedListenerAction / ScriptedTransitionCondition. Without these the
    // base `ScriptedObject::cloneScriptedObject` returns nullptr and the
    // per-instance path silently degrades to the shared template's
    // identity/linear fallback.
    Core* clone() const override;
    ScriptedObject* cloneScriptedObject(DataBindContainer*) const override;
    void addProperty(CustomProperty* prop) override;
};
} // namespace rive

#endif