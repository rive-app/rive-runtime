#include "rive/animation/linear_animation.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/artboard_importer.hpp"
#include "rive/importers/import_stack.hpp"

using namespace rive;

#ifdef TESTING
int LinearAnimation::deleteCount = 0;
#endif

LinearAnimation::~LinearAnimation()
{
#ifdef TESTING
	deleteCount++;
#endif
	for (auto object : m_KeyedObjects)
	{
		delete object;
	}
}

StatusCode LinearAnimation::onAddedDirty(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_KeyedObjects)
	{
		if ((code = object->onAddedDirty(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

StatusCode LinearAnimation::onAddedClean(CoreContext* context)
{
	StatusCode code;
	for (auto object : m_KeyedObjects)
	{
		if ((code = object->onAddedClean(context)) != StatusCode::Ok)
		{
			return code;
		}
	}
	return StatusCode::Ok;
}

void LinearAnimation::addKeyedObject(KeyedObject* object)
{
	m_KeyedObjects.push_back(object);
}

void LinearAnimation::apply(Artboard* artboard, float time, float mix) const
{
	for (auto object : m_KeyedObjects)
	{
		object->apply(artboard, time, mix);
	}
}

StatusCode LinearAnimation::import(ImportStack& importStack)
{
	auto artboardImporter =
	    importStack.latest<ArtboardImporter>(ArtboardBase::typeKey);
	if (artboardImporter == nullptr)
	{
		return StatusCode::MissingObject;
	}
	artboardImporter->addAnimation(this);
	return Super::import(importStack);
}

float LinearAnimation::startSeconds() const
{
	return (enableWorkArea() ? workStart() : 0) / (float)fps();
}
float LinearAnimation::endSeconds() const
{
	return (enableWorkArea() ? workEnd() : duration()) / (float)fps();
}
float LinearAnimation::durationSeconds() const
{
	return endSeconds() - startSeconds();
}