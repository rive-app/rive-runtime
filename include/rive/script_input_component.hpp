#ifndef _RIVE_SCRIPT_INPUT_COMPONENT_HPP_
#define _RIVE_SCRIPT_INPUT_COMPONENT_HPP_
#include "rive/generated/script_input_component_base.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
namespace rive
{
class Component;

/// A script's NodeRef input: a reference to a component in its owner's
/// artboard. componentId is that component's index there, remapped by the
/// exporter like any other Id. The script gets an id, not the node: it asks
/// for the node through ScriptedObject::nodeRefTarget, and the node is read
/// only.
class ScriptInputComponent : public ScriptInputComponentBase, public ScriptInput
{
private:
  Component* m_target = nullptr;

public:
  ~ScriptInputComponent();
  /// The component componentId resolved to, null when it named nothing or
  /// the input is no artboard child.
  Component* target() const { return m_target; }
  /// The NodeRef id the script gets: the target's index plus one, 0 for
  /// none.
  uint32_t nodeRefId() const
  {
    return m_target != nullptr ? componentId() + 1 : 0;
  }
  void initScriptedValue() override
  {
    ScriptInput::initScriptedValue();
    auto obj = scriptedObject();
    if (obj)
    {
      obj->setNodeRefInput(name(), nodeRefId());
    }
  }
  bool validateForScriptInit() override { return true; }
  StatusCode import(ImportStack& importStack) override;
  bool claimsArtboardSlot(ImportStack& importStack) const override
  {
    return ScriptInput::takesArtboardSlot(this);
  }
  StatusCode onAddedDirty(CoreContext* context) override;
  StatusCode onAddedClean(CoreContext* context) override;
};
} // namespace rive

#endif
