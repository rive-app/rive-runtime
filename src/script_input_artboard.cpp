#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/scripted_object_importer.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/script_input_artboard.hpp"

using namespace rive;

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

    return Super::import(importStack);
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