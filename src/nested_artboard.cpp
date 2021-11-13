#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"
#include "rive/nested_animation.hpp"

using namespace rive;

NestedArtboard::~NestedArtboard()
{
	if (m_NestedInstance->isInstance())
	{
		delete m_NestedInstance;
	}
}
Core* NestedArtboard::clone() const
{
	NestedArtboard* nestedArtboard =
	    static_cast<NestedArtboard*>(NestedArtboardBase::clone());
	if (m_NestedInstance == nullptr)
	{
		return nestedArtboard;
	}
	nestedArtboard->nest(m_NestedInstance->instance());
	return nestedArtboard;
}

void NestedArtboard::nest(Artboard* artboard)
{
	assert(artboard != nullptr);
	m_NestedInstance = artboard;
	m_NestedInstance->advance(0.0f);
}

void NestedArtboard::draw(Renderer* renderer)
{
	if (m_NestedInstance == nullptr)
	{
		return;
	}
	renderer->save();
	renderer->transform(worldTransform());
	Mat2D translation;
	translation[4] = -m_NestedInstance->originX() * m_NestedInstance->width();
	translation[5] = -m_NestedInstance->originY() * m_NestedInstance->height();
	renderer->transform(translation);
	m_NestedInstance->draw(renderer);
	renderer->restore();
}

StatusCode NestedArtboard::import(ImportStack& importStack)
{
	auto backboardImporter =
	    importStack.latest<BackboardImporter>(Backboard::typeKey);
	if (backboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	backboardImporter->addNestedArtboard(this);

	return Super::import(importStack);
}

void NestedArtboard::addNestedAnimation(NestedAnimation* nestedAnimation)
{
	m_NestedAnimations.push_back(nestedAnimation);
}

StatusCode NestedArtboard::onAddedClean(CoreContext* context)
{
	// N.B. The nested instance will be null here for the source artboards.
	// Instances will have a nestedInstance available. This is a good thing as
	// it ensures that we only instance animations in artboard instances. It
	// does require that we always use an artboard instance (not just the source
	// artboard) when working with nested artboards, but in general this is good
	// practice for any loaded Rive file.
	if (m_NestedInstance != nullptr)
	{
		for (auto animation : m_NestedAnimations)
		{
			animation->initializeAnimation(m_NestedInstance);
		}
	}
	return Super::onAddedClean(context);
}

bool NestedArtboard::advance(float elapsedSeconds)
{
	if (m_NestedInstance == nullptr)
	{
		return false;
	}
	for (auto animation : m_NestedAnimations)
	{
		animation->advance(elapsedSeconds, m_NestedInstance);
	}
	return m_NestedInstance->advance(elapsedSeconds);
}

void NestedArtboard::update(ComponentDirt value)
{
	Super::update(value);
	if (hasDirt(value, ComponentDirt::WorldTransform) &&
	    m_NestedInstance != nullptr)
	{
		m_NestedInstance->opacity(renderOpacity());
	}
}