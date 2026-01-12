#include "rive/artboard_referencer.hpp"
#include "rive/artboard.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/nested_artboard_leaf.hpp"
#include "rive/nested_artboard_layout.hpp"
#include "rive/script_input_artboard.hpp"
#include "rive/file.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"

using namespace rive;

Artboard* ArtboardReferencer::findArtboard(
    ViewModelInstanceArtboard* viewModelInstanceArtboard,
    Artboard* parentArtboard,
    File* file)
{
    if (viewModelInstanceArtboard == nullptr)
    {
        return nullptr;
    }
    if (viewModelInstanceArtboard->asset() != nullptr)
    {
        if (!parentArtboard ||
            !parentArtboard->isAncestor(
                viewModelInstanceArtboard->asset()->artboard()))
        {
            return viewModelInstanceArtboard->asset()->artboard();
        }
        return nullptr;
    }
    else if (file != nullptr)
    {
        auto asset = file->artboard(viewModelInstanceArtboard->propertyValue());
        if (asset != nullptr)
        {
            auto artboardAsset = asset->as<Artboard>();
            if (!parentArtboard || !parentArtboard->isAncestor(artboardAsset))
            {
                return artboardAsset;
            }
        }
    }
    return nullptr;
}

ArtboardReferencer* ArtboardReferencer::from(Core* component)
{
    switch (component->coreType())
    {
        case NestedArtboardBase::typeKey:
        case NestedArtboardLeafBase::typeKey:
        case NestedArtboardLayoutBase::typeKey:
            return component->as<NestedArtboard>();
        case ScriptInputArtboardBase::typeKey:
            return component->as<ScriptInputArtboard>();
    }
    return nullptr;
}