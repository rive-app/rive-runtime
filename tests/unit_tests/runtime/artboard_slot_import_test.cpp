#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/node.hpp"
#include "rive/generated/component_base.hpp"
#include "rive/generated/core_registry.hpp"
#include "rive/generated/inputs/keyboard_input_base.hpp"
#include "rive/generated/node_base.hpp"
#include "rive/generated/script_input_number_base.hpp"
#include "rive/generated/scripted/scripted_drawable_base.hpp"
#include "rive/generated/scripted/scripted_interpolator_base.hpp"
#include "rive/scripted/scripted_object.hpp"
#include "riv_bytes.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// Ids in a .riv are indices into the artboard's object list, which the
// exporter numbers in stream order. An object whose import() failed before it
// reached the ArtboardImporter used to leave no slot behind, so every object
// after it moved down one index and its id resolved to the next object over.
// File::readObjects now claims the slot before import() and empties it on
// failure.

static void writeNode(RivBytes& riv, const char* name, uint32_t parentId)
{
    riv.object(rive::NodeBase::typeKey);
    riv.propString(rive::ComponentBase::namePropertyKey, name);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, parentId);
    riv.end();
}

// Exporter numbering for nodes written from firstIndex: first = firstIndex,
// second = firstIndex + 1, child = firstIndex + 2 parented to first.
static void writeNodesFrom(RivBytes& riv, uint32_t firstIndex)
{
    writeNode(riv, "first", 0);
    writeNode(riv, "second", 0);
    writeNode(riv, "child", firstIndex);
}

static void writeNodesAfterSlot1(RivBytes& riv) { writeNodesFrom(riv, 2); }

static void requireNodesAfterSlot1(rive::File* file)
{
    REQUIRE(file != nullptr);
    auto artboard = file->artboard();
    REQUIRE(artboard != nullptr);
    auto child = artboard->find<rive::Node>("child");
    REQUIRE(child != nullptr);
    REQUIRE(child->parent() != nullptr);
    CHECK(child->parent()->name() == "first");
}

// No runtime type has this key: it stands in for a scripted object type from
// a newer exporter.
static const uint16_t unknownTypeKey = 16000;

static void writeUnknownObject(RivBytes& riv)
{
    REQUIRE(rive::CoreRegistry::makeCoreInstance(unknownTypeKey) == nullptr);
    riv.object(unknownTypeKey);
    riv.end();
}

static void writeScriptInput(RivBytes& riv, uint32_t parentId)
{
    riv.object(rive::ScriptInputNumberBase::typeKey);
    riv.propUint(rive::ComponentBase::parentIdPropertyKey, parentId);
    riv.end();
}

// The exporter numbers a script input as an artboard child only when its owner
// is a Component, and then names that owner's index as its parent; inputs of
// interpolators, converters, listener actions and transition conditions have
// parent 0. The parent alone decides the slot, even when the owner (which takes
// an empty slot as an unknown object) can't be read.
TEST_CASE("a script input of an unreadable interpolator takes no slot",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    writeUnknownObject(riv); // 1: the owner
    writeScriptInput(riv, 0);
    writeNodesAfterSlot1(riv);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    requireNodesAfterSlot1(file.get());
    auto& objects = file->artboard()->objects();
    REQUIRE(objects.size() == 5);
    CHECK(objects[1] == nullptr);
}

TEST_CASE("a script input of an unreadable component keeps its slot",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    writeUnknownObject(riv);  // 1: the owner
    writeScriptInput(riv, 1); // 2
    writeNodesFrom(riv, 3);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    requireNodesAfterSlot1(file.get());
    auto& objects = file->artboard()->objects();
    REQUIRE(objects.size() == 6);
    // Neither the owner nor its input could be kept.
    CHECK(objects[1] == nullptr);
    CHECK(objects[2] == nullptr);
}

// The scripted object importer stays open until the next scripted object, so
// an unreadable owner's inputs used to find the scripted drawable read before
// it: they joined the drawable's inputs and took slots as its children.
TEST_CASE("a script input of an unreadable owner ignores the previous one",
          "[file][malformed][scripting]")
{
    for (bool componentOwner : {false, true})
    {
        RivBytes riv;
        writeArtboard(riv);
        riv.object(rive::ScriptedDrawableBase::typeKey); // 1
        riv.propUint(rive::ComponentBase::parentIdPropertyKey, 0);
        riv.end();
        writeUnknownObject(riv); // 2: the unreadable owner
        uint32_t next = 3;
        if (componentOwner)
        {
            writeScriptInput(riv, 2); // 3
            next = 4;
        }
        else
        {
            writeScriptInput(riv, 0); // unnumbered
        }
        writeNodesFrom(riv, next);

        rive::ImportResult result;
        auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
        REQUIRE(result == rive::ImportResult::success);
        requireNodesAfterSlot1(file.get());
        auto& objects = file->artboard()->objects();
        REQUIRE(objects.size() == next + 3);
        auto drawable = rive::ScriptedObject::from(objects[1]);
        REQUIRE(drawable != nullptr);
        CHECK(drawable->customProperties().empty());
    }
}

// Not a Component, but it lives in the artboard's object list. With no
// listener input type open its import fails.
TEST_CASE("a keyboard input with no listener keeps its artboard slot",
          "[file][malformed]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(rive::KeyboardInputBase::typeKey);
    riv.end();
    writeNodesAfterSlot1(riv);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    requireNodesAfterSlot1(file.get());
    CHECK(file->artboard()->objects()[1] == nullptr);
}

// The other direction: an input of a scripted object that is not a Component
// is not an artboard child, the exporter does not number it, and it must not
// take a slot.
TEST_CASE("a scripted interpolator's input takes no artboard slot",
          "[file][scripting]")
{
    RivBytes riv;
    writeArtboard(riv);
    riv.object(rive::ScriptedInterpolatorBase::typeKey);
    riv.end();
    riv.object(rive::ScriptInputNumberBase::typeKey);
    riv.end();
    writeNodesAfterSlot1(riv);

    rive::ImportResult result;
    auto file = rive::File::import(riv.bytes(), &gNoOpFactory, &result);
    REQUIRE(result == rive::ImportResult::success);
    requireNodesAfterSlot1(file.get());
    auto& objects = file->artboard()->objects();
    REQUIRE(objects.size() == 5);
    REQUIRE(objects[1] != nullptr);
    const uint16_t interpolatorKey = rive::ScriptedInterpolatorBase::typeKey;
    CHECK(objects[1]->coreType() == interpolatorKey);
}
