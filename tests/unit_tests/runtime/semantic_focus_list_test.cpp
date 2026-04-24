/*
 * Copyright 2026 Rive
 */

// Lock-in for focus_nodes_list_order.riv: four button nodes authored at
// root level at distinct vertical positions. The file is shaped so the
// top-to-bottom visual order (0, 75, 150, 225) disagrees with the
// manager-local id-assignment order (the bottom-most button has the
// smallest id). The diff must present the nodes in visual order.

#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include "rive_file_reader.hpp"
#include "semantic_test_helpers.hpp"
#include <catch.hpp>

using namespace rive;
using namespace rive::semantic_test;

namespace
{

struct ExpectedBounds
{
    float minX, minY, maxX, maxY;
};

constexpr ExpectedBounds kExpectedSlots[] = {
    {0.0f, 0.0f, 122.0f, 59.0f},
    {0.0f, 75.0f, 122.0f, 134.0f},
    {0.0f, 150.0f, 122.0f, 209.0f},
    {0.0f, 225.0f, 500.0f, 500.0f},
};
constexpr size_t kExpectedSlotCount =
    sizeof(kExpectedSlots) / sizeof(kExpectedSlots[0]);

} // namespace

TEST_CASE("focus_nodes_list_order.riv: four root buttons in visual order",
          "[semantics][ordering]")
{
    auto f = loadFixture("assets/semantic/focus_nodes_list_order.riv");
    REQUIRE(f.sm != nullptr);
    auto* mgr = f.sm->semanticManager();
    REQUIRE(mgr != nullptr);
    auto diff = mgr->drainDiff();

    REQUIRE(diff.added.size() == kExpectedSlotCount);

    // All four are top-level buttons.
    for (const auto& n : diff.added)
    {
        CHECK(n.parentId == -1);
        CHECK(n.role == static_cast<uint32_t>(SemanticRole::button));
    }

    // diff.added is emitted in tree pre-order, so indexing it matches
    // visual order directly.
    for (size_t i = 0; i < kExpectedSlotCount; ++i)
    {
        const auto& n = diff.added[i];
        CAPTURE(i, n.id);
        CHECK(n.siblingIndex == i);
        CHECK(n.minX == Approx(kExpectedSlots[i].minX));
        CHECK(n.minY == Approx(kExpectedSlots[i].minY));
        CHECK(n.maxX == Approx(kExpectedSlots[i].maxX));
        CHECK(n.maxY == Approx(kExpectedSlots[i].maxY));
    }

    // The root-level childrenUpdated entry must list the ids in the same
    // order as diff.added, locking in that visual order drove the sort.
    const SemanticsChildrenUpdate* rootUpdate = nullptr;
    for (const auto& cu : diff.childrenUpdated)
    {
        if (cu.parentId == -1)
        {
            rootUpdate = &cu;
            break;
        }
    }
    REQUIRE(rootUpdate != nullptr);
    REQUIRE(rootUpdate->childIds.size() == kExpectedSlotCount);
    for (size_t i = 0; i < kExpectedSlotCount; ++i)
    {
        CAPTURE(i);
        CHECK(rootUpdate->childIds[i] == diff.added[i].id);
    }
}

TEST_CASE("focus_nodes_list_order.riv: bottom button has the smallest id "
          "but sorts last",
          "[semantics][ordering]")
{
    // Guards that the fixture still encodes the id-vs-visual-order mismatch
    // that makes the first test meaningful. If this ever breaks because
    // authoring changed, the sibling-order test above is no longer a
    // regression check for the visual-ordering code path.
    auto f = loadFixture("assets/semantic/focus_nodes_list_order.riv");
    REQUIRE(f.sm != nullptr);
    auto diff = f.sm->semanticManager()->drainDiff();

    REQUIRE(diff.added.size() == kExpectedSlotCount);

    uint32_t minId = diff.added[0].id;
    size_t minIdSlot = 0;
    for (size_t i = 1; i < diff.added.size(); ++i)
    {
        if (diff.added[i].id < minId)
        {
            minId = diff.added[i].id;
            minIdSlot = i;
        }
    }
    CHECK(minIdSlot == kExpectedSlotCount - 1);
}
