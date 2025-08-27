#include "rive/importers/keyed_property_importer.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe.hpp"
#include "rive/animation/linear_animation.hpp"

using namespace rive;

KeyedPropertyImporter::KeyedPropertyImporter(LinearAnimation* animation,
                                             KeyedProperty* keyedProperty) :
    m_Animation(animation), m_KeyedProperty(keyedProperty)
{}

void KeyedPropertyImporter::addKeyFrame(std::unique_ptr<KeyFrame> keyFrame)
{
    keyFrame->computeSeconds(m_Animation->fps());
    m_KeyedProperty->addKeyFrame(std::move(keyFrame));
}

bool KeyedPropertyImporter::readNullObject()
{
    // We don't need to add the null keyframe as nothing references them, but we
    // do need to not allow the null to propagate up.
    return true;
}