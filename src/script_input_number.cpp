#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/scripted_object_importer.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/script_input_number.hpp"
#include "rive/custom_property_container.hpp"

using namespace rive;

ScriptInputNumber::~ScriptInputNumber()
{
    auto obj = scriptedObject();
    if (obj != nullptr)
    {
        obj->removeProperty(this);
    }
}

StatusCode ScriptInputNumber::import(ImportStack& importStack)
{
    auto importer =
        importStack.latest<ScriptedObjectImporter>(ScriptedDrawable::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    importer->addInput(this);

    auto obj = scriptedObject();
    if (obj && obj->component() != nullptr)
    {
        // If the ScriptedObject is a Component, we need the ArtboardImporter
        // to add it as a Component, otherwise, return Ok
        return Super::import(importStack);
    }
    return StatusCode::Ok;
}

StatusCode ScriptInputNumber::onAddedClean(CoreContext* context)
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

void ScriptInputNumber::propertyValueChanged()
{
    auto obj = scriptedObject();
    if (obj != nullptr)
    {
        obj->setNumberInput(name(), propertyValue());
    }
}