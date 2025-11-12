#include "rive/importers/backboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/scripted_object_importer.hpp"
#include "rive/scripted/scripted_drawable.hpp"
#include "rive/script_input_color.hpp"

using namespace rive;

StatusCode ScriptInputColor::import(ImportStack& importStack)
{
    auto importer =
        importStack.latest<ScriptedObjectImporter>(ScriptedDrawable::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    importer->addInput(this);

    return Super::import(importStack);
}