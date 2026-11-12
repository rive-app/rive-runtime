#include "rive/deferred_artboard_body.hpp"
#include "rive/animation/state_machine_listener.hpp"
#include "rive/artboard.hpp"
#include "rive/assets/file_asset.hpp"
#include "rive/assets/file_asset_contents.hpp"
#include "rive/backboard.hpp"
#include "rive/constraints/scrolling/scroll_physics.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/core/reader.h"
#include "rive/data_bind/converters/data_converter.hpp"
#include "rive/data_bind/converters/data_converter_group_item.hpp"
#include "rive/data_bind/converters/formula/formula_token.hpp"
#include "rive/data_bind/converters/string_template/string_template_argument.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/generated/nested_artboard_base.hpp"
#include "rive/generated/shapes/paint/paint_image_base.hpp"
#include "rive/importers/runtime_object_reader.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/selection_style.hpp"
#include "rive/shapes/paint/paint_image.hpp"
#include "rive/text/text_style_paint.hpp"
#include "rive/viewmodel/data_enum.hpp"
#include "rive/viewmodel/data_enum_value.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive/viewmodel/viewmodel_property.hpp"

#include <limits>
#include <memory>
#include <mutex>

using namespace rive;

namespace
{
// What a type means to the skim. Most types are plain: whatever they are,
// they belong to the artboard whose run holds them.
enum class Kind : uint8_t
{
  plain,
  // Starts the next artboard's run.
  artboard,
  stateMachineListener,
  textStylePaint,
  // Nesting references: an edge in the file's nesting graph.
  nestedArtboard,
  paintImage,
  // Kept eager (see ArtboardBodySkim).
  fileLevel,
};

// Above every type key the runtime knows (the largest is about 1100). A key
// past it, like any key the runtime doesn't know, reads as plain: an import
// turns it into a null object, and so does the decode.
constexpr int kTypeKeyLimit = 4096;

Kind classify(const Core* object)
{
  if (object->is<Artboard>())
  {
    return Kind::artboard;
  }
  if (object->is<StateMachineListener>())
  {
    return Kind::stateMachineListener;
  }
  if (object->is<TextStylePaint>())
  {
    return Kind::textStylePaint;
  }
  if (object->is<NestedArtboard>())
  {
    return Kind::nestedArtboard;
  }
  if (object->is<PaintImage>())
  {
    return Kind::paintImage;
  }
  if (object->is<DataConverter>() || object->is<DataConverterGroupItem>() ||
      object->is<FormulaToken>() || object->is<StringTemplateArgument>() ||
      object->is<ScrollPhysics>() || object->is<FileAsset>() ||
      object->is<FileAssetContents>() || object->is<ViewModel>() ||
      object->is<ViewModelProperty>() || object->is<DataEnum>() ||
      object->is<DataEnumValue>() || object->is<Backboard>() ||
      object->is<SelectionStyle>())
  {
    return Kind::fileLevel;
  }
  return Kind::plain;
}

// Built once per process by making one object of each type the runtime knows
// and asking what it is, so a new subclass of any of the families above is
// classified without anyone listing it.
const std::vector<Kind>& kinds()
{
  static std::vector<Kind> table;
  static std::once_flag once;
  std::call_once(once, [] {
    table.assign(kTypeKeyLimit, Kind::plain);
    for (int typeKey = 0; typeKey < kTypeKeyLimit; typeKey++)
    {
      std::unique_ptr<Core> object(CoreRegistry::makeCoreInstance(typeKey));
      if (object != nullptr)
      {
        table[typeKey] = classify(object.get());
      }
    }
  });
  return table;
}
} // namespace

ArtboardBodySkimResult rive::skimArtboardBody(
    BinaryReader& reader,
    const RuntimeHeader& header)
{
  ArtboardBodySkimResult result;
  const std::vector<Kind>& kindTable = kinds();
  const int8_t* fieldIds = knownPropertyFieldIds();
  // Read with a bare pointer: this passes every byte of the artboards it
  // defers, and a player that needs them all pays it on top of the decode.
  // It fails exactly where BinaryReader and readRuntimeObject would.
  const uint8_t* const start = reader.position();
  const uint8_t* const limit = reader.endPosition();
  const uint8_t* p = start;
  auto readVarUint = [&](uint64_t* value) {
    if (p < limit && *p < 0x80)
    {
      *value = *p++;
      return true;
    }
    size_t length = decode_uint_leb(p, limit, value);
    p += length;
    return length != 0;
  };
  // Where the body ends: at the start of the record that follows it.
  const uint8_t* bodyEnd = start;
  while (true)
  {
    bodyEnd = p;
    if (p >= limit)
    {
      break;
    }
    uint64_t typeKey;
    if (!readVarUint(&typeKey) ||
        typeKey > (uint64_t)std::numeric_limits<int>::max())
    {
      result.outcome = ArtboardBodySkim::readError;
      return result;
    }
    Kind kind = typeKey < (uint64_t)kTypeKeyLimit ? kindTable[typeKey]
                                                  : Kind::plain;
    // The property holding the artboard a nesting reference names, if this
    // record is one.
    uint16_t nestedIdKey = 0;
    switch (kind)
    {
      case Kind::artboard:
        goto bodyEnd;
      case Kind::stateMachineListener:
        result.hasStateMachineListener = true;
        break;
      case Kind::textStylePaint:
        result.hasTextStylePaint = true;
        break;
      case Kind::nestedArtboard:
        nestedIdKey = NestedArtboardBase::artboardIdPropertyKey;
        break;
      case Kind::paintImage:
        nestedIdKey = PaintImageBase::artboardIdPropertyKey;
        break;
      case Kind::fileLevel:
        // Not expected in any exported file, only a guard (see
        // ArtboardBodySkim::fileLevel).
        result.outcome = ArtboardBodySkim::fileLevel;
        return result;
      case Kind::plain:
        break;
    }
    // The record's properties, read the way readRuntimeObject would.
    int64_t nestedId = -1;
    while (true)
    {
      uint64_t key;
      if (!readVarUint(&key) || key > std::numeric_limits<uint16_t>::max())
      {
        result.outcome = ArtboardBodySkim::readError;
        return result;
      }
      if (key == 0)
      {
        break;
      }
      uint16_t propertyKey = (uint16_t)key;
      int fieldId = fieldIds[propertyKey];
      bool known = fieldId != -1;
      if (!known)
      {
        fieldId = header.propertyFieldId(propertyKey);
      }
      switch (fieldId)
      {
        case -1:
          result.outcome = ArtboardBodySkim::readError;
          return result;
        case CoreUintType::id:
        {
          uint64_t value;
          // The runtime reads the uints it knows in 32 bits and fails the
          // import on a larger one. Whether this type is one that knows
          // the key doesn't matter: deciding to import it eagerly is
          // always safe.
          if (!readVarUint(&value) ||
              (value > std::numeric_limits<uint32_t>::max() && known))
          {
            result.outcome = ArtboardBodySkim::readError;
            return result;
          }
          if (propertyKey == nestedIdKey)
          {
            nestedId = (int64_t)value;
          }
          break;
        }
        case CoreBoolType::id:
          if (p >= limit)
          {
            result.outcome = ArtboardBodySkim::readError;
            return result;
          }
          p++;
          break;
        case CoreStringType::id:
        {
          // Strings and bytes: a length, then that many bytes.
          uint64_t length;
          if (!readVarUint(&length) || length > (uint64_t)(limit - p))
          {
            result.outcome = ArtboardBodySkim::readError;
            return result;
          }
          p += length;
          break;
        }
        case CoreDoubleType::id:
        case CoreColorType::id:
          // A float32 or a 32 bit color.
          if (limit - p < 4)
          {
            result.outcome = ArtboardBodySkim::readError;
            return result;
          }
          p += 4;
          break;
        default:
          // skipRuntimePropertyValue reads nothing for these either.
          break;
      }
    }
    if (nestedId != -1)
    {
      result.nestedArtboardIds.push_back((int)nestedId);
    }
  }
bodyEnd:
  if (bodyEnd == start)
  {
    result.outcome = ArtboardBodySkim::empty;
    return result;
  }
  result.outcome = ArtboardBodySkim::deferred;
  result.bytes = Span<const uint8_t>(start, (size_t)(bodyEnd - start));
  reader.readBytes(result.bytes.size());
  return result;
}
