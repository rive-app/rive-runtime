#include "rive/importers/runtime_object_reader.hpp"
#include "rive/core.hpp"
#include "rive/core/binary_reader.hpp"
#include "rive/core/field_types/core_bool_type.hpp"
#include "rive/core/field_types/core_color_type.hpp"
#include "rive/core/field_types/core_double_type.hpp"
#include "rive/core/field_types/core_string_type.hpp"
#include "rive/core/field_types/core_uint_type.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/runtime_header.hpp"

#include <cstdio>
#include <mutex>

using namespace rive;

const int8_t* rive::knownPropertyFieldIds()
{
  static int8_t table[65536];
  static std::once_flag once;
  std::call_once(once, [] {
    for (int key = 0; key < 65536; key++)
    {
      table[key] = (int8_t)CoreRegistry::propertyFieldId(key);
    }
  });
  return table;
}

int rive::runtimePropertyFieldId(
    const RuntimeHeader& header,
    uint16_t propertyKey)
{
  int id = CoreRegistry::propertyFieldId(propertyKey);
  if (id == -1)
  {
    id = header.propertyFieldId(propertyKey);
  }
  return id;
}

void rive::skipRuntimePropertyValue(BinaryReader& reader, int fieldId)
{
  switch (fieldId)
  {
    case CoreUintType::id:
      // Uint64 shares the uint type id; skip the full range so an unknown 64
      // bit value never aborts the read.
      reader.readVarUint64();
      break;
    case CoreBoolType::id:
      // A bool the registry types but the object does not store (a bit of a
      // packed mask, written standalone). Skipping nothing here read the
      // value byte as the next key and desynchronized everything after it.
      CoreBoolType::deserialize(reader);
      break;
    case CoreStringType::id:
      // Strings and bytes share the id: a length, then that many bytes.
      reader.readBytes();
      break;
    case CoreDoubleType::id:
      CoreDoubleType::deserialize(reader);
      break;
    case CoreColorType::id:
      CoreColorType::deserialize(reader);
      break;
  }
}

Core* rive::readRuntimeObject(
    BinaryReader& reader,
    const RuntimeHeader& header,
    bool& malformed)
{
  auto coreObjectKey = reader.readVarUintAs<int>();
  auto object = CoreRegistry::makeCoreInstance(coreObjectKey);
  while (true)
  {
    auto propertyKey = reader.readVarUintAs<uint16_t>();
    if (propertyKey == 0)
    {
      // Terminator. https://media.giphy.com/media/7TtvTUMm9mp20/giphy.gif
      break;
    }

    if (reader.hasError())
    {
      delete object;
      return nullptr;
    }
    if (object == nullptr || !object->deserialize(propertyKey, reader))
    {
      // We have an unknown object or property, first see if core knows the
      // property type, then check the toc.
      int id = runtimePropertyFieldId(header, propertyKey);
      if (id == -1)
      {
        // Still couldn't find it, give up.
        fprintf(
            stderr,
            "Unknown property key %d, missing from property ToC.\n",
            propertyKey);
        delete object;
        malformed = true;
        return nullptr;
      }
      skipRuntimePropertyValue(reader, id);
    }
  }
  if (object == nullptr)
  {
    // fprintf(stderr,
    //         "File contains an unknown object with coreType " RIVE_FMT_U64
    //         ", which " "this runtime doesn't understand.\n",
    //         coreObjectKey);
    return nullptr;
  }
#ifdef WITH_RIVE_EDITOR
  // The .riv importer is exclusively the runtime path (editor flow
  // hydrates via coop, never through here), so imported objects are
  // fully wired by the import stack — mark them validated or the
  // editor build's hasValidated() gate starves every generated
  // ${name}Changed() on runtime instances (frozen animation dirt,
  // silent VMI value callbacks).
  object->markValidated();
#endif
  return object;
}
