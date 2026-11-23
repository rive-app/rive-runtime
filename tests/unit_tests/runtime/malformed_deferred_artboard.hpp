/*
 * Copyright 2026 Rive
 */

// A malformed file whose deferred artboard fails to decode, shared by
// lazy_artboard_test.cpp and command_queue_test.cpp.

#ifndef _RIVE_MALFORMED_DEFERRED_ARTBOARD_HPP_
#define _RIVE_MALFORMED_DEFERRED_ARTBOARD_HPP_

#include "rive/core/vector_binary_writer.hpp"
#include "rive/file.hpp"
#include "rive/generated/artboard_base.hpp"
#include "rive/generated/backboard_base.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/layout_component_base.hpp"
#include "rive/generated/nested_artboard_base.hpp"
#include "rive/generated/shapes/image_base.hpp"
#include "rive/generated/shapes/mesh_base.hpp"

#include <string>
#include <vector>

namespace lazy_decode_test
{
using namespace rive;

// The second artboard's mesh has no triangle indices, which fails it in
// initialize: an import fails whole, a deferred decode only that artboard.
// The first nests the second. The Apple runtime's
// Tests/Concurrency/rivs/malformed_deferred_artboard.riv holds these bytes;
// regenerate it when they change.
inline std::vector<uint8_t> meshWithoutIndices()
{
  std::vector<uint8_t> bytes;
  VectorBinaryWriter writer(&bytes);
  writer.write((const uint8_t*)"RIVE", 4);
  writer.writeVarUint((uint32_t)File::majorVersion);
  writer.writeVarUint((uint32_t)File::minorVersion);
  writer.writeVarUint((uint32_t)0);
  // No table of contents: every key is one the runtime knows.
  writer.writeVarUint((uint32_t)0);
  auto record = [&](uint16_t typeKey) {
    writer.writeVarUint((uint32_t)typeKey);
  };
  auto uintProperty = [&](uint16_t key, uint32_t value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeVarUint(value);
  };
  auto floatProperty = [&](uint16_t key, float value) {
    writer.writeVarUint((uint32_t)key);
    writer.writeFloat(value);
  };
  auto stringProperty = [&](uint16_t key, const std::string& value) {
    writer.writeVarUint((uint32_t)key);
    writer.write(value);
  };
  auto end = [&]() { writer.writeVarUint((uint32_t)0); };
  auto artboard = [&](const char* name) {
    record(ArtboardBase::typeKey);
    stringProperty(ComponentBase::namePropertyKey, name);
    floatProperty(LayoutComponentBase::widthPropertyKey, 100);
    floatProperty(LayoutComponentBase::heightPropertyKey, 100);
    end();
  };
  record(BackboardBase::typeKey);
  end();
  artboard("first");
  record(NestedArtboardBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 0);
  uintProperty(NestedArtboardBase::artboardIdPropertyKey, 1);
  end();
  artboard("second");
  record(ImageBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 0);
  end();
  record(MeshBase::typeKey);
  uintProperty(ComponentBase::parentIdPropertyKey, 1);
  end();
  return bytes;
}
} // namespace lazy_decode_test

#endif
