#include "rive/artboard.hpp"
#include "rive/animation/blend_animation.hpp"
#include "rive/animation/layer_state.hpp"
#include "rive/importers/layer_state_importer.hpp"
#include "rive/importers/artboard_importer.hpp"

using namespace rive;

LinearAnimation BlendAnimation::m_EmptyAnimation;

StatusCode BlendAnimation::import(ImportStack& importStack)
{
    auto importer = importStack.latest<LayerStateImporter>(LayerStateBase::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    else if (!importer->addBlendAnimation(this))
    {
        return StatusCode::InvalidObject;
    }

    auto artboardImporter = importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
    if (artboardImporter == nullptr)
    {
        return StatusCode::MissingObject;
    }

    auto artboard = artboardImporter->artboard();
    size_t animationCount = artboard->animationCount();
    if ((size_t)animationId() < animationCount)
    {
        m_Animation = artboardImporter->artboard()->animation(animationId());
    }

    return Super::import(importStack);
}
