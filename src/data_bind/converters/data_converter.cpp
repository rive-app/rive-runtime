#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/backboard.hpp"

using namespace rive;

StatusCode DataConverter::import(ImportStack& importStack)
{
    auto backboardImporter = importStack.latest<BackboardImporter>(Backboard::typeKey);
    if (backboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    backboardImporter->addDataConverter(this);

    return Super::import(importStack);
}