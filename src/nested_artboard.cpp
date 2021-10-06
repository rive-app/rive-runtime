#include "rive/nested_artboard.hpp"
#include "rive/artboard.hpp"
#include "rive/backboard.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/backboard_importer.hpp"

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
	    static_cast<NestedArtboard*>(Super::clone());
	if (m_NestedInstance == nullptr)
	{
		return nestedArtboard;
	}
	nestedArtboard->nest(m_NestedInstance->instance());
	// TODO: delete nestedArtboard on delete if it's an instance.
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