#include "rive/importers/listener_input_type_semantic_importer.hpp"

using namespace rive;

ListenerInputTypeSemanticImporter::ListenerInputTypeSemanticImporter(
    ListenerInputTypeSemantic* listenerInputTypeSemantic) :
    m_listenerInputTypeSemantic(listenerInputTypeSemantic)
{}

StatusCode ListenerInputTypeSemanticImporter::resolve()
{
    return StatusCode::Ok;
}
