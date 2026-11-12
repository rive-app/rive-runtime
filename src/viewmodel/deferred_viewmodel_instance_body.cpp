#include "rive/viewmodel/deferred_viewmodel_instance_body.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/data_bind/data_bind_path.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/importers/runtime_object_reader.hpp"
#include "rive/viewmodel/viewmodel_instance_list.hpp"
#include "rive/viewmodel/viewmodel_instance_list_item.hpp"
#include "rive/viewmodel/viewmodel_instance_value.hpp"

#include <limits>
#include <memory>
#include <mutex>
#include <vector>

using namespace rive;

namespace
{
// What a type means to the skim.
enum class Kind : uint8_t
{
  // Not part of an instance's body: the body ends before it.
  other,
  // A type this runtime doesn't know.
  unknown,
  value,
  // A value that opens the records a list item needs.
  list,
  listItem,
  bind,
  bindPath,
};

Kind classify(const Core* object)
{
  if (object->is<ViewModelInstanceList>())
  {
    return Kind::list;
  }
  if (object->is<ViewModelInstanceValue>())
  {
    return Kind::value;
  }
  if (object->is<ViewModelInstanceListItem>())
  {
    return Kind::listItem;
  }
  if (object->is<DataBind>())
  {
    return Kind::bind;
  }
  if (object->is<DataBindPath>())
  {
    return Kind::bindPath;
  }
  return Kind::other;
}

// Above every type key the runtime knows, as in skimArtboardBody.
constexpr int kTypeKeyLimit = 4096;

// Built once per process from one object of each type the runtime knows, so
// a new value type is part of a body without anyone listing it.
const std::vector<Kind>& kinds()
{
  static std::vector<Kind> table;
  static std::once_flag once;
  std::call_once(once, [] {
    table.assign(kTypeKeyLimit, Kind::unknown);
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

Kind kindOf(int typeKey)
{
  return typeKey >= 0 && typeKey < kTypeKeyLimit ? kinds()[typeKey]
                                                 : Kind::unknown;
}
} // namespace

ViewModelInstanceBodySkimResult rive::skimViewModelInstanceBody(
    BinaryReader& reader,
    const RuntimeHeader& header)
{
  ViewModelInstanceBodySkimResult result;
  const int8_t* fieldIds = knownPropertyFieldIds();
  BinaryReader scan = reader;
  const uint8_t* start = scan.position();
  // Where the body ends: at the start of the record that follows it.
  BinaryReader end = scan;
  // Whether the last record a bind would target is the instance itself.
  bool bindTargetsInstance = true;
  bool sawList = false;
  while (true)
  {
    end = scan;
    if (scan.reachedEnd())
    {
      break;
    }
    auto typeKey = scan.readVarUintAs<int>();
    if (scan.hasError())
    {
      result.outcome = ViewModelInstanceBodySkim::readError;
      return result;
    }
    switch (kindOf(typeKey))
    {
      case Kind::other:
        goto bodyEnd;
      case Kind::unknown:
        result.outcome = ViewModelInstanceBodySkim::unexpectedType;
        return result;
      case Kind::list:
        sawList = true;
        bindTargetsInstance = false;
        break;
      case Kind::value:
      case Kind::bindPath:
        bindTargetsInstance = false;
        break;
      case Kind::listItem:
        if (!sawList)
        {
          result.outcome = ViewModelInstanceBodySkim::outOfOrder;
          return result;
        }
        bindTargetsInstance = false;
        break;
      case Kind::bind:
        // Not expected (see ViewModelInstanceBodySkim::outOfOrder).
        if (bindTargetsInstance)
        {
          result.outcome = ViewModelInstanceBodySkim::outOfOrder;
          return result;
        }
        break;
    }
    // The record's properties, read the way readRuntimeObject would.
    while (true)
    {
      auto propertyKey = scan.readVarUintAs<uint16_t>();
      if (propertyKey == 0)
      {
        break;
      }
      if (scan.hasError())
      {
        result.outcome = ViewModelInstanceBodySkim::readError;
        return result;
      }
      int fieldId = fieldIds[propertyKey];
      bool known = fieldId != -1;
      if (!known)
      {
        fieldId = header.propertyFieldId(propertyKey);
      }
      if (fieldId == -1)
      {
        result.outcome = ViewModelInstanceBodySkim::readError;
        return result;
      }
      if (fieldId != CoreUintType::id)
      {
        skipRuntimePropertyValue(scan, fieldId);
        continue;
      }
      uint64_t value = scan.readVarUint64();
      // The runtime reads the uints it knows in 32 bits and fails the
      // import on a larger one (see skimArtboardBody).
      if (value > std::numeric_limits<uint32_t>::max() && known)
      {
        result.outcome = ViewModelInstanceBodySkim::readError;
        return result;
      }
    }
    if (scan.hasError())
    {
      result.outcome = ViewModelInstanceBodySkim::readError;
      return result;
    }
    result.records++;
  }
bodyEnd:
  if (end.position() == start)
  {
    result.outcome = ViewModelInstanceBodySkim::empty;
    return result;
  }
  result.outcome = ViewModelInstanceBodySkim::deferred;
  result.bytes = Span<const uint8_t>(start, (size_t)(end.position() - start));
  reader = end;
  return result;
}
