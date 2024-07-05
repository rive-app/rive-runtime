#include "rive/animation/animation_reset_factory.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/animation/animation_state.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/generated/core_registry.hpp"
#include <map>
#include <set>

using namespace rive;

class KeyedPropertyData
{
public:
    const KeyedProperty* keyedProperty;
    bool isBaseline;
    KeyedPropertyData(const KeyedProperty* value, bool baselineValue) :
        keyedProperty(value), isBaseline(baselineValue)
    {}
};

class KeyedObjectData
{
public:
    std::vector<KeyedPropertyData> keyedPropertiesData;
    std::set<int> keyedPropertiesSet;
    uint32_t objectId;
    KeyedObjectData(const uint32_t value) { objectId = value; }
    void addProperties(const KeyedObject* keyedObject, bool isBaseline)
    {
        size_t index = 0;
        while (index < keyedObject->numKeyedProperties())
        {
            auto keyedProperty = keyedObject->getProperty(index);
            auto pos = keyedPropertiesSet.find(keyedProperty->propertyKey());
            if (pos == keyedPropertiesSet.end())
            {
                switch (CoreRegistry::propertyFieldId(keyedProperty->propertyKey()))
                {
                    case CoreDoubleType::id:
                    case CoreColorType::id:
                        keyedPropertiesSet.insert(keyedProperty->propertyKey());
                        keyedPropertiesData.push_back(KeyedPropertyData(keyedProperty, isBaseline));
                        break;
                }
            }
            index++;
        }
    }
};

class AnimationsData
{

private:
    std::vector<std::unique_ptr<KeyedObjectData>> keyedObjectsData;
    KeyedObjectData* getKeyedObjectData(const KeyedObject* keyedObject)
    {
        for (auto& keyedObjectData : keyedObjectsData)
        {
            if (keyedObjectData->objectId == keyedObject->objectId())
            {
                return keyedObjectData.get();
            }
        }

        auto keyedObjectData = rivestd::make_unique<KeyedObjectData>(keyedObject->objectId());
        auto ref = keyedObjectData.get();
        keyedObjectsData.push_back(std::move(keyedObjectData));
        return ref;
    }

    void findKeyedObjects(const LinearAnimation* animation, bool isFirstAnimation)
    {
        size_t index = 0;
        while (index < animation->numKeyedObjects())
        {
            auto keyedObject = animation->getObject(index);
            auto keyedObjectData = getKeyedObjectData(keyedObject);

            keyedObjectData->addProperties(keyedObject, isFirstAnimation);
            index++;
        }
    }

public:
    AnimationsData(std::vector<const LinearAnimation*>& animations, bool useFirstAsBaseline)
    {
        bool isFirstAnimation = useFirstAsBaseline;
        for (auto animation : animations)
        {
            findKeyedObjects(animation, isFirstAnimation);
            isFirstAnimation = false;
        }
    }

    void writeObjects(AnimationReset* animationReset, ArtboardInstance* artboard)
    {
        for (auto& keyedObjectData : keyedObjectsData)
        {
            auto object = artboard->resolve(keyedObjectData->objectId)->as<Component>();
            auto propertiesData = keyedObjectData->keyedPropertiesData;
            if (propertiesData.size() > 0)
            {
                animationReset->writeObjectId(keyedObjectData->objectId);
                animationReset->writeTotalProperties(propertiesData.size());
                for (auto keyedPropertyData : propertiesData)
                {
                    auto keyedProperty = keyedPropertyData.keyedProperty;
                    auto propertyKey = keyedProperty->propertyKey();
                    switch (CoreRegistry::propertyFieldId(propertyKey))
                    {
                        case CoreDoubleType::id:
                            animationReset->writePropertyKey(propertyKey);
                            if (keyedPropertyData.isBaseline)
                            {
                                auto firstKeyframe = keyedProperty->first();
                                if (firstKeyframe != nullptr)
                                {
                                    auto value =
                                        keyedProperty->first()->as<KeyFrameDouble>()->value();
                                    animationReset->writePropertyValue(value);
                                }
                            }
                            else
                            {
                                animationReset->writePropertyValue(
                                    CoreRegistry::getDouble(object, propertyKey));
                            }
                            break;
                        case CoreColorType::id:

                            animationReset->writePropertyKey(propertyKey);
                            if (keyedPropertyData.isBaseline)
                            {
                                auto firstKeyframe = keyedProperty->first();
                                if (firstKeyframe != nullptr)
                                {
                                    auto value =
                                        keyedProperty->first()->as<KeyFrameColor>()->value();
                                    animationReset->writePropertyValue(value);
                                }
                            }
                            else
                            {
                                animationReset->writePropertyValue(
                                    CoreRegistry::getColor(object, propertyKey));
                            }
                            break;
                    }
                }
            }
        }
        animationReset->complete();
    }
};

std::unique_ptr<AnimationReset> AnimationResetFactory::getInstance()
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if (m_resources.size() > 0)
    {
        auto instance = std::move(m_resources.back());
        m_resources.pop_back();
        return instance;
    }
    auto instance = rivestd::make_unique<AnimationReset>();
    return instance;
}

void AnimationResetFactory::fromState(StateInstance* stateInstance,
                                      std::vector<const LinearAnimation*>& animations)
{
    if (stateInstance != nullptr)
    {
        auto state = stateInstance->state();
        if (state->is<AnimationState>() && state->as<AnimationState>()->animation() != nullptr)
        {
            animations.push_back(state->as<AnimationState>()->animation());
        }
    }
}

std::unique_ptr<AnimationReset> AnimationResetFactory::fromStates(StateInstance* stateFrom,
                                                                  StateInstance* currentState,
                                                                  ArtboardInstance* artboard)
{
    std::vector<const LinearAnimation*> animations;
    fromState(stateFrom, animations);
    fromState(currentState, animations);
    return fromAnimations(animations, artboard, false);
}

std::unique_ptr<AnimationReset> AnimationResetFactory::fromAnimations(
    std::vector<const LinearAnimation*>& animations,
    ArtboardInstance* artboard,
    bool useFirstAsBaseline)
{
    auto animationsData = new AnimationsData(animations, useFirstAsBaseline);
    auto animationReset = AnimationResetFactory::getInstance();
    animationsData->writeObjects(animationReset.get(), artboard);
    delete animationsData;
    return animationReset;
}

std::vector<std::unique_ptr<AnimationReset>> AnimationResetFactory::m_resources;

std::mutex AnimationResetFactory::m_mutex;

void AnimationResetFactory::release(std::unique_ptr<AnimationReset> value)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    value->clear();
    m_resources.push_back(std::move(value));
}
