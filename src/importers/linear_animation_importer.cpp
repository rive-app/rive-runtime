#include "rive/importers/linear_animation_importer.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"

using namespace rive;

LinearAnimationImporter::LinearAnimationImporter(LinearAnimation* animation) :
    m_Animation(animation)
{}

void LinearAnimationImporter::addKeyedObject(std::unique_ptr<KeyedObject> object)
{
    m_Animation->addKeyedObject(std::move(object));
}