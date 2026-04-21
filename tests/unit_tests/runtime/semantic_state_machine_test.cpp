/*
 * Copyright 2026 Rive
 */

// Integration tests for StateMachineInstance's semantic path, driven by
// real .riv fixtures so authored StateMachineListeners and SemanticData
// components are exercised the same way they are in production.

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

using namespace rive;
using namespace rive::semantic_test;

namespace
{

static std::vector<const SemanticsDiffNode*> tabsFromDiff(
    const SemanticsDiff& diff)
{
    std::vector<const SemanticsDiffNode*> tabs;
    for (const auto& n : diff.added)
    {
        if (n.role == static_cast<uint32_t>(SemanticRole::tab))
        {
            tabs.push_back(&n);
        }
    }
    return tabs;
}

} // namespace

// ---------------------------------------------------------------------------
// simpsons.riv — an authored tabList with several tab items.
// ---------------------------------------------------------------------------

TEST_CASE("simpsons.riv exposes a tab list with labelled tabs",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");

    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    REQUIRE(sm != nullptr);
    sm->enableSemantics();

    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    if (vmi != nullptr)
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());

    // Exactly one tabList.
    int tabListCount = 0;
    for (const auto& n : diff.added)
    {
        if (n.role == static_cast<uint32_t>(SemanticRole::tabList))
        {
            tabListCount++;
        }
    }
    CHECK(tabListCount == 1);

    // At least two tabs, all labelled.
    const auto tabs = tabsFromDiff(diff);
    REQUIRE(tabs.size() >= 2);
    for (const auto* tab : tabs)
    {
        CHECK_FALSE(tab->label.empty());
    }

    // Exactly one tab is initially selected.
    int selected = 0;
    for (const auto* tab : tabs)
    {
        if (hasSemanticState(tab->stateFlags, SemanticState::Selected))
        {
            selected++;
        }
    }
    CHECK(selected == 1);
}

TEST_CASE(
    "simpsons.riv: pointer-tap on a tab selects it and deselects the old one",
    "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    if (vmi != nullptr)
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());
    const auto tabs = tabsFromDiff(diff);
    REQUIRE(tabs.size() >= 2);

    // Find the currently-selected tab and a different one.
    const SemanticsDiffNode* initiallySelected = nullptr;
    const SemanticsDiffNode* other = nullptr;
    for (const auto* tab : tabs)
    {
        if (hasSemanticState(tab->stateFlags, SemanticState::Selected))
        {
            initiallySelected = tab;
        }
        else if (other == nullptr)
        {
            other = tab;
        }
    }
    REQUIRE(initiallySelected != nullptr);
    REQUIRE(other != nullptr);

    // Pointer-tap the other tab at its visual center.
    const auto center = centerOf(*other);
    sm->pointerDown(center);
    sm->pointerUp(center);
    for (int i = 0; i < 10; ++i)
    {
        sm->advanceAndApply(0.1f);
    }

    auto* mgr = sm->semanticManager();
    CHECK_FALSE(isSelected(mgr->nodeById(initiallySelected->id)));
    CHECK(isSelected(mgr->nodeById(other->id)));
}

TEST_CASE("simpsons.riv: fireSemanticAction(tap) selects the target tab",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    auto vmi = file->createDefaultViewModelInstance(artboard.get());
    if (vmi != nullptr)
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());
    const auto tabs = tabsFromDiff(diff);
    REQUIRE(tabs.size() >= 2);

    const SemanticsDiffNode* initiallySelected = nullptr;
    const SemanticsDiffNode* target = nullptr;
    for (const auto* tab : tabs)
    {
        if (hasSemanticState(tab->stateFlags, SemanticState::Selected))
        {
            initiallySelected = tab;
        }
        else if (target == nullptr)
        {
            target = tab;
        }
    }
    REQUIRE(initiallySelected != nullptr);
    REQUIRE(target != nullptr);

    sm->fireSemanticAction(target->id, SemanticActionType::tap);
    for (int i = 0; i < 10; ++i)
    {
        sm->advanceAndApply(0.1f);
    }

    auto* mgr = sm->semanticManager();
    CHECK(isSelected(mgr->nodeById(target->id)));
    CHECK_FALSE(isSelected(mgr->nodeById(initiallySelected->id)));
}

TEST_CASE(
    "simpsons.riv: fireSemanticAction and pointerDown/Up converge on the same "
    "selected tab",
    "[semantics][sm]")
{
    // Two independent setups so state doesn't carry between them.
    auto filePointer = ReadRiveFile("assets/semantic/simpsons.riv");
    auto fileSemantic = ReadRiveFile("assets/semantic/simpsons.riv");

    auto pointerArtboard = filePointer->artboardDefault();
    auto semanticArtboard = fileSemantic->artboardDefault();
    auto pointerSm = pointerArtboard->stateMachineAt(0);
    auto semanticSm = semanticArtboard->stateMachineAt(0);
    pointerSm->enableSemantics();
    semanticSm->enableSemantics();

    if (auto vmi =
            filePointer->createDefaultViewModelInstance(pointerArtboard.get()))
    {
        pointerArtboard->bindViewModelInstance(vmi);
        pointerSm->bindViewModelInstance(vmi);
    }
    if (auto vmi = fileSemantic->createDefaultViewModelInstance(
            semanticArtboard.get()))
    {
        semanticArtboard->bindViewModelInstance(vmi);
        semanticSm->bindViewModelInstance(vmi);
    }

    const auto pointerDiff = settleAndDrainDiff(pointerSm.get());
    const auto semanticDiff = settleAndDrainDiff(semanticSm.get());
    const auto pointerTabs = tabsFromDiff(pointerDiff);
    const auto semanticTabs = tabsFromDiff(semanticDiff);
    REQUIRE(pointerTabs.size() >= 2);
    REQUIRE(pointerTabs.size() == semanticTabs.size());

    // Pick the second tab on each tree.
    const auto* pointerTarget = pointerTabs[1];
    const SemanticsDiffNode* semanticTarget = nullptr;
    for (const auto* t : semanticTabs)
    {
        if (t->label == pointerTarget->label)
        {
            semanticTarget = t;
            break;
        }
    }
    REQUIRE(semanticTarget != nullptr);

    // Trigger the same target via the two different paths.
    const auto center = centerOf(*pointerTarget);
    pointerSm->pointerDown(center);
    pointerSm->pointerUp(center);
    semanticSm->fireSemanticAction(semanticTarget->id, SemanticActionType::tap);

    for (int i = 0; i < 10; ++i)
    {
        pointerSm->advanceAndApply(0.1f);
        semanticSm->advanceAndApply(0.1f);
    }

    auto* pointerMgr = pointerSm->semanticManager();
    auto* semanticMgr = semanticSm->semanticManager();

    // Every tab's selected bit must match between the two trees.
    for (size_t i = 0; i < pointerTabs.size(); ++i)
    {
        const auto pSelected =
            isSelected(pointerMgr->nodeById(pointerTabs[i]->id));
        const auto sSelected =
            isSelected(semanticMgr->nodeById(semanticTabs[i]->id));
        CAPTURE(pointerTabs[i]->label);
        CHECK(pSelected == sSelected);
    }
}

TEST_CASE("simpsons.riv: fireSemanticAction with an unknown id is a no-op",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    if (auto vmi = file->createDefaultViewModelInstance(artboard.get()))
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());

    // Snapshot the selected tab before firing a bogus action.
    const auto tabs = tabsFromDiff(diff);
    REQUIRE(!tabs.empty());
    const SemanticsDiffNode* selectedBefore = nullptr;
    for (const auto* tab : tabs)
    {
        if (hasSemanticState(tab->stateFlags, SemanticState::Selected))
        {
            selectedBefore = tab;
            break;
        }
    }
    REQUIRE(selectedBefore != nullptr);

    // Fire on an id that definitely doesn't exist. Must not crash, must not
    // disturb selection state.
    sm->fireSemanticAction(0xdeadbeef, SemanticActionType::tap);
    for (int i = 0; i < 10; ++i)
    {
        sm->advanceAndApply(0.1f);
    }

    auto* mgr = sm->semanticManager();
    CHECK(isSelected(mgr->nodeById(selectedBefore->id)));
}

// Each simpsons.riv tab filters a list of authored cards below it. The list
// is a `list` role wrapping `listItem` children; selecting each tab swaps
// which cards are shown. Fixture authoring: the three tabs expose 2, 3, and
// 5 list items (parents / children / all).

TEST_CASE(
    "simpsons.riv: each tab produces a distinct listItem count below the list",
    "[semantics][sm]")
{
    auto f = loadFixture("assets/semantic/simpsons.riv");
    auto* mgr = f.sm->semanticManager();
    REQUIRE(mgr != nullptr);

    // Local aggregator: replay diffs so we can count listItems and find
    // tab bounds after each tap.
    struct Snap
    {
        uint32_t role = 0;
        AABB bounds;
    };
    std::unordered_map<uint32_t, Snap> nodes;
    auto ingest = [&](const SemanticsDiff& d) {
        for (auto id : d.removed)
        {
            nodes.erase(id);
        }
        for (const auto& n : d.added)
        {
            nodes[n.id] = {n.role, AABB(n.minX, n.minY, n.maxX, n.maxY)};
        }
        for (const auto& n : d.updatedSemantic)
        {
            auto it = nodes.find(n.id);
            if (it != nodes.end())
            {
                it->second.role = n.role;
            }
        }
        for (const auto& g : d.updatedGeometry)
        {
            auto it = nodes.find(g.id);
            if (it != nodes.end())
            {
                it->second.bounds = AABB(g.minX, g.minY, g.maxX, g.maxY);
            }
        }
    };
    auto countListItems = [&]() {
        int count = 0;
        for (const auto& kv : nodes)
        {
            if (kv.second.role == static_cast<uint32_t>(SemanticRole::listItem))
            {
                count++;
            }
        }
        return count;
    };

    ingest(mgr->drainDiff());

    // The fixture must have a list container.
    int listCount = 0;
    for (const auto& kv : nodes)
    {
        if (kv.second.role == static_cast<uint32_t>(SemanticRole::list))
        {
            listCount++;
        }
    }
    CHECK(listCount == 1);

    // Collect the three tabs' ids and centers from the initial snapshot.
    std::vector<std::pair<uint32_t, Vec2D>> tabs;
    for (const auto& kv : nodes)
    {
        if (kv.second.role == static_cast<uint32_t>(SemanticRole::tab))
        {
            const auto& b = kv.second.bounds;
            tabs.emplace_back(
                kv.first,
                Vec2D((b.minX + b.maxX) * 0.5f, (b.minY + b.maxY) * 0.5f));
        }
    }
    REQUIRE(tabs.size() == 3);

    // Tap each tab and record the listItem count under the filtered list.
    std::vector<int> counts;
    for (const auto& [tabId, center] : tabs)
    {
        f.sm->pointerDown(center);
        f.sm->pointerUp(center);
        advance(f.sm.get());
        ingest(mgr->drainDiff());
        counts.push_back(countListItems());
    }

    // Three tabs → the three authored filter counts, in some order.
    std::sort(counts.begin(), counts.end());
    CHECK(counts == std::vector<int>{2, 3, 5});
}

// ---------------------------------------------------------------------------
// tabtest.riv — a second tab-style fixture. Loops fireSemanticAction
// through every non-selected tab and verifies single-selection invariant.
// ---------------------------------------------------------------------------

TEST_CASE("tabtest.riv: firing tap on each tab keeps exactly one tab selected",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/tabtest.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    if (auto vmi = file->createDefaultViewModelInstance(artboard.get()))
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());
    const auto tabs = tabsFromDiff(diff);
    REQUIRE(tabs.size() >= 2);

    auto* mgr = sm->semanticManager();
    for (const auto* tab : tabs)
    {
        sm->fireSemanticAction(tab->id, SemanticActionType::tap);
        for (int i = 0; i < 10; ++i)
        {
            sm->advanceAndApply(0.1f);
        }

        CAPTURE(tab->label);
        CHECK(isSelected(mgr->nodeById(tab->id)));

        int selectedCount = 0;
        for (const auto* t : tabs)
        {
            if (isSelected(mgr->nodeById(t->id)))
            {
                selectedCount++;
            }
        }
        CHECK(selectedCount == 1);
    }
}

// ---------------------------------------------------------------------------
// enableSemantics / semanticManager lifecycle.
// ---------------------------------------------------------------------------

TEST_CASE("enableSemantics is idempotent; manager pointer is stable",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);

    CHECK(sm->semanticManager() == nullptr);

    sm->enableSemantics();
    auto* first = sm->semanticManager();
    REQUIRE(first != nullptr);

    sm->enableSemantics();
    auto* second = sm->semanticManager();
    CHECK(second == first);
}

TEST_CASE(
    "enableSemantics before advance: first drainDiff delivers the authored "
    "tree",
    "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/simpsons.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    if (auto vmi = file->createDefaultViewModelInstance(artboard.get()))
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    // Advance enough frames for bounds/layout to settle (some nodes need
    // Shape path builds which run during the advance cycle).
    for (int i = 0; i < 10; ++i)
    {
        sm->advanceAndApply(0.1f);
    }

    auto* mgr = sm->semanticManager();
    REQUIRE(mgr != nullptr);
    const auto diff = mgr->drainDiff();

    CHECK(diff.treeVersion > 0);
    CHECK_FALSE(diff.added.empty());
    CHECK(findAddedByRole(diff, SemanticRole::tabList) != nullptr);

    // Second drainDiff (without further advance) must be empty — the diff
    // machinery is one-shot.
    const auto second = mgr->drainDiff();
    CHECK(second.empty());
}

// ---------------------------------------------------------------------------
// data_binding_lists.riv — listener wired up on a dropdown item; fire action
// and verify the authored StateMachineListener actually executed by checking
// a bindable property on the VMI.
// ---------------------------------------------------------------------------

TEST_CASE("data_binding_lists.riv exposes at least a dropdown button",
          "[semantics][sm]")
{
    auto file = ReadRiveFile("assets/semantic/data_binding_lists.riv");
    auto artboard = file->artboardDefault();
    auto sm = artboard->stateMachineAt(0);
    sm->enableSemantics();
    if (auto vmi = file->createDefaultViewModelInstance(artboard.get()))
    {
        artboard->bindViewModelInstance(vmi);
        sm->bindViewModelInstance(vmi);
    }

    const auto diff = settleAndDrainDiff(sm.get());

    // Authored "Select a fandom" dropdown button.
    const auto* button = findAddedByLabel(diff, kDropdownLabel);
    REQUIRE(button != nullptr);
    CHECK(button->role == static_cast<uint32_t>(SemanticRole::button));
}

// ---------------------------------------------------------------------------
// semantic_list_scroll_focus_fixed.riv — a 6-item list wired into the focus
// system. Semantic requestFocus should route through FocusData's handshake
// so the target item's Focused state bit is set (and peers are cleared),
// and navigating toward the bottom of the list should scroll earlier items
// out of view.
// ---------------------------------------------------------------------------

namespace
{

struct ListFixture
{
    Fixture fixture;
    // Initial diff keyed by label so tests can look up "Element N" quickly.
    std::unordered_map<std::string, const SemanticsDiffNode*> byLabel;
    SemanticsDiff initialDiff;
};

// Loads the list fixture and builds a label→SemanticsDiffNode index from
// the initial diff. The SemanticsDiffNode pointers stay valid because the
// Fixture owns the underlying diff via initialDiff.
static ListFixture loadListFocusFixture()
{
    ListFixture lf;
    lf.fixture =
        loadFixture("assets/semantic/semantic_list_scroll_focus_fixed.riv");
    auto* mgr = lf.fixture.sm->semanticManager();
    REQUIRE(mgr != nullptr);
    lf.initialDiff = mgr->drainDiff();
    for (const auto& n : lf.initialDiff.added)
    {
        if (n.role == static_cast<uint32_t>(SemanticRole::listItem))
        {
            lf.byLabel[n.label] = &n;
        }
    }
    return lf;
}

} // namespace

// Fixture authoring: the list's viewport shows five slots labelled
// "Element 1"…"Element 5"; focus-driven scroll shifts every visible item's
// bounds upward so a focused item moves into view.
constexpr int kVisibleListSlots = 5;

TEST_CASE(
    "list_scroll_focus.riv exposes one list with five labelled list items",
    "[semantics][sm]")
{
    auto lf = loadListFocusFixture();

    int lists = 0;
    for (const auto& n : lf.initialDiff.added)
    {
        if (n.role == static_cast<uint32_t>(SemanticRole::list))
        {
            lists++;
        }
    }
    CHECK(lists == 1);

    for (int i = 1; i <= kVisibleListSlots; ++i)
    {
        const auto label = "Element " + std::to_string(i);
        CAPTURE(label);
        REQUIRE(lf.byLabel.count(label) == 1);
        CHECK(lf.byLabel[label]->role ==
              static_cast<uint32_t>(SemanticRole::listItem));
    }
    CHECK(lf.byLabel.size() == kVisibleListSlots);
}

TEST_CASE("list_scroll_focus.riv: list items expose the Focusable trait",
          "[semantics][sm]")
{
    auto lf = loadListFocusFixture();
    for (int i = 1; i <= kVisibleListSlots; ++i)
    {
        const auto label = "Element " + std::to_string(i);
        REQUIRE(lf.byLabel.count(label) == 1);
        CAPTURE(label);
        // FocusData siblings on each item auto-set the Focusable trait at
        // semantic-node construction.
        CHECK(hasSemanticTrait(lf.byLabel[label]->traitFlags,
                               SemanticTrait::Focusable));
    }
}

TEST_CASE(
    "list_scroll_focus.riv: requestFocus sets Focused only on the target item",
    "[semantics][sm]")
{
    auto lf = loadListFocusFixture();
    auto* mgr = lf.fixture.sm->semanticManager();

    const auto* target = lf.byLabel["Element 3"];
    REQUIRE(target != nullptr);
    CHECK(mgr->requestFocus(target->id));
    advance(lf.fixture.sm.get());

    for (int i = 1; i <= kVisibleListSlots; ++i)
    {
        const auto label = "Element " + std::to_string(i);
        const auto* node = mgr->nodeById(lf.byLabel[label]->id);
        REQUIRE(node != nullptr);
        CAPTURE(label);
        const bool focused =
            hasSemanticState(node->stateFlags(), SemanticState::Focused);
        if (i == 3)
        {
            CHECK(focused);
        }
        else
        {
            CHECK_FALSE(focused);
        }
    }
}

TEST_CASE(
    "list_scroll_focus.riv: moving focus hands the Focused bit between items",
    "[semantics][sm]")
{
    auto lf = loadListFocusFixture();
    auto* mgr = lf.fixture.sm->semanticManager();

    const auto* first = lf.byLabel["Element 1"];
    const auto* third = lf.byLabel["Element 3"];
    REQUIRE(first != nullptr);
    REQUIRE(third != nullptr);

    mgr->requestFocus(first->id);
    advance(lf.fixture.sm.get());
    CHECK(hasSemanticState(mgr->nodeById(first->id)->stateFlags(),
                           SemanticState::Focused));
    CHECK_FALSE(hasSemanticState(mgr->nodeById(third->id)->stateFlags(),
                                 SemanticState::Focused));

    mgr->requestFocus(third->id);
    advance(lf.fixture.sm.get());
    CHECK_FALSE(hasSemanticState(mgr->nodeById(first->id)->stateFlags(),
                                 SemanticState::Focused));
    CHECK(hasSemanticState(mgr->nodeById(third->id)->stateFlags(),
                           SemanticState::Focused));
}

TEST_CASE(
    "list_scroll_focus.riv: focusing the bottom slot scrolls the list (all "
    "items shift upward)",
    "[semantics][sm]")
{
    auto lf = loadListFocusFixture();
    auto* mgr = lf.fixture.sm->semanticManager();

    // Pin starting Y coordinates for every visible slot.
    std::unordered_map<uint32_t, float> startMinY;
    for (int i = 1; i <= kVisibleListSlots; ++i)
    {
        const auto* n = lf.byLabel["Element " + std::to_string(i)];
        REQUIRE(n != nullptr);
        startMinY[n->id] = n->minY;
    }

    // Focus the bottom-most visible slot. The authored scroll listener
    // should shift every visible item upward.
    const auto* last =
        lf.byLabel["Element " + std::to_string(kVisibleListSlots)];
    REQUIRE(last != nullptr);
    mgr->requestFocus(last->id);
    advance(lf.fixture.sm.get());
    const auto scrollDiff = mgr->drainDiff();

    // Every slot's minY must have decreased (scrolled up).
    for (const auto& [id, startY] : startMinY)
    {
        const auto* node = mgr->nodeById(id);
        REQUIRE(node != nullptr);
        CAPTURE(startY);
        CAPTURE(node->bounds().minY);
        CHECK(node->bounds().minY < startY);
    }

    // The scroll diff should carry a geometry update for each visible slot.
    CHECK(scrollDiff.updatedGeometry.size() >=
          static_cast<size_t>(kVisibleListSlots));
    CHECK(scrollDiff.treeVersion > lf.initialDiff.treeVersion);
}
