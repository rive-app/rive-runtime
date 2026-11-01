#include "rive/animation/blend_accumulator.hpp"
#include "rive/animation/keyed_object.hpp"
#include "rive/animation/linear_animation.hpp"
#include "rive/artboard.hpp"
#include "rive/generated/core_registry.hpp"

#include <algorithm>

using namespace rive;

// The key of the entry that marks an object as having tracked properties
// (see track), which no property has.
static constexpr int kObjectMarker = -1;

static uint32_t slotOf(const Core* object, int propertyKey)
{
  // Fibonacci hashing of the pointer, with the key folded in.
  uint64_t h = ((uint64_t)(uintptr_t)object ^
                ((uint64_t)(uint32_t)propertyKey << 48)) *
               UINT64_C(0x9E3779B97F4A7C15);
  return (uint32_t)(h >> 32);
}

uint32_t BlendAccumulator::find(const Core* object, int propertyKey) const
{
  if (m_index.empty())
  {
    return 0;
  }
  const uint32_t mask = (uint32_t)m_index.size() - 1;
  for (uint32_t i = slotOf(object, propertyKey) & mask;; i = (i + 1) & mask)
  {
    uint32_t entry = m_index[i];
    if (entry == 0)
    {
      return 0;
    }
    const Value& value = m_values[entry - 1];
    if (value.object == object && value.propertyKey == propertyKey)
    {
      return entry;
    }
  }
}

void BlendAccumulator::index(uint32_t valueIndex)
{
  const uint32_t mask = (uint32_t)m_index.size() - 1;
  const Value& value = m_values[valueIndex];
  uint32_t i = slotOf(value.object, value.propertyKey) & mask;
  while (m_index[i] != 0)
  {
    i = (i + 1) & mask;
  }
  m_index[i] = valueIndex + 1;
}

uint32_t BlendAccumulator::add(Core* object, int propertyKey, Kind kind)
{
  uint32_t bits = 0;
  if (kind == Kind::string)
  {
    bits = (uint32_t)m_layer->strings.size();
    m_layer->strings.emplace_back();
  }
  m_values.push_back({object, propertyKey, kind, 0, false, 0, 0.0f, bits});
  if (m_values.size() * 2 > m_index.size())
  {
    m_index.assign(m_index.empty() ? 16 : m_index.size() * 2, 0);
    for (uint32_t i = 0; i < (uint32_t)m_values.size(); i++)
    {
      index(i);
    }
  }
  else
  {
    index((uint32_t)m_values.size() - 1);
  }
  return (uint32_t)m_values.size();
}

void BlendAccumulator::notifyChangesTo(Artboard* artboard)
{
  if (m_layer == nullptr)
  {
    m_layer = std::make_unique<LayerMode>();
  }
  m_layer->artboard = artboard;
}

void BlendAccumulator::track(Core* object, int propertyKey, Kind kind)
{
  assert(m_layer != nullptr);
  if (object == nullptr || find(object, propertyKey) != 0)
  {
    return;
  }
  // Each object's properties follow its marker, which counts them.
  uint32_t marker = find(object, kObjectMarker);
  if (marker == 0)
  {
    marker = add(object, kObjectMarker, Kind::number);
  }
  assert(m_values[marker - 1].bits + marker == m_values.size());
  m_values[marker - 1].bits++;
  add(object, propertyKey, kind);
  m_layer->lastObject = nullptr;
}

const std::vector<bool>* BlendAccumulator::trackedObjects(
    const LinearAnimation* animation)
{
  if (m_layer == nullptr)
  {
    return nullptr;
  }
  for (const auto& mask : m_layer->masks)
  {
    if (mask.first == animation)
    {
      return &mask.second;
    }
  }
  std::vector<bool> mask(animation->numKeyedObjects(), false);
  for (size_t i = 0; i < mask.size(); i++)
  {
    Core* object = m_layer->artboard != nullptr
                       ? m_layer->artboard->resolve(
                             animation->getObject(i)->objectId())
                       : nullptr;
    mask[i] = object != nullptr && find(object, kObjectMarker) != 0;
  }
  m_layer->masks.emplace_back(animation, std::move(mask));
  return &m_layer->masks.back().second;
}

bool BlendAccumulator::tracksAny(const LinearAnimation* animation)
{
  const std::vector<bool>* mask = trackedObjects(animation);
  return mask != nullptr &&
         std::find(mask->begin(), mask->end(), true) != mask->end();
}

BlendAccumulator::Value* BlendAccumulator::trackedValue(
    const Core* object,
    int propertyKey)
{
  LayerMode& layer = *m_layer;
  if (object != layer.lastObject)
  {
    layer.lastObject = object;
    uint32_t marker = find(object, kObjectMarker);
    layer.lastFirst = marker;
    layer.lastCount = marker == 0 ? 0 : m_values[marker - 1].bits;
  }
  for (uint32_t i = layer.lastFirst, end = layer.lastFirst + layer.lastCount;
       i < end;
       i++)
  {
    if (m_values[i].propertyKey == propertyKey)
    {
      return &m_values[i];
    }
  }
  return nullptr;
}

void BlendAccumulator::load(Value& value, const Value* from)
{
  Core* object = value.object;
  int propertyKey = value.propertyKey;
  switch (value.kind)
  {
    case Kind::number:
      value.doubleValue = from != nullptr
                              ? from->doubleValue
                              : CoreRegistry::getDouble(object, propertyKey);
      break;
    case Kind::color:
      value.bits = from != nullptr
                       ? from->bits
                       : CoreRegistry::getColor(object, propertyKey);
      break;
    // Discrete values are only ever written whole; the current one is what
    // a write is compared against.
    case Kind::uint:
      value.bits = CoreRegistry::getUint(object, propertyKey);
      break;
    case Kind::id:
#ifndef WITH_RIVE_EDITOR
      // Runtime ids are 32 bits; editor builds don't collect them.
      value.bits = CoreRegistry::getId(object, propertyKey);
#endif
      break;
    case Kind::boolean:
      value.bits = CoreRegistry::getBool(object, propertyKey);
      break;
    case Kind::string:
      m_layer->strings[value.bits] = CoreRegistry::getString(
          object,
          propertyKey);
      break;
  }
}

const BlendAccumulator::Value* BlendAccumulator::pending(
    const Core* object,
    int propertyKey)
{
  const Value* value = trackedValue(object, propertyKey);
  if (value == nullptr || value->frame != m_frame || !value->collecting)
  {
    return nullptr;
  }
  return value;
}

BlendAccumulator::Value* BlendAccumulator::blendValue(
    Core* object,
    int propertyKey,
    Kind kind,
    bool seeding)
{
  uint32_t entry = find(object, propertyKey);
  if (entry == 0)
  {
    entry = add(object, propertyKey, kind);
  }
  Value& value = m_values[entry - 1];
  if (value.frame != m_frame)
  {
    value.frame = m_frame;
    m_written.push_back(entry - 1);
    if (!seeding)
    {
      // An earlier layer may have written it to the machine's accumulator
      // and not to the object yet.
      load(
          value,
          m_parent != nullptr ? m_parent->pending(object, propertyKey)
                              : nullptr);
    }
  }
  return &value;
}

BlendAccumulator::Value* BlendAccumulator::layerValue(
    Core* object,
    int propertyKey)
{
  Value* value = trackedValue(object, propertyKey);
  if (value == nullptr)
  {
    return nullptr;
  }
  if (value->frame != m_frame)
  {
    // The first write since the last flush. Collect it only if the property
    // was written more than once before a flush last time; once is set
    // straight away, as it would have been anyway. Either way it lands
    // exactly where writing each value to the object would have.
    value->frame = m_frame;
    value->collecting = value->writes >= 2;
    value->writes = 0;
    if (value->collecting)
    {
      m_written.push_back((uint32_t)(value - m_values.data()));
      load(*value, nullptr);
    }
  }
  if (value->writes < 2)
  {
    value->writes++;
  }
  return value->collecting ? value : nullptr;
}

void BlendAccumulator::changed(const Value& value)
{
  if (m_layer->artboard != nullptr)
  {
    m_layer->artboard->noteChange();
  }
  value.object->notifyPropertyChanged((uint16_t)value.propertyKey);
}

int BlendAccumulator::storageKey(int propertyKey)
{
  switch (propertyKey)
  {
    case NodeBase::xArtboardPropertyKey:
      return NodeBase::xPropertyKey;
    case NodeBase::yArtboardPropertyKey:
      return NodeBase::yPropertyKey;
  }
  return propertyKey;
}

void BlendAccumulator::seedDouble(Core* object, int propertyKey, float value)
{
  blendValue(object, storageKey(propertyKey), Kind::number, true)
      ->doubleValue = value;
}

void BlendAccumulator::seedColor(Core* object, int propertyKey, ColorInt value)
{
  blendValue(object, propertyKey, Kind::color, true)->bits = value;
}

bool BlendAccumulator::applyDouble(
    Core* object,
    int propertyKey,
    float mix,
    float value)
{
  propertyKey = storageKey(propertyKey);
  if (m_layer == nullptr)
  {
    Value* current = blendValue(object, propertyKey, Kind::number, false);
    current->doubleValue = mixKeyedDouble(current->doubleValue, value, mix);
    return true;
  }
  Value* current = layerValue(object, propertyKey);
  if (current == nullptr)
  {
    return false;
  }
  float mixed = mixKeyedDouble(current->doubleValue, value, mix);
  // Compared the way the generated setters compare.
  bool changes = mixed != current->doubleValue;
  current->doubleValue = mixed;
  if (changes)
  {
    changed(*current);
  }
  return true;
}

bool BlendAccumulator::applyColor(
    Core* object,
    int propertyKey,
    float mix,
    ColorInt value)
{
  if (m_layer == nullptr)
  {
    Value* current = blendValue(object, propertyKey, Kind::color, false);
    current->bits = mix == 1.0f ? value : colorLerp(current->bits, value, mix);
    return true;
  }
  Value* current = layerValue(object, propertyKey);
  if (current == nullptr)
  {
    return false;
  }
  ColorInt mixed = mix == 1.0f ? value : colorLerp(current->bits, value, mix);
  bool changes = mixed != current->bits;
  current->bits = mixed;
  if (changes)
  {
    changed(*current);
  }
  return true;
}

bool BlendAccumulator::applyDiscrete(
    Core* object,
    int propertyKey,
    uint32_t value)
{
  if (m_layer == nullptr)
  {
    // Blend states set discrete values themselves, as each animation
    // applies, unless the machine collects them.
    return m_parent != nullptr &&
           m_parent->applyDiscrete(object, propertyKey, value);
  }
  Value* current = layerValue(object, propertyKey);
  if (current == nullptr)
  {
    return false;
  }
  bool changes = value != current->bits;
  current->bits = value;
  if (changes)
  {
    changed(*current);
  }
  return true;
}

bool BlendAccumulator::applyString(
    Core* object,
    int propertyKey,
    const std::string& value)
{
  if (m_layer == nullptr)
  {
    return m_parent != nullptr &&
           m_parent->applyString(object, propertyKey, value);
  }
  Value* current = layerValue(object, propertyKey);
  if (current == nullptr)
  {
    return false;
  }
  std::string& pending = m_layer->strings[current->bits];
  if (pending != value)
  {
    pending = value;
    changed(*current);
  }
  return true;
}

void BlendAccumulator::set(const Value& value)
{
  switch (value.kind)
  {
    case Kind::number:
      CoreRegistry::setDouble(
          value.object,
          value.propertyKey,
          value.doubleValue);
      break;
    case Kind::color:
      CoreRegistry::setColor(value.object, value.propertyKey, value.bits);
      break;
    case Kind::uint:
      CoreRegistry::setUint(value.object, value.propertyKey, value.bits);
      break;
    case Kind::id:
#ifndef WITH_RIVE_EDITOR
      CoreRegistry::setId(value.object, value.propertyKey, value.bits);
#endif
      break;
    case Kind::boolean:
      CoreRegistry::setBool(value.object, value.propertyKey, value.bits != 0);
      break;
    case Kind::string:
      CoreRegistry::setString(
          value.object,
          value.propertyKey,
          m_layer->strings[value.bits]);
      break;
  }
}

void BlendAccumulator::flush()
{
  // Every flush starts a new frame, even one with nothing to set: a layer
  // mode accumulator decides what to collect from one frame to the next.
  // Wrapping back to a frame some value still carries would skip loading
  // it, so start the count over with every value marked unwritten.
  if (++m_frame == 0)
  {
    for (Value& value : m_values)
    {
      value.frame = 0;
    }
    m_frame = 1;
  }
  if (m_written.empty())
  {
    return;
  }
  // Setters run hooks, so work through the list moved aside: nothing they
  // do can reach it. Swapped back below to keep its capacity.
  std::vector<uint32_t> flushing;
  flushing.swap(m_written);
  for (uint32_t written : flushing)
  {
    const Value& value = m_values[written];
    // A blend state's go to the machine's accumulator when it handles them,
    // which may set them straight away itself. Blend mode only collects
    // numbers and colors.
    bool forwarded = false;
    if (m_parent != nullptr)
    {
      forwarded = value.kind == Kind::color ? m_parent->applyColor(
                                                  value.object,
                                                  value.propertyKey,
                                                  1.0f,
                                                  value.bits)
                                            : m_parent->applyDouble(
                                                  value.object,
                                                  value.propertyKey,
                                                  1.0f,
                                                  value.doubleValue);
    }
    if (!forwarded)
    {
      set(value);
    }
  }
  flushing.clear();
  if (m_written.empty())
  {
    m_written.swap(flushing);
  }
}
