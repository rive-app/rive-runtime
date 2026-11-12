#ifndef _RIVE_RUNTIME_OBJECT_READER_HPP_
#define _RIVE_RUNTIME_OBJECT_READER_HPP_

#include <cstdint>

namespace rive
{
class BinaryReader;
class Core;
class RuntimeHeader;

/// Reads one object from a runtime (.riv) stream: its type key, then its
/// properties up to the 0 terminator. Properties the object doesn't know are
/// skipped by their field type (see runtimePropertyFieldId). Returns null for
/// an object type this runtime doesn't know (its properties are still
/// consumed). A property key that neither the runtime nor the file's ToC can
/// type leaves the stream unreadable: nothing after it parses as intended, so
/// [malformed] is set and the caller fails the import instead of treating the
/// rest of the file as a run of unknown objects.
Core* readRuntimeObject(
    BinaryReader& reader,
    const RuntimeHeader& header,
    bool& malformed);

/// The field type id of [propertyKey]: the registry's when it knows the key,
/// otherwise the one the file's table of contents declares, otherwise -1.
int runtimePropertyFieldId(const RuntimeHeader& header, uint16_t propertyKey);

/// The registry's field type id of every property key (-1 for keys it
/// doesn't know), built once per process: the skims look up every property
/// they read past, and a table load beats the registry's switch.
const int8_t* knownPropertyFieldIds();

/// Consumes one property value of field type [fieldId] the way
/// readRuntimeObject skips a property its object doesn't know, without
/// allocating. Uints are read in full 64 bit range, so this never sets the
/// reader's int range error.
void skipRuntimePropertyValue(BinaryReader& reader, int fieldId);
} // namespace rive

#endif
