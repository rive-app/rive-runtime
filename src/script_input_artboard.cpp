#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/scripted_object_importer.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/script_input_artboard.hpp"
#include "rive/custom_property_container.hpp"

using namespace rive;

ScriptInputArtboard::~ScriptInputArtboard()
{
    auto obj = scriptedObject();
    if (obj != nullptr)
    {
        obj->removeProperty(this);
    }
    m_artboard = nullptr;
}

StatusCode ScriptInputArtboard::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addScriptInputArtboard(this);

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

StatusCode ScriptInputArtboard::onAddedClean(CoreContext* context)
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

Core* ScriptInputArtboard::clone() const
{
    ScriptInputArtboard* twin =
        ScriptInputArtboardBase::clone()->as<ScriptInputArtboard>();
    if (m_artboard != nullptr)
    {
        twin->artboard(m_artboard);
    }
    return twin;
}