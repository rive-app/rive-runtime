/*
 * Copyright 2026 Rive
 */

// Unit tests for SemanticData's lifecycle:
//   - Lazy semanticNode() creation snapshots authored properties.
//   - Property-change setters propagate to the underlying SemanticNode.
//   - Once registered in a manager, property changes emerge as
//     updatedSemantic entries on the next drainDiff().
//   - Removing the SD from the manager cleans up the index.
//
// SD's visibility/collapse path (shouldExcludeFromSemanticTree) and the
// focus-trait auto-detection depend on a parent Node and sibling
// FocusData; those are covered by fixture-driven tests elsewhere because
// constructing that hierarchy in-process would require a full artboard.

#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/refcnt.hpp>
#include <rive/semantic/semantic_data.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <rive/semantic/semantic_trait.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include "rive_file_reader.hpp"
#include "semantic_test_helpers.hpp"
#include <catch.hpp>

using namespace rive;

namespace
{

// Configures a SemanticData's authored properties so the first
// semanticNode() call can snapshot them.
static void authorProperties(SemanticData& sd)
{
    sd.role(static_cast<uint32_t>(SemanticRole::button));
    sd.label("Submit");
    sd.value("");
    sd.hint("Tap to send");
    sd.headingLevel(0);
    sd.traitFlags(static_cast<uint32_t>(SemanticTrait::Enablable));
    sd.stateFlags(0);
}

} // namespace

// ---------------------------------------------------------------------------
// Lazy semanticNode() creation + property snapshot.
// ---------------------------------------------------------------------------

TEST_CASE("hasSemanticNode flips once semanticNode() is called",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    CHECK_FALSE(sd.hasSemanticNode());

    auto node = sd.semanticNode();
    CHECK(sd.hasSemanticNode());
    REQUIRE(node != nullptr);
}

TEST_CASE("semanticNode() returns the same instance on repeated calls",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    auto first = sd.semanticNode();
    auto second = sd.semanticNode();
    CHECK(first.get() == second.get());
}

TEST_CASE("semanticNode() snapshots authored properties onto the node",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    authorProperties(sd);

    auto node = sd.semanticNode();

    CHECK(node->role() == static_cast<uint32_t>(SemanticRole::button));
    CHECK(node->label() == "Submit");
    CHECK(node->hint() == "Tap to send");
    CHECK(hasSemanticTrait(node->traitFlags(), SemanticTrait::Enablable));
    CHECK(node->stateFlags() == 0);
}

TEST_CASE("semanticNode() back-reference points at the owning SemanticData",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    auto node = sd.semanticNode();
    CHECK(node->semanticData() == &sd);
}

// ---------------------------------------------------------------------------
// Property-change setters push to the existing node.
// ---------------------------------------------------------------------------

TEST_CASE("setter changes after first semanticNode() propagate to the node",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    authorProperties(sd);
    auto node = sd.semanticNode();

    sd.role(static_cast<uint32_t>(SemanticRole::link));
    CHECK(node->role() == static_cast<uint32_t>(SemanticRole::link));

    sd.label("Learn more");
    CHECK(node->label() == "Learn more");

    sd.value("$");
    CHECK(node->value() == "$");

    sd.hint("External link");
    CHECK(node->hint() == "External link");

    sd.headingLevel(2);
    CHECK(node->headingLevel() == 2);

    const uint32_t newTraits = static_cast<uint32_t>(SemanticTrait::Enablable |
                                                     SemanticTrait::Expandable);
    sd.traitFlags(newTraits);
    CHECK(node->traitFlags() == newTraits);

    const uint32_t newStates = static_cast<uint32_t>(SemanticState::Selected);
    sd.stateFlags(newStates);
    CHECK(node->stateFlags() == newStates);
}

TEST_CASE("mutating a property does not recreate the SemanticNode",
          "[semantics][lifecycle]")
{
    SemanticData sd;
    authorProperties(sd);
    auto first = sd.semanticNode();

    sd.label("Different");
    sd.role(static_cast<uint32_t>(SemanticRole::link));

    auto second = sd.semanticNode();
    CHECK(first.get() == second.get());
}

// ---------------------------------------------------------------------------
// Property-change → diff plumbing.
//
// SemanticData::markContentDirty() bails out when parent() is nullptr, so a
// bare SemanticData can't exercise the dirty → diff path on its own. The
// pathway is covered by fixture-driven tests in semantic_state_machine_test
// and semantic_artboard_test (tapping an authored dropdown triggers the
// Expanded state flip, which appears in updatedSemantic); it's also
// exercised end-to-end by the incremental-path tests in
// semantic_label_inference_test that drive the manager directly.
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Teardown: destroying the SD removes its node from the manager.
// ---------------------------------------------------------------------------

TEST_CASE("destroying a registered SD removes its node from the manager",
          "[semantics][lifecycle]")
{
    SemanticManager mgr;
    uint32_t capturedId = 0;
    {
        SemanticData sd;
        authorProperties(sd);
        auto node = sd.semanticNode();
        mgr.addChild(nullptr, node);
        capturedId = node->id();
        REQUIRE(mgr.nodeById(capturedId) != nullptr);
    }
    CHECK(mgr.nodeById(capturedId) == nullptr);
}

TEST_CASE("destroying a registered SD emits a removed entry on the next diff",
          "[semantics][lifecycle]")
{
    SemanticManager mgr;
    uint32_t capturedId = 0;
    {
        SemanticData sd;
        authorProperties(sd);
        auto node = sd.semanticNode();
        mgr.addChild(nullptr, node);
        capturedId = node->id();
        (void)mgr.drainDiff();
    }
    const auto diff = mgr.drainDiff();
    REQUIRE(diff.removed.size() == 1);
    CHECK(diff.removed[0] == capturedId);
}

// ---------------------------------------------------------------------------
// Fixture-driven property-change pathway. SemanticData's setters route
// through markContentDirty, which only fires when parent() is non-null; a
// bare SD can't trigger that in isolation, so we drive it end-to-end via
// an authored state machine that flips the dropdown button's Expanded bit.
// ---------------------------------------------------------------------------

TEST_CASE(
    "SD property change authored by state machine appears in updatedSemantic",
    "[semantics][lifecycle]")
{
    using namespace rive::semantic_test;

    auto f = loadFixture("assets/semantic/data_binding_lists.riv");
    auto* mgr = f.sm->semanticManager();
    REQUIRE(mgr != nullptr);

    const auto initial = mgr->drainDiff();
    const auto* initialButton = findAddedByLabel(initial, kDropdownLabel);
    REQUIRE(initialButton != nullptr);
    CHECK(hasSemanticState(initialButton->stateFlags, SemanticState::Expanded));
    const uint32_t buttonId = initialButton->id;

    // Fire tap through the manager's authoritative path; the authored
    // StateMachineListener toggles the Expanded bit.
    f.sm->fireSemanticAction(buttonId, SemanticActionType::tap);
    advance(f.sm.get());
    const auto follow = mgr->drainDiff();

    const SemanticsDiffNode* updated = nullptr;
    for (const auto& n : follow.updatedSemantic)
    {
        if (n.id == buttonId)
        {
            updated = &n;
            break;
        }
    }
    REQUIRE(updated != nullptr);
    CHECK_FALSE(hasSemanticState(updated->stateFlags, SemanticState::Expanded));
}
