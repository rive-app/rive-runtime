#include "rive/artboard.hpp"
#include "rive/custom_property.hpp"
#include "rive/assets/script_asset.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "rive/importers/scripted_object_importer.hpp"

using namespace rive;

ScriptedObjectImporter::ScriptedObjectImporter(ScriptedObject* obj) :
    m_scriptedObject(obj)
{}

void ScriptedObjectImporter::addInput(CustomProperty* value)
{
    auto input = ScriptInput::from(value);
    if (input != nullptr)
    {
        m_scriptedObject->addProperty(value);
        input->scriptedObject(m_scriptedObject);
    }
}

StatusCode ScriptedObjectImporter::resolve() { return StatusCode::Ok; }