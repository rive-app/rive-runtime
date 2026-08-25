#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"
#include "rive/layout_component.hpp"
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
    // `clip` is animatable; a layout whose clip is keyed needs its
    // DrawableProxy up front so it exists before the artboard's one-time proxy
    // injection, even while clip is currently false. This runs on the source
    // only (instances share animations); LayoutComponent::clone carries the
    // flag to instances.
    const bool isLayout = coreObject->is<LayoutComponent>();

    for (auto itr = m_keyedProperties.begin(); itr != m_keyedProperties.end();)
    {
        auto& property = *itr;
        // Validate coreObject supports propertyKey, if not remove it from the
        // property keys.
        if (!CoreRegistry::objectSupportsProperty(coreObject,
                                                  property->propertyKey()))
        {
            itr = m_keyedProperties.erase(itr);
            continue;
        }
        if (isLayout &&
            property->propertyKey() == LayoutComponentBase::clipPropertyKey)
        {
            coreObject->as<LayoutComponent>()->markClipMayBeDynamic();
        }
        StatusCode code;
        if ((code = property->onAddedDirty(context)) != StatusCode::Ok)
        {
            return code;
        }
        itr++;
    }
#ifdef WITH_RIVE_EDITOR
    // The editor list is rebuilt by `finalizeBatch` AFTER Pass 3
    // onAddedDirty runs, so at the time the dispatcher calls us on a
    // freshly-hydrated KeyedObject it's still empty. On subsequent
    // finalizeBatch runs (new coop batch), the editor list carries
    // the previously-wired coop keyed properties; walking it again
    // here keeps CoreRegistry::objectSupportsProperty pruning
    // consistent with the runtime path.
    for (auto itr = m_editorKeyedProperties.begin();
         itr != m_editorKeyedProperties.end();)
    {
        auto* property = *itr;
        if (!CoreRegistry::objectSupportsProperty(coreObject,
                                                  property->propertyKey()))
        {
            itr = m_editorKeyedProperties.erase(itr);
            continue;
        }
        property->onAddedDirty(context);
        itr++;
    }
#endif
    return StatusCode::Ok;
}

StatusCode KeyedObject::onAddedClean(CoreContext* context)
{
    for (auto& property : m_keyedProperties)
    {
        property->onAddedClean(context);
    }
#ifdef WITH_RIVE_EDITOR
    for (auto* property : m_editorKeyedProperties)
    {
        property->onAddedClean(context);
    }
#endif
    return StatusCode::Ok;
}

// `addKeyedPropertyForEditor` and `clearEditorKeyedProperties` live in
// `editor_native/native/src/editor/animation/keyed_object_editor.cpp`
// — see the matching comment in `keyed_property.cpp`.

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
#ifdef WITH_RIVE_EDITOR
    for (auto* property : m_editorKeyedProperties)
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
#endif
}

void KeyedObject::apply(Artboard* artboard,
                        float time,
                        float mix,
                        const LinearAnimationInstance* context)
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
        property->apply(object, time, mix, context);
    }
#ifdef WITH_RIVE_EDITOR
    for (auto* property : m_editorKeyedProperties)
    {
        if (CoreRegistry::isCallback(property->propertyKey()))
        {
            continue;
        }
        property->apply(object, time, mix);
    }
#endif
}

StatusCode KeyedObject::import(ImportStack& importStack)
{
    auto importer = importStack.latest<LinearAnimationImporter>(
        LinearAnimationBase::typeKey);
    if (importer == nullptr)
    {
        return StatusCode::MissingObject;
    }
    // we transfer ownership of ourself to the importer!
    importer->addKeyedObject(std::unique_ptr<KeyedObject>(this));
    return Super::import(importStack);
}