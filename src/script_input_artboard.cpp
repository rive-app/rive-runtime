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
    m_referencedArtboard = nullptr;
}

StatusCode ScriptInputArtboard::import(ImportStack& importStack)
{
    auto backboardImporter =
        importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addArtboardReferencer(this);

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

void ScriptInputArtboard::initScriptedValue()
{
    ScriptInput::initScriptedValue();
    syncReferencedArtboard();
}

void ScriptInputArtboard::syncReferencedArtboard()
{

    if (m_referencedArtboard == nullptr)
    {
        return;
    }
    auto obj = scriptedObject();
    if (obj)
    {
        obj->setArtboardInput(name(), m_referencedArtboard);
    }
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
    if (m_referencedArtboard != nullptr)
    {
        twin->referencedArtboard(m_referencedArtboard);
        twin->file(m_file);
    }
    return twin;
}

void ScriptInputArtboard::artboardIdChanged()
{
    if (m_file)
    {
        m_referencedArtboard = m_file->artboard(artboardId());
        syncReferencedArtboard();
    }
}

void ScriptInputArtboard::updateArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard)
{
    auto referencedArtboard =
        findArtboard(viewModelInstanceArtboard, artboard(), m_file);
    if (referencedArtboard)
    {
        m_referencedArtboard = referencedArtboard;
        syncReferencedArtboard();
    }
}

int ScriptInputArtboard::referencedArtboardId() { return artboardId(); }