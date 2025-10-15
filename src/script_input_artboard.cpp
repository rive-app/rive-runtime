#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
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