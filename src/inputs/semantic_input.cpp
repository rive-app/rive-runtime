#include "rive/inputs/semantic_input.hpp"
#include "rive/animation/listener_types/listener_input_type_semantic.hpp"
#include "rive/generated/animation/listener_types/listener_input_type_semantic_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/listener_input_type_semantic_importer.hpp"

using namespace rive;

StatusCode SemanticInput::import(ImportStack& importStack)
{
    auto* litImporter = importStack.latest<ListenerInputTypeSemanticImporter>(
        ListenerInputTypeSemanticBase::typeKey);
    if (litImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    litImporter->listenerInputTypeSemantic()->addSemanticInput(this);

    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}
