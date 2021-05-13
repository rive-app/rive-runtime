#include "artboard.hpp"
#include "animation/blend_animation.hpp"
#include "animation/layer_state.hpp"
#include "importers/layer_state_importer.hpp"
#include "importers/artboard_importer.hpp"

using namespace rive;

StatusCode BlendAnimation::import(ImportStack& importStack)
{
	auto importer =
	    importStack.latest<LayerStateImporter>(LayerStateBase::typeKey);
	if (importer == nullptr)
	{
		return StatusCode::MissingObject;
	}
	else if (!importer->addBlendAnimation(this))
	{
		return StatusCode::InvalidObject;
	}

	auto artboardImporter =
	    importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
	if (artboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}

	auto artboard = artboardImporter->artboard();
	auto animationCount = artboard->animationCount();
	if (animationId() >= 0 && animationId() < animationCount)
	{
		m_Animation = artboardImporter->artboard()->animation(animationId());
	}

	return Super::import(importStack);
}