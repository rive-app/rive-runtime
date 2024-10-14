#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/core.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/layout_component.hpp"

using namespace rive;

InterpolatorHost* InterpolatorHost::from(Core* component)
{
    switch (component->coreType())
    {
        case LayoutComponent::typeKey:
            return component->as<LayoutComponent>();
        default:
            return nullptr;
    }
    return nullptr;
}

StatusCode KeyFrameInterpolator::import(ImportStack& importStack)
{
    auto artboardImporter =
        importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }
    artboardImporter->addComponent(this);
    return Super::import(importStack);
}