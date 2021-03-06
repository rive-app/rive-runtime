#include "animation/linear_animation.hpp"
#include "animation/keyed_object.hpp"
#include "importers/artboard_importer.hpp"
#include "importers/import_stack.hpp"
#include "artboard.hpp"

using namespace rive;

LinearAnimation::~LinearAnimation()
{
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

void LinearAnimation::apply(Artboard* artboard, float time, float mix)
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