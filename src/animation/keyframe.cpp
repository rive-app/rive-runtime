#include "rive/animation/keyframe.hpp"
#include "rive/animation/cubic_interpolator.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/core_context.hpp"
#include "rive/importers/import_stack.hpp"
#include "rive/importers/keyed_property_importer.hpp"

using namespace rive;

void KeyFrame::computeSeconds(int fps) { m_seconds = frame() / (float)fps; }

StatusCode KeyFrame::import(ImportStack& importStack)
{
    auto importer = importStack.latest<KeyedPropertyImporter>(KeyedProperty::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    importer->addKeyFrame(std::unique_ptr<KeyFrame>(this));
    return Super::import(importStack);
}