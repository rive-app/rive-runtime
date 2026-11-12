#include "rive/animation/deferred_animation_body.hpp"
#include "rive/animation/keyframe_interpolator.hpp"
#include "rive/artboard.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/reader.h"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/generated/animation/interpolating_keyframe_base.hpp"
#include "rive/generated/animation/keyed_object_base.hpp"
#include "rive/generated/animation/keyed_property_base.hpp"
#include "rive/generated/animation/keyframe_base.hpp"
#include "rive/generated/animation/keyframe_bool_base.hpp"
#include "rive/generated/animation/keyframe_callback_base.hpp"
#include "rive/generated/animation/keyframe_color_base.hpp"
#include "rive/generated/animation/keyframe_double_base.hpp"
#include "rive/generated/animation/keyframe_id_base.hpp"
#include "rive/generated/animation/keyframe_int_base.hpp"
#include "rive/generated/animation/keyframe_string_base.hpp"
#include "rive/generated/animation/keyframe_uint_base.hpp"
#include "rive/generated/animation/linear_animation_base.hpp"
#include "rive/generated/animation/state_machine_base.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/generated/data_bind/data_bind_base.hpp"
#include "rive/generated/data_bind/data_bind_context_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/importers/runtime_object_reader.hpp"

#include <limits>

using namespace rive;

namespace
{
enum class BodyRecord
{
  keyedObject,
  keyedProperty,
  keyFrame,
  // Starts whatever follows the body.
  end,
  dataBind,
  unexpected,
};

BodyRecord bodyRecord(int typeKey)
{
  switch (typeKey)
  {
    case KeyedObjectBase::typeKey:
      return BodyRecord::keyedObject;
    case KeyedPropertyBase::typeKey:
      return BodyRecord::keyedProperty;
    case KeyFrameDoubleBase::typeKey:
    case KeyFrameColorBase::typeKey:
    case KeyFrameBoolBase::typeKey:
    case KeyFrameIdBase::typeKey:
    case KeyFrameIntBase::typeKey:
    case KeyFrameUintBase::typeKey:
    case KeyFrameStringBase::typeKey:
    case KeyFrameCallbackBase::typeKey:
      return BodyRecord::keyFrame;
    case LinearAnimationBase::typeKey:
    case StateMachineBase::typeKey:
    case ArtboardBase::typeKey:
      return BodyRecord::end;
    case DataBindBase::typeKey:
    case DataBindContextBase::typeKey:
      return BodyRecord::dataBind;
  }
  // Includes the abstract KeyFrame and InterpolatingKeyFrame, which the
  // runtime reads as unknown objects.
  return BodyRecord::unexpected;
}

// The uint properties the body's own types deserialize, and so range check
// to 32 bits (CoreUintType, CoreIdType, CoreIntType). Any other uint is
// skipped in full 64 bit range, exactly as readRuntimeObject does.
bool readsUint32(int typeKey, uint16_t propertyKey)
{
  switch (typeKey)
  {
    case KeyedObjectBase::typeKey:
      return propertyKey == KeyedObjectBase::objectIdPropertyKey;
    case KeyedPropertyBase::typeKey:
      return propertyKey == KeyedPropertyBase::propertyKeyPropertyKey;
    case KeyFrameCallbackBase::typeKey:
      return propertyKey == KeyFrameBase::framePropertyKey;
  }
  switch (propertyKey)
  {
    case KeyFrameBase::framePropertyKey:
    case InterpolatingKeyFrameBase::interpolationTypePropertyKey:
    case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
      return true;
    case KeyFrameIdBase::valuePropertyKey:
      return typeKey == KeyFrameIdBase::typeKey;
    case KeyFrameIntBase::valuePropertyKey:
      return typeKey == KeyFrameIntBase::typeKey;
    case KeyFrameUintBase::valuePropertyKey:
      return typeKey == KeyFrameUintBase::typeKey;
    case DataBindBase::propertyKeyPropertyKey:
    case DataBindBase::flagsPropertyKey:
    case DataBindBase::converterIdPropertyKey:
      return typeKey == DataBindBase::typeKey ||
             typeKey == DataBindContextBase::typeKey;
  }
  return false;
}

// Reads a varuint the way BinaryReader does. False where it would fail.
bool readVarUint(const uint8_t*& p, const uint8_t* limit, uint64_t* value)
{
  if (p < limit && *p < 0x80)
  {
    *value = *p++;
    return true;
  }
  size_t length = decode_uint_leb(p, limit, value);
  p += length;
  return length != 0;
}

// Reads past a property value of the field type [fieldId] the way
// readRuntimeObject does, leaving a uint's value in [uintValue]. False where
// it would fail.
bool readValue(
    const uint8_t*& p,
    const uint8_t* limit,
    int fieldId,
    uint64_t* uintValue)
{
  switch (fieldId)
  {
    case -1:
      return false;
    case CoreUintType::id:
      return readVarUint(p, limit, uintValue);
    case CoreBoolType::id:
      if (p >= limit)
      {
        return false;
      }
      p++;
      return true;
    case CoreStringType::id:
    {
      // Strings and bytes: a length, then that many bytes.
      uint64_t length;
      if (!readVarUint(p, limit, &length) || length > (uint64_t)(limit - p))
      {
        return false;
      }
      p += length;
      return true;
    }
    case CoreDoubleType::id:
    case CoreColorType::id:
      // A float32 or a 32 bit color.
      if (limit - p < 4)
      {
        return false;
      }
      p += 4;
      return true;
  }
  // skipRuntimePropertyValue reads nothing for these either.
  return true;
}
} // namespace

AnimationBodySkimResult rive::skimAnimationBody(
    BinaryReader& reader,
    const RuntimeHeader& header)
{
  const int8_t* fieldIds = knownPropertyFieldIds();
  AnimationBodySkimResult result;
  // Read with a bare pointer, as skimArtboardBody does: it fails exactly
  // where BinaryReader and readRuntimeObject would.
  const uint8_t* const start = reader.position();
  const uint8_t* const limit = reader.endPosition();
  const uint8_t* p = start;
  // Where the body ends: at the start of the record that follows it.
  const uint8_t* bodyEnd = start;
  bool inKeyedObject = false;
  bool inKeyedProperty = false;
  // Whether the last record that wasn't a bind was a keyframe: binds are
  // written right after their target.
  bool afterKeyFrame = false;
  // The current keyed object's id. 0, the artboard itself, is its default.
  uint32_t keyedObjectId = 0;
  while (true)
  {
    bodyEnd = p;
    if (p >= limit)
    {
      break;
    }
    uint64_t typeKey64;
    if (!readVarUint(p, limit, &typeKey64) ||
        typeKey64 > (uint64_t)std::numeric_limits<int>::max())
    {
      result.outcome = AnimationBodySkim::readError;
      return result;
    }
    int typeKey = (int)typeKey64;
    switch (bodyRecord(typeKey))
    {
      case BodyRecord::keyedObject:
        inKeyedObject = true;
        inKeyedProperty = false;
        afterKeyFrame = false;
        keyedObjectId = 0;
        break;
      case BodyRecord::keyedProperty:
        if (!inKeyedObject)
        {
          result.outcome = AnimationBodySkim::outOfOrder;
          return result;
        }
        inKeyedProperty = true;
        afterKeyFrame = false;
        result.hasKeyedProperty = true;
        break;
      case BodyRecord::keyFrame:
        if (!inKeyedProperty)
        {
          result.outcome = AnimationBodySkim::outOfOrder;
          return result;
        }
        afterKeyFrame = true;
        break;
      case BodyRecord::end:
        goto bodyEnd;
      case BodyRecord::dataBind:
        if (!afterKeyFrame)
        {
          // Not expected in any exported file, only a guard (see
          // AnimationBodySkim::dataBind).
          result.outcome = AnimationBodySkim::dataBind;
          return result;
        }
        result.hasKeyFrameBinds = true;
        break;
      case BodyRecord::unexpected:
        result.outcome = AnimationBodySkim::unexpectedType;
        return result;
    }
    // The record's properties, read the way readRuntimeObject would. A key
    // read twice keeps its last value, as it does there.
    uint32_t keyedPropertyKey = Core::invalidPropertyKey;
    while (true)
    {
      uint64_t key;
      if (!readVarUint(p, limit, &key) ||
          key > std::numeric_limits<uint16_t>::max())
      {
        result.outcome = AnimationBodySkim::readError;
        return result;
      }
      if (key == 0)
      {
        break;
      }
      uint16_t propertyKey = (uint16_t)key;
      int fieldId = fieldIds[propertyKey];
      if (fieldId == -1)
      {
        fieldId = header.propertyFieldId(propertyKey);
      }
      uint64_t value = 0;
      if (!readValue(p, limit, fieldId, &value))
      {
        result.outcome = AnimationBodySkim::readError;
        return result;
      }
      if (fieldId == CoreUintType::id && readsUint32(typeKey, propertyKey))
      {
        // The runtime reads these in 32 bits and fails the import on a
        // larger one.
        if (value > std::numeric_limits<uint32_t>::max())
        {
          result.outcome = AnimationBodySkim::readError;
          return result;
        }
        if (typeKey == KeyedObjectBase::typeKey)
        {
          keyedObjectId = (uint32_t)value;
        }
        else if (typeKey == KeyedPropertyBase::typeKey)
        {
          keyedPropertyKey = (uint32_t)value;
        }
      }
    }
    if (typeKey == KeyedPropertyBase::typeKey)
    {
      if (keyedPropertyKey == LayoutComponentBase::clipPropertyKey)
      {
        result.keyedClipObjectIds.push_back(keyedObjectId);
      }
      if (CoreRegistry::isCallback(keyedPropertyKey))
      {
        result.hasKeyedCallbacks = true;
      }
    }
  }
bodyEnd:
  if (bodyEnd == start)
  {
    result.outcome = AnimationBodySkim::empty;
    return result;
  }
  result.outcome = AnimationBodySkim::deferred;
  result.bytes = Span<const uint8_t>(start, (size_t)(bodyEnd - start));
  reader.readBytes(result.bytes.size());
  return result;
}

void rive::forEachKeptKeyedProperty(
    const DeferredAnimationBody& body,
    const RuntimeHeader& header,
    const std::function<void(uint32_t objectId, uint32_t propertyKey)>& visit,
    const std::function<void(uint32_t converterId)>& visitBind)
{
  const int8_t* fieldIds = knownPropertyFieldIds();
  const Artboard* artboard = body.artboard;
  const uint8_t* p = body.bytes.data();
  const uint8_t* const limit = p + body.bytes.size();
  // A decode validates each keyed object whole (KeyedObject::onAddedDirty),
  // so an object's properties wait here until it ends: one keyframe naming
  // an interpolator that doesn't resolve drops them all.
  uint32_t objectId = 0;
  Core* object = nullptr;
  bool objectKept = false;
  std::vector<uint32_t> objectKeys;
  // The converter ids of the binds on the kept properties' keyframes.
  std::vector<uint32_t> objectBindConverters;
  uint32_t propertyKey = Core::invalidPropertyKey;
  bool propertyKept = false;
  bool propertyHasKeyFrame = false;
  auto endProperty = [&]() {
    if (propertyKept && propertyHasKeyFrame)
    {
      objectKeys.push_back(propertyKey);
    }
    propertyKept = false;
    propertyHasKeyFrame = false;
  };
  auto endObject = [&]() {
    endProperty();
    if (objectKept)
    {
      for (uint32_t key : objectKeys)
      {
        visit(objectId, key);
      }
      for (uint32_t converterId : objectBindConverters)
      {
        visitBind(converterId);
      }
    }
    objectKeys.clear();
    objectBindConverters.clear();
    objectKept = false;
  };
  while (p < limit)
  {
    uint64_t typeKey64;
    if (!readVarUint(p, limit, &typeKey64))
    {
      break;
    }
    int typeKey = (int)typeKey64;
    // The record's ids, at their defaults until read. A key read twice
    // keeps its last value, as it does in readRuntimeObject.
    uint32_t recordObjectId = 0;
    uint32_t recordPropertyKey = Core::invalidPropertyKey;
    uint32_t interpolatorId = 0xFFFFFFFFu;
    uint32_t converterId = 0xFFFFFFFFu;
    bool read = true;
    while (true)
    {
      uint64_t key;
      if (!readVarUint(p, limit, &key) ||
          key > std::numeric_limits<uint16_t>::max())
      {
        read = false;
        break;
      }
      if (key == 0)
      {
        break;
      }
      uint16_t recordKey = (uint16_t)key;
      int fieldId = fieldIds[recordKey];
      if (fieldId == -1)
      {
        fieldId = header.propertyFieldId(recordKey);
      }
      uint64_t value = 0;
      if (!readValue(p, limit, fieldId, &value))
      {
        read = false;
        break;
      }
      if (fieldId != CoreUintType::id)
      {
        continue;
      }
      switch (recordKey)
      {
        case KeyedObjectBase::objectIdPropertyKey:
          recordObjectId = (uint32_t)value;
          break;
        case KeyedPropertyBase::propertyKeyPropertyKey:
          recordPropertyKey = (uint32_t)value;
          break;
        case InterpolatingKeyFrameBase::interpolatorIdPropertyKey:
          interpolatorId = (uint32_t)value;
          break;
        case DataBindBase::converterIdPropertyKey:
          converterId = (uint32_t)value;
          break;
      }
    }
    if (!read)
    {
      // The skim read these same bytes cleanly when it deferred them.
      break;
    }
    switch (bodyRecord(typeKey))
    {
      case BodyRecord::keyedObject:
        endObject();
        objectId = recordObjectId;
        object = artboard->resolve(objectId);
        objectKept = object != nullptr;
        break;
      case BodyRecord::keyedProperty:
        endProperty();
        propertyKey = recordPropertyKey;
        propertyKept = objectKept && CoreRegistry::objectSupportsProperty(
                                         object,
                                         propertyKey);
        break;
      case BodyRecord::keyFrame:
        propertyHasKeyFrame = true;
        // Every keyframe but a callback interpolates (InterpolatingKeyFrame,
        // whose onAddedDirty resolves its interpolator).
        if (propertyKept && typeKey != KeyFrameCallbackBase::typeKey &&
            interpolatorId != 0xFFFFFFFFu)
        {
          Core* interpolator = artboard->resolve(interpolatorId);
          if (interpolator == nullptr ||
              !interpolator->is<KeyFrameInterpolator>())
          {
            objectKept = false;
          }
        }
        break;
      case BodyRecord::dataBind:
        // On the property's last keyframe (the skim deferred no other bind).
        if (propertyKept && visitBind != nullptr)
        {
          objectBindConverters.push_back(converterId);
        }
        break;
      default:
        break;
    }
  }
  endObject();
}
