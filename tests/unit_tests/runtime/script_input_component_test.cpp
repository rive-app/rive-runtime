#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/node.hpp"
#include "rive/script_input_component.hpp"
#include "rive/shapes/shape.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/shapes/shape_base.hpp"
#include "rive/generated/shapes/path_base.hpp"
#include "rive/generated/script_input_component_base.hpp"
#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// A NodeRef input stores its target as an Id the exporter remaps to the
// target's index in the artboard. The script holds index + 1 (0 for none)
// and asks the host for the node on each read, naming the type it declared.

namespace
{
const uint32_t kShape = 1;
const uint32_t kNode = 2;
const uint32_t kDrawable = 3;

void writeComponentInput(
    RivBytes& riv,
    const char* name,
    uint32_t parentId,
    uint64_t componentId)
{
  riv.object(rive::ScriptInputComponentBase::typeKey);
  riv.propString(rive::ComponentBase::namePropertyKey, name);
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, parentId);
  riv.propUint(
      rive::ScriptInputComponentBase::componentIdPropertyKey,
      componentId);
  riv.end();
}

std::vector<uint8_t> nodeRefFile()
{
  RivBytes riv;
  writeArtboard(riv);                   // 0
  riv.object(rive::ShapeBase::typeKey); // 1
  riv.propString(rive::ComponentBase::namePropertyKey, "caster");
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
  riv.end();
  riv.object(rive::NodeBase::typeKey); // 2
  riv.propString(rive::ComponentBase::namePropertyKey, "plain");
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
  riv.end();
  riv.object(rive::ScriptedDrawableBase::typeKey); // 3
  riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
  riv.end();
  writeComponentInput(riv, "caster", kDrawable, kShape);    // 4
  writeComponentInput(riv, "plain", kDrawable, kNode);      // 5
  writeComponentInput(riv, "missing", kDrawable, 99);       // 6
  writeComponentInput(riv, "unset", kDrawable, 0xFFFFFFFF); // 7
  writeComponentInput(riv, "artboard", kDrawable, 0);       // 8
  return riv.bytes();
}

rive::ScriptInputComponent* input(rive::Artboard* artboard, uint32_t index)
{
  auto object = artboard->objects()[index];
  REQUIRE(object != nullptr);
  REQUIRE(object->is<rive::ScriptInputComponent>());
  return object->as<rive::ScriptInputComponent>();
}
} // namespace

TEST_CASE("a component script input resolves its target", "[scripting]")
{
  auto bytes = nodeRefFile();
  rive::ImportResult result;
  auto file = rive::File::import(bytes, &gNoOpFactory, &result);
  REQUIRE(result == rive::ImportResult::success);
  auto artboard = file->artboard();
  REQUIRE(artboard->objects().size() == 9);

  auto caster = input(artboard, 4);
  CHECK(caster->target() == artboard->objects()[kShape]);
  CHECK(caster->nodeRefId() == kShape + 1);
  CHECK(input(artboard, 5)->target() == artboard->objects()[kNode]);
  CHECK(input(artboard, 5)->nodeRefId() == kNode + 1);
  // Nothing at that index, or no target at all: the script gets 0.
  CHECK(input(artboard, 6)->target() == nullptr);
  CHECK(input(artboard, 6)->nodeRefId() == 0);
  CHECK(input(artboard, 7)->target() == nullptr);
  CHECK(input(artboard, 7)->nodeRefId() == 0);
  // The artboard is a Node, but no node in itself.
  CHECK(input(artboard, 8)->target() == nullptr);
  CHECK(input(artboard, 8)->nodeRefId() == 0);

  auto drawable = rive::ScriptedObject::from(artboard->objects()[kDrawable]);
  REQUIRE(drawable != nullptr);
  CHECK(drawable->customProperties().size() == 5);
}

TEST_CASE("a NodeRef reads any transform in its artboard", "[scripting]")
{
  auto bytes = nodeRefFile();
  rive::ImportResult result;
  auto file = rive::File::import(bytes, &gNoOpFactory, &result);
  REQUIRE(result == rive::ImportResult::success);
  auto artboard = file->artboard();
  auto drawable = rive::ScriptedObject::from(artboard->objects()[kDrawable]);
  REQUIRE(drawable != nullptr);
  auto shape = artboard->objects()[kShape];
  auto node = artboard->objects()[kNode];

  CHECK(drawable->nodeRefTarget(kShape + 1) == shape);
  CHECK(drawable->nodeRefTarget(kNode + 1) == node);
  // 0 is none; the artboard is no target; past the end is nothing.
  CHECK(drawable->nodeRefTarget(0) == nullptr);
  CHECK(drawable->nodeRefTarget(1) == nullptr);
  CHECK(drawable->nodeRefTarget(100) == nullptr);
  // A script input is no transform.
  CHECK(drawable->nodeRefTarget(5) == nullptr);
}

TEST_CASE("a NodeRef names the node in its own instance", "[scripting]")
{
  auto bytes = nodeRefFile();
  rive::ImportResult result;
  auto file = rive::File::import(bytes, &gNoOpFactory, &result);
  REQUIRE(result == rive::ImportResult::success);
  auto instance = file->artboardDefault();
  REQUIRE(instance != nullptr);
  auto shape = instance->objects()[kShape];
  REQUIRE(shape != file->artboard()->objects()[kShape]);

  CHECK(input(instance.get(), 4)->target() == shape);
  auto drawable = rive::ScriptedObject::from(instance->objects()[kDrawable]);
  REQUIRE(drawable != nullptr);
  CHECK(drawable->nodeRefTarget(kShape + 1) == shape);
}
