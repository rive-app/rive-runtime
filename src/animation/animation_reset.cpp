#include "rive/animation/animation_reset.hpp"
#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/keyed_property.hpp"
#include "rive/animation/keyframe_color.hpp"
#include "rive/animation/keyframe_double.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/generated/core_registry.hpp"

using namespace rive;

AnimationReset::AnimationReset()
    : m_binaryWriter(&m_WriteBuffer)
    , m_binaryReader(nullptr, 0)
{}

void AnimationReset::writeObjectId(uint32_t objectId)
{
  m_binaryWriter.writeVarUint(objectId);
}

void AnimationReset::writeTotalProperties(uint32_t value)
{
  m_binaryWriter.writeVarUint(value);
}

void AnimationReset::writePropertyKey(uint32_t value)
{
  m_binaryWriter.writeVarUint(value);
}

void AnimationReset::writePropertyValue(float value)
{
  m_binaryWriter.writeFloat(value);
}

void AnimationReset::clear() { m_binaryWriter.clear(); }

namespace
{
// What AnimationReset::build works in, one set per thread, so that a reset
// held for long (a 1D blend state's) carries none of it.
struct BuildScratch
{
  struct Entry
  {
    uint32_t objectIndex;
    bool isBaseline;
    const KeyedProperty* property;
  };
  std::vector<uint32_t> objectIds;
  std::vector<Entry> entries;
  std::vector<Entry> byObject;
  std::vector<uint32_t> objectStarts;
};
} // namespace

void AnimationReset::build(
    const LinearAnimation* const* animations,
    size_t count,
    Artboard* artboard,
    bool useFirstAsBaseline)
{
  static thread_local BuildScratch scratch;
  auto& objectIds = scratch.objectIds;
  auto& entries = scratch.entries;
  auto& byObject = scratch.byObject;
  auto& objectStarts = scratch.objectStarts;
  objectIds.clear();
  entries.clear();
  for (size_t a = 0; a < count; a++)
  {
    auto animation = animations[a];
    const bool isBaseline = useFirstAsBaseline && a == 0;
    for (size_t o = 0, objects = animation->numKeyedObjects(); o < objects; o++)
    {
      auto keyedObject = animation->getObject(o);
      auto objectId = keyedObject->objectId();
      uint32_t objectIndex = 0;
      while (objectIndex < objectIds.size() &&
             objectIds[objectIndex] != objectId)
      {
        objectIndex++;
      }
      if (objectIndex == objectIds.size())
      {
        objectIds.push_back(objectId);
      }
      for (size_t k = 0, keyed = keyedObject->numKeyedProperties(); k < keyed;
           k++)
      {
        auto property = keyedObject->getProperty(k);
        switch (CoreRegistry::propertyFieldId(property->propertyKey()))
        {
          case CoreDoubleType::id:
          case CoreColorType::id:
            entries.push_back({objectIndex, isBaseline, property});
            break;
        }
      }
    }
  }

  // Group the entries by object, keeping their order within each.
  const size_t objectCount = objectIds.size();
  objectStarts.assign(objectCount + 1, 0);
  for (const auto& entry : entries)
  {
    objectStarts[entry.objectIndex + 1]++;
  }
  for (size_t i = 0; i < objectCount; i++)
  {
    objectStarts[i + 1] += objectStarts[i];
  }
  byObject.resize(entries.size());
  for (const auto& entry : entries)
  {
    byObject[objectStarts[entry.objectIndex]++] = entry;
  }
  // Placing moved each start to its object's end: shift them back.
  for (size_t i = objectCount; i > 0; i--)
  {
    objectStarts[i] = objectStarts[i - 1];
  }
  objectStarts[0] = 0;

  for (size_t i = 0; i < objectCount; i++)
  {
    auto begin = byObject.begin() + objectStarts[i];
    auto end = byObject.begin() + objectStarts[i + 1];
    // A property a later animation keys again keeps its first entry.
    auto last = begin;
    for (auto it = begin; it != end; ++it)
    {
      auto key = it->property->propertyKey();
      bool seen = false;
      for (auto kept = begin; kept != last; ++kept)
      {
        if (kept->property->propertyKey() == key)
        {
          seen = true;
          break;
        }
      }
      if (!seen)
      {
        *last++ = *it;
      }
    }
    if (last == begin)
    {
      continue;
    }
    auto objectId = objectIds[i];
    auto object = artboard->resolve(objectId);
    if (object == nullptr)
    {
      continue;
    }
    auto component = object->as<Component>();
    writeObjectId(objectId);
    writeTotalProperties((uint32_t)(last - begin));
    for (auto it = begin; it != last; ++it)
    {
      auto keyedProperty = it->property;
      auto propertyKey = keyedProperty->propertyKey();
      // As before, a baseline property without keyframes writes its key
      // and no value.
      switch (CoreRegistry::propertyFieldId(propertyKey))
      {
        case CoreDoubleType::id:
          writePropertyKey(propertyKey);
          if (it->isBaseline)
          {
            if (auto first = keyedProperty->first())
            {
              writePropertyValue(first->as<KeyFrameDouble>()->value());
            }
          }
          else
          {
            writePropertyValue(CoreRegistry::getDouble(component, propertyKey));
          }
          break;
        case CoreColorType::id:
          writePropertyKey(propertyKey);
          if (it->isBaseline)
          {
            if (auto first = keyedProperty->first())
            {
              writePropertyValue((float)first->as<KeyFrameColor>()->value());
            }
          }
          else
          {
            writePropertyValue(
                (float)CoreRegistry::getColor(component, propertyKey));
          }
          break;
      }
    }
  }
  complete();
}

void AnimationReset::complete()
{
  if (!m_WriteBuffer.empty())
  {
    m_binaryReader.complete(&m_WriteBuffer.front(), m_binaryWriter.size());
  }
}

template <typename Visit>
void AnimationReset::forEachValue(Artboard* artboard, Visit&& visit)
{
  if (m_WriteBuffer.empty())
  {
    return;
  }
  m_binaryReader.reset(&m_WriteBuffer.front());
  while (!m_binaryReader.isEOF())
  {
    auto objectId = m_binaryReader.readVarUint32();
    auto object = artboard->resolve(objectId);
    auto totalProperties = m_binaryReader.readVarUint32();
    uint32_t currentPropertyIndex = 0;
    while (currentPropertyIndex < totalProperties)
    {
      auto propertyKey = m_binaryReader.readVarUint32();
      auto propertyValue = m_binaryReader.readFloat32();
      visit(object, propertyKey, propertyValue);
      currentPropertyIndex++;
    }
  }
}

void AnimationReset::apply(Artboard* artboard, BlendAccumulator* accumulator)
{
  forEachValue(
      artboard,
      [accumulator](Core* object, uint32_t propertyKey, float propertyValue) {
        switch (CoreRegistry::propertyFieldId(propertyKey))
        {
          case CoreDoubleType::id:
            if (accumulator == nullptr ||
                !accumulator
                     ->applyDouble(object, propertyKey, 1.0f, propertyValue))
            {
              CoreRegistry::setDouble(object, propertyKey, propertyValue);
            }
            break;
          case CoreColorType::id:
            // The same conversion setColor makes.
            if (accumulator == nullptr || !accumulator->applyColor(
                                              object,
                                              propertyKey,
                                              1.0f,
                                              (ColorInt)(int)propertyValue))
            {
              CoreRegistry::setColor(object, propertyKey, propertyValue);
            }
            break;
        }
      });
}

void AnimationReset::seed(Artboard* artboard, BlendAccumulator& accumulator)
{
  forEachValue(
      artboard,
      [&](Core* object, uint32_t propertyKey, float propertyValue) {
        switch (CoreRegistry::propertyFieldId(propertyKey))
        {
          case CoreDoubleType::id:
            accumulator.seedDouble(object, propertyKey, propertyValue);
            break;
          case CoreColorType::id:
            // The same conversion apply() makes through setColor.
            accumulator.seedColor(
                object,
                propertyKey,
                (ColorInt)(int)propertyValue);
            break;
        }
      });
}
