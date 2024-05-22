#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"
#include "rive/importers/linear_animation_importer.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

KeyedObject::KeyedObject() {}
KeyedObject::~KeyedObject() {}

void KeyedObject::addKeyedProperty(std::unique_ptr<KeyedProperty> property)
{
    m_keyedProperties.push_back(std::move(property));
}

StatusCode KeyedObject::onAddedDirty(CoreContext* context)
{
    // Make sure we're keying a valid object.
    Core* coreObject = context->resolve(objectId());
    if (coreObject == nullptr)
    {
        return StatusCode::MissingObject;
    }

    for (auto& property : m_keyedProperties)
    {
        // Validate coreObject supports propertyKey
        if (!CoreRegistry::objectSupportsProperty(coreObject, property->propertyKey()))
        {
            return StatusCode::InvalidObject;
        }
        StatusCode code;
        if ((code = property->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
    }
    return StatusCode::Ok;
}

StatusCode KeyedObject::onAddedClean(CoreContext* context)
{
    for (auto& property : m_keyedProperties)
    {
        property->onAddedClean(context);
    }
    return StatusCode::Ok;
}

void KeyedObject::reportKeyedCallbacks(KeyedCallbackReporter* reporter,
                                       float secondsFrom,
                                       float secondsTo,
                                       bool isAtStartFrame) const
{
    for (const std::unique_ptr<KeyedProperty>& property : m_keyedProperties)
    {
        if (!CoreRegistry::isCallback(property->propertyKey()))
        {
            continue;
        }
        property->reportKeyedCallbacks(reporter,
                                       objectId(),
                                       secondsFrom,
                                       secondsTo,
                                       isAtStartFrame);
    }
}

void KeyedObject::apply(Artboard* artboard, float time, float mix)
{
    Core* object = artboard->resolve(objectId());
    if (object == nullptr)
    {
        return;
    }
    for (std::unique_ptr<KeyedProperty>& property : m_keyedProperties)
    {
        if (CoreRegistry::isCallback(property->propertyKey()))
        {
            continue;
        }
        property->apply(object, time, mix);
    }
}

StatusCode KeyedObject::import(ImportStack& importStack)
{
    auto importer = importStack.latest<LinearAnimationImporter>(LinearAnimationBase::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // we transfer ownership of ourself to the importer!
    importer->addKeyedObject(std::unique_ptr<KeyedObject>(this));
    return Super::import(importStack);
}