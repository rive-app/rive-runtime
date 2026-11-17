#include "rive/importers/import_stack.hpp"
#include "rive/importers/scripted_object_importer.hpp"
#include "rive/script_input_component.hpp"
#include "rive/custom_property_container.hpp"

using namespace rive;

ScriptInputComponent::~ScriptInputComponent()
{
  auto obj = scriptedObject();
  if (obj != nullptr)
  {
    obj->removeProperty(this);
  }
}

StatusCode ScriptInputComponent::import(ImportStack& importStack)
{
  auto importer = ScriptInput::ownerImporter(importStack, this);
  if (importer == nullptr)
  {
    return StatusCode::MissingObject;
  }
  importer->addInput(this);

  auto obj = scriptedObject();
  if (obj && obj->component() != nullptr)
  {
    // An artboard child when its ScriptedObject is a Component (see
    // ScriptInputNumber::import). Any other owner has no artboard to name a
    // node in, so the input stays empty.
    return Super::import(importStack);
  }
  return StatusCode::Ok;
}

StatusCode ScriptInputComponent::onAddedDirty(CoreContext* context)
{
  StatusCode code = Super::onAddedDirty(context);
  if (code != StatusCode::Ok)
  {
    return code;
  }
  // Resolved per instance, like TargetedConstraint's target. Anything that
  // is not a component, or nothing at all, leaves the input empty rather
  // than failing the artboard.
  auto coreObject = context->resolve(componentId());
  m_target = coreObject != nullptr && coreObject->is<Component>() &&
                     coreObject != static_cast<Core*>(artboard())
                 ? coreObject->as<Component>()
                 : nullptr;
  return StatusCode::Ok;
}

StatusCode ScriptInputComponent::onAddedClean(CoreContext* context)
{
  StatusCode code = Super::onAddedClean(context);
  if (code != StatusCode::Ok)
  {
    return code;
  }

  auto p = parent();
  if (p != nullptr)
  {
    auto scriptedObj = ScriptedObject::from(p);
    if (scriptedObj != nullptr)
    {
      scriptedObj->addProperty(this);
    }
  }

  return StatusCode::Ok;
}
