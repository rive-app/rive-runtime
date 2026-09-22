#include "rive/artboard.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/aabb.hpp"
#include "rive/shapes/shape.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include <catch.hpp>

// Every artboard is a 200x120 flex row holding a fixed 60x40 sibling and then
// a hug subject whose content is 260x180 -- bigger than the row on both axes,
// so the bound has something to bite.
//
// A hugging axis is measured against the space available from the nearest
// definitely sized ancestor, because every leaf measure clamps to it.
// hugUnbounded (LayoutSizingStyle, riv key 451) drops that bound.

static rive::AABB subjectBounds(rive::Artboard* artboard)
{
    auto* subject = artboard->find<rive::LayoutComponent>("Subject");
    REQUIRE(subject != nullptr);
    return subject->layoutBounds();
}

static rive::AABB participantBounds(rive::Artboard* artboard)
{
    auto* shape = artboard->find<rive::Shape>("Subject");
    REQUIRE(shape != nullptr);
    auto* provider = rive::LayoutNodeProvider::from(shape);
    REQUIRE(provider != nullptr);
    return provider->layoutBounds();
}

static rive::Artboard* advanced(rive::File* file, const char* name)
{
    auto* artboard = file->artboard(name);
    REQUIRE(artboard != nullptr);
    artboard->advance(0.0f);
    return artboard;
}

TEST_CASE("a hug leaf is bounded by the space available to it",
          "[hugunbounded]")
{
    auto file = ReadRiveFile("assets/layout/hug_unbounded.riv");

    // The content is 260x180 but the row offers 200x120, and the leaf measure
    // clamps to what it is offered.
    auto bounded = subjectBounds(advanced(file.get(), "LeafBounded"));
    REQUIRE(bounded.width() == 200.0f);
    REQUIRE(bounded.height() == 120.0f);

    // Same scene, hugUnbounded set: the measure ignores the offer and reports
    // the content, so the subject overflows its row.
    auto unbounded = subjectBounds(advanced(file.get(), "LeafUnbounded"));
    REQUIRE(unbounded.width() == 260.0f);
    REQUIRE(unbounded.height() == 180.0f);
}

TEST_CASE("a hug participant is bounded the same way as a hug leaf",
          "[hugunbounded]")
{
    auto file = ReadRiveFile("assets/layout/hug_unbounded.riv");

    // Shape::measureLayout's participant branch honours the measure mode, so a
    // participant hugs exactly like the non-participant path above rather than
    // escaping the row.
    auto bounded =
        participantBounds(advanced(file.get(), "ParticipantBounded"));
    REQUIRE(bounded.width() == 200.0f);
    REQUIRE(bounded.height() == 120.0f);

    auto unbounded =
        participantBounds(advanced(file.get(), "ParticipantUnbounded"));
    REQUIRE(unbounded.width() == 260.0f);
    REQUIRE(unbounded.height() == 180.0f);
}

TEST_CASE("max sizing still bounds an unbounded hug", "[hugunbounded]")
{
    auto file = ReadRiveFile("assets/layout/hug_unbounded.riv");

    // Yoga applies min/max to whatever the measure returned, so dropping the
    // implicit bound leaves the explicit one in force. 180x150 is below the
    // 260x180 content and above the 200x120 row, so it can only have come
    // from the max.
    auto bounds = subjectBounds(advanced(file.get(), "UnboundedMaxed"));
    REQUIRE(bounds.width() == 180.0f);
    REQUIRE(bounds.height() == 150.0f);
}

TEST_CASE("a fixed axis survives hugUnbounded", "[hugunbounded]")
{
    auto file = ReadRiveFile("assets/layout/hug_unbounded.riv");

    // Only atMost is a bound. exactly means the size is already decided, so
    // unbounding it too would lose the fixed 40 height here -- the width still
    // reaches its 260 content.
    auto bounds = subjectBounds(advanced(file.get(), "UnboundedFixedHeight"));
    REQUIRE(bounds.width() == 260.0f);
    REQUIRE(bounds.height() == 40.0f);
}
