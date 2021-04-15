#include "importers/keyed_property_importer.hpp"
#include "animation/keyed_property.hpp"
#include "animation/keyframe.hpp"
#include "animation/linear_animation.hpp"

using namespace rive;

KeyedPropertyImporter::KeyedPropertyImporter(LinearAnimation* animation,
                                             KeyedProperty* keyedProperty) :
    m_Animation(animation), m_KeyedProperty(keyedProperty)
{
}

void KeyedPropertyImporter::addKeyFrame(KeyFrame* keyFrame)
{
	keyFrame->computeSeconds(m_Animation->fps());
	m_KeyedProperty->addKeyFrame(keyFrame);
}

bool KeyedPropertyImporter::readNullObject()
{
	// We don't need to add the null keyframe as nothing references them, but we
	// do need to not allow the null to propagate up.
	return true;
}