/*
 * Copyright 2026 Rive
 */

// Integration tests for Artboard-level semantic tree construction using
// data_binding_lists.riv — a dropdown button that toggles a list of four
// fandom items via data binding.

#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <rive/semantic/semantic_trait.hpp>
#include "rive_file_reader.hpp"
#include "semantic_test_helpers.hpp"
#include <catch.hpp>

#include <set>
#include <string>

using namespace rive;
using namespace rive::semantic_test;

namespace
{

static const std::set<std::string> kFandomLabels = {
    "War of the Stars",
    "Scufflestar Galactica",
    "Galaxy Hike",
    "Dino Planet",
};

// Diff-replay aggregator: holds the current semantic tree as seen through
// drainDiff(), mirroring SemanticTreeModel.applyDiff on the Dart side but
// retaining only the fields these tests assert on.
struct Setup
{
    Fixture fixture;

    struct Entry
    {
        uint32_t id;
        uint32_t role;
        std::string label;
        uint32_t stateFlags;
        uint32_t traitFlags;
        AABB bounds;
    };
    std::unordered_map<uint32_t, Entry> entries;

    StateMachineInstance* sm() const { return fixture.sm.get(); }

    void applyDiff(const SemanticsDiff& diff)
    {
        for (auto id : diff.removed)
        {
            entries.erase(id);
        }
        for (const auto& n : diff.added)
        {
            entries[n.id] = {n.id,
                             n.role,
                             n.label,
                             n.stateFlags,
                             n.traitFlags,
                             AABB(n.minX, n.minY, n.maxX, n.maxY)};
        }
        for (const auto& n : diff.updatedSemantic)
        {
            auto it = entries.find(n.id);
            if (it != entries.end())
            {
                it->second.role = n.role;
                it->second.label = n.label;
                it->second.stateFlags = n.stateFlags;
                it->second.traitFlags = n.traitFlags;
            }
        }
        for (const auto& g : diff.updatedGeometry)
        {
            auto it = entries.find(g.id);
            if (it != entries.end())
            {
                it->second.bounds = AABB(g.minX, g.minY, g.maxX, g.maxY);
            }
        }
    }

    void settle()
    {
        auto* mgr = sm()->semanticManager();
        REQUIRE(mgr != nullptr);
        advance(sm());
        applyDiff(mgr->drainDiff());
    }

    const Entry* findByLabel(const std::string& label) const
    {
        for (const auto& kv : entries)
        {
            if (kv.second.label == label)
            {
                return &kv.second;
            }
        }
        return nullptr;
    }

    std::set<std::string> fandomLabelsPresent() const
    {
        std::set<std::string> out;
        for (const auto& kv : entries)
        {
            if (kv.second.role == static_cast<uint32_t>(SemanticRole::text) &&
                kFandomLabels.count(kv.second.label) > 0)
            {
                out.insert(kv.second.label);
            }
        }
        return out;
    }

    void tapDropdown()
    {
        const auto* button = findByLabel(kDropdownLabel);
        REQUIRE(button != nullptr);
        const Vec2D center((button->bounds.minX + button->bounds.maxX) * 0.5f,
                           (button->bounds.minY + button->bounds.maxY) * 0.5f);
        sm()->pointerDown(center);
        sm()->pointerUp(center);
        settle();
    }
};

// Loads the fixture via the shared helper, then seeds the diff-replay
// aggregator from the initial settle diff so Setup::entries is ready.
static Setup loadDataBindingLists()
{
    Setup s;
    s.fixture = loadFixture("assets/semantic/data_binding_lists.riv");
    REQUIRE(s.fixture.sm != nullptr);
    auto* mgr = s.fixture.sm->semanticManager();
    REQUIRE(mgr != nullptr);
    s.applyDiff(mgr->drainDiff());
    return s;
}

} // namespace

// ---------------------------------------------------------------------------
// Unified tree: ArtboardComponentList items register into the top-level
// SemanticManager.
// ---------------------------------------------------------------------------

TEST_CASE("ArtboardComponentList items populate the parent artboard's semantic "
          "manager",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    // All four fandom labels should be present as text SDs in the unified
    // tree, even though they live inside list-item sub-artboards.
    CHECK(s.fandomLabelsPresent() == kFandomLabels);

    // Each fandom text SD's id must resolve through the top-level manager
    // (not just show up in the diff).
    auto* mgr = s.sm()->semanticManager();
    for (const auto& label : kFandomLabels)
    {
        const auto* entry = s.findByLabel(label);
        REQUIRE(entry != nullptr);
        CAPTURE(label);
        CHECK(mgr->nodeById(entry->id) != nullptr);
    }
}

TEST_CASE("Dropdown button starts expanded with the Expandable trait authored",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    const auto* button = s.findByLabel(kDropdownLabel);
    REQUIRE(button != nullptr);
    CHECK(button->role == static_cast<uint32_t>(SemanticRole::button));
    CHECK(hasSemanticTrait(button->traitFlags, SemanticTrait::Expandable));
    CHECK(hasSemanticState(button->stateFlags, SemanticState::Expanded));
}

TEST_CASE("List-item SDs have non-empty bounds in the unified coordinate space",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    for (const auto& label : kFandomLabels)
    {
        const auto* entry = s.findByLabel(label);
        REQUIRE(entry != nullptr);
        CAPTURE(label);
        CAPTURE(entry->bounds.minX);
        CAPTURE(entry->bounds.minY);
        CAPTURE(entry->bounds.maxX);
        CAPTURE(entry->bounds.maxY);
        // List items go through rootTransform during bounds computation —
        // the result must be a non-degenerate rect.
        CHECK_FALSE(entry->bounds.isEmptyOrNaN());
        CHECK(entry->bounds.width() > 0.0f);
        CHECK(entry->bounds.height() > 0.0f);
    }
}

// ---------------------------------------------------------------------------
// Spawn / despawn cycle.
// ---------------------------------------------------------------------------

TEST_CASE("Collapsing the dropdown removes list-item SDs from the unified tree",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    CHECK(s.fandomLabelsPresent() == kFandomLabels);

    // Record ids before collapse so we can verify the manager index
    // drops them.
    std::vector<uint32_t> preCollapseIds;
    for (const auto& label : kFandomLabels)
    {
        const auto* entry = s.findByLabel(label);
        REQUIRE(entry != nullptr);
        preCollapseIds.push_back(entry->id);
    }

    s.tapDropdown();

    CHECK(s.fandomLabelsPresent().empty());

    // Dropdown button's Expanded state bit must have flipped.
    const auto* button = s.findByLabel(kDropdownLabel);
    REQUIRE(button != nullptr);
    CHECK_FALSE(hasSemanticState(button->stateFlags, SemanticState::Expanded));

    // The manager's authoritative index must agree: every pre-collapse id
    // is gone.
    auto* mgr = s.sm()->semanticManager();
    for (auto id : preCollapseIds)
    {
        CHECK(mgr->nodeById(id) == nullptr);
    }
}

TEST_CASE("Re-expanding the dropdown re-registers list-item SDs",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    s.tapDropdown(); // collapse
    CHECK(s.fandomLabelsPresent().empty());

    s.tapDropdown(); // re-expand
    CHECK(s.fandomLabelsPresent() == kFandomLabels);

    // All four labels must resolve through the top-level manager post-reexpand.
    auto* mgr = s.sm()->semanticManager();
    for (const auto& label : kFandomLabels)
    {
        const auto* entry = s.findByLabel(label);
        REQUIRE(entry != nullptr);
        CAPTURE(label);
        CHECK(mgr->nodeById(entry->id) != nullptr);
    }
}

TEST_CASE("Multiple collapse / uncollapse cycles maintain exactly 4 fandoms",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    // Initial.
    CHECK(s.fandomLabelsPresent() == kFandomLabels);

    // Three full cycles — each cycle must clean up fully and rebuild
    // without duplicating entries or leaking state.
    for (int cycle = 0; cycle < 3; ++cycle)
    {
        CAPTURE(cycle);
        s.tapDropdown(); // collapse
        CHECK(s.fandomLabelsPresent().empty());

        s.tapDropdown(); // re-expand
        CHECK(s.fandomLabelsPresent() == kFandomLabels);

        // There must be exactly four fandom-labelled text SDs — not eight
        // or twelve — across all cycles.
        int count = 0;
        for (const auto& kv : s.entries)
        {
            if (kv.second.role == static_cast<uint32_t>(SemanticRole::text) &&
                kFandomLabels.count(kv.second.label) > 0)
            {
                count++;
            }
        }
        CHECK(count == 4);
    }
}

// ---------------------------------------------------------------------------
// fireSemanticAction routed to a SD reached via the unified manager.
// ---------------------------------------------------------------------------

TEST_CASE("fireSemanticAction(tap) on the dropdown button collapses the list",
          "[semantics][artboard]")
{
    auto s = loadDataBindingLists();

    const auto* button = s.findByLabel(kDropdownLabel);
    REQUIRE(button != nullptr);
    CHECK(hasSemanticState(button->stateFlags, SemanticState::Expanded));
    CHECK(s.fandomLabelsPresent() == kFandomLabels);

    s.sm()->fireSemanticAction(button->id, SemanticActionType::tap);
    s.settle();

    const auto* afterButton = s.findByLabel(kDropdownLabel);
    REQUIRE(afterButton != nullptr);
    CHECK_FALSE(
        hasSemanticState(afterButton->stateFlags, SemanticState::Expanded));
    CHECK(s.fandomLabelsPresent().empty());
}

TEST_CASE(
    "fireSemanticAction(tap) via pointerDown/Up converge on the same state",
    "[semantics][artboard]")
{
    auto pointerSetup = loadDataBindingLists();
    auto semanticSetup = loadDataBindingLists();

    // Pointer path.
    pointerSetup.tapDropdown();

    // Semantic path.
    const auto* button = semanticSetup.findByLabel(kDropdownLabel);
    REQUIRE(button != nullptr);
    semanticSetup.sm()->fireSemanticAction(button->id, SemanticActionType::tap);
    semanticSetup.settle();

    // Both must have collapsed.
    CHECK(pointerSetup.fandomLabelsPresent().empty());
    CHECK(semanticSetup.fandomLabelsPresent().empty());

    // Both should agree on the dropdown button's Expanded bit.
    const auto* pBtn = pointerSetup.findByLabel(kDropdownLabel);
    const auto* sBtn = semanticSetup.findByLabel(kDropdownLabel);
    REQUIRE(pBtn != nullptr);
    REQUIRE(sBtn != nullptr);
    CHECK(hasSemanticState(pBtn->stateFlags, SemanticState::Expanded) ==
          hasSemanticState(sBtn->stateFlags, SemanticState::Expanded));
}

// ---------------------------------------------------------------------------
// enableSemantics idempotence under data-bound content.
// ---------------------------------------------------------------------------

TEST_CASE("enableSemantics called twice doesn't duplicate list-item SD entries",
          "[semantics][artboard]")
{
    auto file = ReadRiveFile("assets/semantic/data_binding_lists.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);

    sm->enableSemantics();
    sm->enableSemantics(); // second call should be a no-op

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    if (vmi != nullptr)
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    for (int i = 0; i < 10; ++i)
    {
        sm->advanceAndApply(0.1f);
    }

    auto* mgr = sm->semanticManager();
    REQUIRE(mgr != nullptr);
    const auto diff = mgr->drainDiff();

    // Count unique ids for each fandom label in the initial added diff.
    // Duplicate enableSemantics would show up as duplicate added entries.
    std::set<uint32_t> fandomIds;
    for (const auto& n : diff.added)
    {
        if (n.role == static_cast<uint32_t>(SemanticRole::text) &&
            kFandomLabels.count(n.label) > 0)
        {
            fandomIds.insert(n.id);
        }
    }
    CHECK(fandomIds.size() == kFandomLabels.size());
}
