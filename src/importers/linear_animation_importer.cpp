#include "importers/linear_animation_importer.hpp"
#include "animation/keyed_object.hpp"
#include "animation/linear_animation.hpp"
#include "artboard.hpp"

using namespace rive;

LinearAnimationImporter::LinearAnimationImporter(LinearAnimation* animation) :
    m_Animation(animation)
{
}

void LinearAnimationImporter::addKeyedObject(KeyedObject* object)
{
	m_Animation->addKeyedObject(object);
}