/*
 * Copyright 2025 Rive
 */

#include <rive/refcnt.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>

using namespace rive;

// Helper to create a SemanticNode with a given role and label.
static rcp<SemanticNode> makeNode(uint32_t id,
                                  SemanticRole role,
                                  const std::string& label = "")
{
    auto node = rcp<SemanticNode>(new SemanticNode(id));
    node->role(static_cast<uint32_t>(role));
    node->label(label);
    return node;
}

// Finds a node in the diff's added list by id.
static const SemanticsDiffNode* findAdded(const SemanticsDiff& diff,
                                          uint32_t id)
{
    for (const auto& n : diff.added)
    {
        if (n.id == id)
        {
            return &n;
        }
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// §2-3: Label derivation from child text nodes
// ---------------------------------------------------------------------------

TEST_CASE("button derives label from child text", "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto text = makeNode(2, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    auto diff = mgr.drainDiff();

    // Button should have derived label "Play".
    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play");

    // The text child should be absorbed (not in the diff).
    auto* txt = findAdded(diff, 2);
    CHECK(txt == nullptr);
}

TEST_CASE("button with explicit label ignores children",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button, "Play media");
    auto text = makeNode(2, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    auto diff = mgr.drainDiff();

    // Explicit label wins — should be "Play media", not "Play".
    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play media");
}

// ---------------------------------------------------------------------------
// §4: Tree order = reading order
// ---------------------------------------------------------------------------

TEST_CASE("multiple text children concatenated in tree order",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto text1 = makeNode(2, SemanticRole::text, "Play");
    auto text2 = makeNode(3, SemanticRole::text, "now");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text1);
    mgr.addChild(button, text2);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play now");
}

// ---------------------------------------------------------------------------
// §3.3: Image fallback
// ---------------------------------------------------------------------------

TEST_CASE("button derives label from image when no text",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto image = makeNode(2, SemanticRole::image, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, image);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play");
}

TEST_CASE("text takes priority over image in label derivation",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto image = makeNode(2, SemanticRole::image, "Play icon");
    auto text = makeNode(3, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, image);
    mgr.addChild(button, text);

    auto diff = mgr.drainDiff();

    // Should be "Play", not "Play icon Play" or "Play icon".
    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play");
}

// ---------------------------------------------------------------------------
// §3.4: Text normalization
// ---------------------------------------------------------------------------

TEST_CASE("derived label trims and collapses whitespace",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto text1 = makeNode(2, SemanticRole::text, "  Hello  ");
    auto text2 = makeNode(3, SemanticRole::text, "  world  ");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text1);
    mgr.addChild(button, text2);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Hello world");
}

// ---------------------------------------------------------------------------
// §5: Flattening rules
// ---------------------------------------------------------------------------

TEST_CASE("absorbed children are not in the flat output",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto icon = makeNode(2, SemanticRole::none); // decorative
    auto text = makeNode(3, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, icon);
    mgr.addChild(button, text);

    auto diff = mgr.drainDiff();

    // Button is present with derived label.
    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play");

    // Icon and text are absorbed.
    CHECK(findAdded(diff, 2) == nullptr);
    CHECK(findAdded(diff, 3) == nullptr);
}

TEST_CASE("text nested in group contributes to button label",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto group = makeNode(2, SemanticRole::group);
    auto text = makeNode(3, SemanticRole::text, "Submit");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, group);
    mgr.addChild(group, text);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Submit");

    // Group and text are absorbed.
    CHECK(findAdded(diff, 2) == nullptr);
    CHECK(findAdded(diff, 3) == nullptr);
}

// ---------------------------------------------------------------------------
// §7: Group behavior — groups provide context, not labels
// ---------------------------------------------------------------------------

TEST_CASE("standalone group does not absorb child labels",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto group = makeNode(1, SemanticRole::group, "Settings");
    auto button = makeNode(2, SemanticRole::button, "WiFi");

    mgr.addChild(nullptr, group);
    mgr.addChild(group, button);

    auto diff = mgr.drainDiff();

    // Both should be present separately.
    auto* grp = findAdded(diff, 1);
    REQUIRE(grp != nullptr);
    CHECK(grp->label == "Settings");

    auto* btn = findAdded(diff, 2);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "WiFi");
}

// ---------------------------------------------------------------------------
// §9: TextField requires explicit labeling
// ---------------------------------------------------------------------------

TEST_CASE("textField does not derive label from children",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto textField = makeNode(1, SemanticRole::textField);
    auto text = makeNode(2, SemanticRole::text, "Search");

    mgr.addChild(nullptr, textField);
    mgr.addChild(textField, text);

    auto diff = mgr.drainDiff();

    // textField should NOT derive — its label stays empty.
    auto* tf = findAdded(diff, 1);
    REQUIRE(tf != nullptr);
    CHECK(tf->label == "");

    // The text child should still be present (not absorbed).
    auto* txt = findAdded(diff, 2);
    CHECK(txt != nullptr);
}

// ---------------------------------------------------------------------------
// §12: Explicit label override
// ---------------------------------------------------------------------------

TEST_CASE("explicit label on button overrides child text",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button, "Play media");
    auto text = makeNode(2, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play media");

    // When there's an explicit label, children are NOT absorbed — they
    // remain as separate nodes since the parent didn't need them for
    // label derivation.
    auto* txt = findAdded(diff, 2);
    CHECK(txt != nullptr);
}

// ---------------------------------------------------------------------------
// All interactive roles should derive labels
// ---------------------------------------------------------------------------

TEST_CASE("all interactive roles derive labels from children",
          "[semantics][inference]")
{
    const SemanticRole interactiveRoles[] = {
        SemanticRole::button,
        SemanticRole::link,
        SemanticRole::checkbox,
        SemanticRole::switchControl,
        SemanticRole::slider,
        SemanticRole::tab,
        SemanticRole::listItem,
    };

    for (auto role : interactiveRoles)
    {
        DYNAMIC_SECTION("role " << static_cast<int>(role))
        {
            SemanticManager mgr;

            auto parent = makeNode(1, role);
            auto text = makeNode(2, SemanticRole::text, "Label");

            mgr.addChild(nullptr, parent);
            mgr.addChild(parent, text);

            auto diff = mgr.drainDiff();

            auto* node = findAdded(diff, 1);
            REQUIRE(node != nullptr);
            CHECK(node->label == "Label");
        }
    }
}

// ---------------------------------------------------------------------------
// Interactive node with no children and no label → empty
// ---------------------------------------------------------------------------

TEST_CASE("interactive node with no children has empty label",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    mgr.addChild(nullptr, button);

    auto diff = mgr.drainDiff();

    auto* btn = findAdded(diff, 1);
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "");
}

// ---------------------------------------------------------------------------
// Incremental: content change on absorbed child triggers re-derivation
// ---------------------------------------------------------------------------

TEST_CASE("content change on absorbed child re-derives parent label",
          "[semantics][inference]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto text = makeNode(2, SemanticRole::text, "Play");

    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    // First diff establishes baseline.
    auto diff1 = mgr.drainDiff();
    auto* btn1 = findAdded(diff1, 1);
    REQUIRE(btn1 != nullptr);
    CHECK(btn1->label == "Play");

    // Change the text content.
    text->label("Pause");
    mgr.markNodeDirty(2, SemanticDirt::Content);

    auto diff2 = mgr.drainDiff();

    // The parent button's label should have been re-derived.
    // Check updatedSemantic for the button (id=1).
    bool buttonUpdated = false;
    for (const auto& n : diff2.updatedSemantic)
    {
        if (n.id == 1)
        {
            CHECK(n.label == "Pause");
            buttonUpdated = true;
        }
    }
    CHECK(buttonUpdated);
}

// ---------------------------------------------------------------------------
// isInteractiveRole helper
// ---------------------------------------------------------------------------

TEST_CASE("isInteractiveRole correctly classifies roles",
          "[semantics][inference]")
{
    // Interactive roles.
    CHECK(isInteractiveRole(SemanticRole::button));
    CHECK(isInteractiveRole(SemanticRole::link));
    CHECK(isInteractiveRole(SemanticRole::checkbox));
    CHECK(isInteractiveRole(SemanticRole::switchControl));
    CHECK(isInteractiveRole(SemanticRole::slider));
    CHECK(isInteractiveRole(SemanticRole::tab));
    CHECK(isInteractiveRole(SemanticRole::listItem));

    // Non-interactive roles.
    CHECK_FALSE(isInteractiveRole(SemanticRole::none));
    CHECK_FALSE(isInteractiveRole(SemanticRole::textField));
    CHECK_FALSE(isInteractiveRole(SemanticRole::image));
    CHECK_FALSE(isInteractiveRole(SemanticRole::text));
    CHECK_FALSE(isInteractiveRole(SemanticRole::list));
    CHECK_FALSE(isInteractiveRole(SemanticRole::tabList));
    CHECK_FALSE(isInteractiveRole(SemanticRole::group));

    // uint32_t overload.
    CHECK(isInteractiveRole(static_cast<uint32_t>(SemanticRole::button)));
    CHECK_FALSE(isInteractiveRole(static_cast<uint32_t>(SemanticRole::none)));
}

// ---------------------------------------------------------------------------
// Visual position ordering
// ---------------------------------------------------------------------------

// Helper: find the SemanticsChildrenUpdate for a given parent id.
static const SemanticsChildrenUpdate* findChildrenUpdate(
    const SemanticsDiff& diff,
    int32_t parentId)
{
    for (const auto& cu : diff.childrenUpdated)
    {
        if (cu.parentId == parentId)
        {
            return &cu;
        }
    }
    return nullptr;
}

TEST_CASE("children sorted by bounds position", "[semantics][ordering]")
{
    SemanticManager mgr;

    auto parent = makeNode(1, SemanticRole::group, "container");
    parent->bounds(AABB(0, 0, 50, 250));

    // Insertion order would be 2, 3, 4 — but bounds say 4 (top),
    // 3 (mid), 2 (bottom). Visual position should win.
    auto childA = makeNode(2, SemanticRole::button, "A");
    childA->bounds(AABB(0, 200, 50, 250));
    auto childB = makeNode(3, SemanticRole::button, "B");
    childB->bounds(AABB(0, 100, 50, 150));
    auto childC = makeNode(4, SemanticRole::button, "C");
    childC->bounds(AABB(0, 0, 50, 50));

    mgr.addChild(nullptr, parent);
    mgr.addChild(parent, childA);
    mgr.addChild(parent, childB);
    mgr.addChild(parent, childC);

    auto diff = mgr.drainDiff();

    auto* cu = findChildrenUpdate(diff, 1);
    REQUIRE(cu != nullptr);
    // Sorted top-to-bottom: C (y=0), B (y=100), A (y=200).
    REQUIRE(cu->childIds.size() == 3);
    CHECK(cu->childIds[0] == 4); // C
    CHECK(cu->childIds[1] == 3); // B
    CHECK(cu->childIds[2] == 2); // A
}

TEST_CASE("X position breaks ties for same Y", "[semantics][ordering]")
{
    SemanticManager mgr;

    auto parent = makeNode(1, SemanticRole::group, "row");
    parent->bounds(AABB(0, 0, 300, 50));

    auto childA = makeNode(2, SemanticRole::button, "A");
    childA->bounds(AABB(200, 0, 300, 50)); // same Y, right
    auto childB = makeNode(3, SemanticRole::button, "B");
    childB->bounds(AABB(0, 0, 100, 50)); // same Y, left

    mgr.addChild(nullptr, parent);
    mgr.addChild(parent, childA);
    mgr.addChild(parent, childB);

    auto diff = mgr.drainDiff();

    auto* cu = findChildrenUpdate(diff, 1);
    REQUIRE(cu != nullptr);
    // Left-to-right: B (x=0) before A (x=200).
    REQUIRE(cu->childIds.size() == 2);
    CHECK(cu->childIds[0] == 3); // B
    CHECK(cu->childIds[1] == 2); // A
}

TEST_CASE("bounds change that swaps visual order triggers reorder in diff",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    auto parent = makeNode(1, SemanticRole::group, "container");
    parent->bounds(AABB(0, 0, 50, 300));

    auto childA = makeNode(2, SemanticRole::button, "A");
    childA->bounds(AABB(0, 0, 50, 50)); // top
    auto childB = makeNode(3, SemanticRole::button, "B");
    childB->bounds(AABB(0, 100, 50, 150)); // bottom

    mgr.addChild(nullptr, parent);
    mgr.addChild(parent, childA);
    mgr.addChild(parent, childB);

    // First diff: A before B.
    auto diff1 = mgr.drainDiff();
    auto* cu1 = findChildrenUpdate(diff1, 1);
    REQUIRE(cu1 != nullptr);
    REQUIRE(cu1->childIds.size() == 2);
    CHECK(cu1->childIds[0] == 2); // A
    CHECK(cu1->childIds[1] == 3); // B

    // Move A below B.
    childA->bounds(AABB(0, 200, 50, 250));
    mgr.markNodeDirty(2, SemanticDirt::Bounds);

    auto diff2 = mgr.drainDiff();

    // Should have a childrenUpdated with B before A.
    auto* cu2 = findChildrenUpdate(diff2, 1);
    REQUIRE(cu2 != nullptr);
    REQUIRE(cu2->childIds.size() == 2);
    CHECK(cu2->childIds[0] == 3); // B (y=100)
    CHECK(cu2->childIds[1] == 2); // A (y=200)
}

TEST_CASE("bounds change without order change stays on incremental path",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    auto parent = makeNode(1, SemanticRole::group, "container");
    parent->bounds(AABB(0, 0, 50, 300));

    auto childA = makeNode(2, SemanticRole::button, "A");
    childA->bounds(AABB(0, 0, 50, 50));
    auto childB = makeNode(3, SemanticRole::button, "B");
    childB->bounds(AABB(0, 100, 50, 150));

    mgr.addChild(nullptr, parent);
    mgr.addChild(parent, childA);
    mgr.addChild(parent, childB);

    auto diff1 = mgr.drainDiff();

    // Move A slightly but still before B — order unchanged.
    childA->bounds(AABB(0, 10, 50, 60));
    mgr.markNodeDirty(2, SemanticDirt::Bounds);

    auto diff2 = mgr.drainDiff();

    // No childrenUpdated since order didn't change.
    CHECK(diff2.childrenUpdated.empty());

    // Should have updatedGeometry for the moved node.
    bool hasGeometry = false;
    for (const auto& n : diff2.updatedGeometry)
    {
        if (n.id == 2)
        {
            hasGeometry = true;
        }
    }
    CHECK(hasGeometry);
}

TEST_CASE("empty bounds nodes preserve insertion order",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    // Nodes without bounds (structural / not yet laid out) keep
    // their insertion order (append order from addChild).
    auto a = makeNode(1, SemanticRole::button, "A");
    auto b = makeNode(2, SemanticRole::button, "B");
    auto c = makeNode(3, SemanticRole::button, "C");

    mgr.addChild(nullptr, a);
    mgr.addChild(nullptr, b);
    mgr.addChild(nullptr, c);

    auto diff = mgr.drainDiff();

    // Root ordering.
    auto* cu = findChildrenUpdate(diff, -1);
    REQUIRE(cu != nullptr);
    REQUIRE(cu->childIds.size() == 3);
    CHECK(cu->childIds[0] == 1);
    CHECK(cu->childIds[1] == 2);
    CHECK(cu->childIds[2] == 3);
}

TEST_CASE("boundary node children reorder when leaf bounds swap",
          "[semantics][ordering]")
{
    // Simulates a data-binding list: a parent with boundary nodes (one
    // per nested artboard), each containing a leaf semantic node. When
    // list items swap positions, the leaf bounds change. Boundary node
    // container bounds must be recomputed so the ordering check detects
    // the swap.
    SemanticManager mgr;

    auto listNode = makeNode(1, SemanticRole::list, "menu");
    listNode->bounds(AABB(0, 0, 200, 300));

    // Boundary nodes mimic nested-artboard boundaries. They have no
    // SemanticData, so their bounds are purely container-derived.
    auto boundary0 = rcp<SemanticNode>(new SemanticNode(100));
    boundary0->isBoundaryNode(true);
    auto boundary1 = rcp<SemanticNode>(new SemanticNode(101));
    boundary1->isBoundaryNode(true);

    // Leaf nodes inside each boundary (the actual semantic content).
    auto item0 = makeNode(2, SemanticRole::listItem, "Item A");
    item0->bounds(AABB(0, 0, 200, 50)); // top
    auto item1 = makeNode(3, SemanticRole::listItem, "Item B");
    item1->bounds(AABB(0, 100, 200, 150)); // bottom

    mgr.addChild(nullptr, listNode);
    mgr.addChild(listNode, boundary0);
    mgr.addChild(boundary0, item0);
    mgr.addChild(listNode, boundary1);
    mgr.addChild(boundary1, item1);

    // First diff: Item A (top) before Item B (bottom).
    auto diff1 = mgr.drainDiff();
    auto* cu1 = findChildrenUpdate(diff1, 1);
    REQUIRE(cu1 != nullptr);
    REQUIRE(cu1->childIds.size() == 2);
    CHECK(cu1->childIds[0] == 2); // Item A
    CHECK(cu1->childIds[1] == 3); // Item B

    // Simulate list swap: Item A moves to bottom, Item B to top.
    item0->bounds(AABB(0, 100, 200, 150)); // was top, now bottom
    item1->bounds(AABB(0, 0, 200, 50));    // was bottom, now top
    mgr.markNodeDirty(2, SemanticDirt::Bounds);
    mgr.markNodeDirty(3, SemanticDirt::Bounds);

    auto diff2 = mgr.drainDiff();

    // Boundary container bounds must have been recomputed from the
    // updated leaf bounds. The ordering check should detect the swap
    // and the re-flatten should produce B before A.
    auto* cu2 = findChildrenUpdate(diff2, 1);
    REQUIRE(cu2 != nullptr);
    REQUIRE(cu2->childIds.size() == 2);
    CHECK(cu2->childIds[0] == 3); // Item B (now top)
    CHECK(cu2->childIds[1] == 2); // Item A (now bottom)
}

// ---------------------------------------------------------------------------
// Diff ordering: added/moved/updatedSemantic/updatedGeometry/removed must be
// emitted in tree pre-order so consumers can apply diffs sequentially without
// worrying about hash iteration variance.
// ---------------------------------------------------------------------------

TEST_CASE("added array is emitted in tree pre-order on first frame",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    //    1 (group)
    //    ├── 2 (group)
    //    │   ├── 3 (text)
    //    │   └── 4 (text)
    //    └── 5 (group)
    //        ├── 6 (text)
    //        └── 7 (text)
    auto root = makeNode(1, SemanticRole::group);
    auto g0 = makeNode(2, SemanticRole::group);
    auto g1 = makeNode(5, SemanticRole::group);
    auto t0 = makeNode(3, SemanticRole::text, "a");
    auto t1 = makeNode(4, SemanticRole::text, "b");
    auto t2 = makeNode(6, SemanticRole::text, "c");
    auto t3 = makeNode(7, SemanticRole::text, "d");

    root->bounds(AABB(0, 0, 100, 400));
    g0->bounds(AABB(0, 0, 100, 200));
    g1->bounds(AABB(0, 200, 100, 400));
    t0->bounds(AABB(0, 0, 100, 100));
    t1->bounds(AABB(0, 100, 100, 200));
    t2->bounds(AABB(0, 200, 100, 300));
    t3->bounds(AABB(0, 300, 100, 400));

    mgr.addChild(nullptr, root);
    mgr.addChild(root, g0);
    mgr.addChild(root, g1);
    mgr.addChild(g0, t0);
    mgr.addChild(g0, t1);
    mgr.addChild(g1, t2);
    mgr.addChild(g1, t3);

    auto diff = mgr.drainDiff();

    // Groups are not interactive → no label derivation, no absorption.
    // All 7 nodes appear as additions, in tree pre-order.
    REQUIRE(diff.added.size() == 7);
    CHECK(diff.added[0].id == 1); // root
    CHECK(diff.added[1].id == 2); // g0
    CHECK(diff.added[2].id == 3); // t0
    CHECK(diff.added[3].id == 4); // t1
    CHECK(diff.added[4].id == 5); // g1
    CHECK(diff.added[5].id == 6); // t2
    CHECK(diff.added[6].id == 7); // t3

    // childrenUpdated must also be emitted in tree pre-order: roots (-1)
    // first, then parents in the order their subtrees are first entered.
    REQUIRE(diff.childrenUpdated.size() >= 3);
    CHECK(diff.childrenUpdated[0].parentId == -1);
    CHECK(diff.childrenUpdated[1].parentId == 1);
    CHECK(diff.childrenUpdated[2].parentId == 2);
    CHECK(diff.childrenUpdated[3].parentId == 5);
}

TEST_CASE("added/updatedSemantic/updatedGeometry emit in tree order on "
          "subsequent frames",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    //    1 (list)
    //    ├── 2 (listItem "A")
    //    ├── 3 (listItem "B")
    //    └── 4 (listItem "C")
    auto list = makeNode(1, SemanticRole::list);
    auto item0 = makeNode(2, SemanticRole::listItem, "A");
    auto item1 = makeNode(3, SemanticRole::listItem, "B");
    auto item2 = makeNode(4, SemanticRole::listItem, "C");

    list->bounds(AABB(0, 0, 100, 300));
    item0->bounds(AABB(0, 0, 100, 100));
    item1->bounds(AABB(0, 100, 100, 200));
    item2->bounds(AABB(0, 200, 100, 300));

    mgr.addChild(nullptr, list);
    mgr.addChild(list, item0);
    mgr.addChild(list, item1);
    mgr.addChild(list, item2);

    // Drain first-frame diff.
    (void)mgr.drainDiff();

    // Add a fourth listItem and change labels on the existing items.
    // Dirty each out-of-tree-order (5 first, then 2) to prove the diff
    // still emits in tree order.
    auto item3 = makeNode(5, SemanticRole::listItem, "D");
    item3->bounds(AABB(0, 300, 100, 400));
    mgr.addChild(list, item3);

    item2->label("C2");
    item0->label("A2");
    mgr.markNodeDirty(4, SemanticDirt::Content);
    mgr.markNodeDirty(2, SemanticDirt::Content);

    // Bounds change on item1 (out-of-order dirty).
    item1->bounds(AABB(0, 105, 100, 205));
    mgr.markNodeDirty(3, SemanticDirt::Bounds);

    auto diff = mgr.drainDiff();

    // Added contains the single new node 5.
    REQUIRE(diff.added.size() == 1);
    CHECK(diff.added[0].id == 5);

    // updatedSemantic must be in tree pre-order: 2 before 4.
    REQUIRE(diff.updatedSemantic.size() >= 2);
    size_t idx2 = SIZE_MAX, idx4 = SIZE_MAX;
    for (size_t i = 0; i < diff.updatedSemantic.size(); ++i)
    {
        if (diff.updatedSemantic[i].id == 2)
            idx2 = i;
        if (diff.updatedSemantic[i].id == 4)
            idx4 = i;
    }
    REQUIRE(idx2 != SIZE_MAX);
    REQUIRE(idx4 != SIZE_MAX);
    CHECK(idx2 < idx4);
}

// ---------------------------------------------------------------------------
// Id allocation: ids are scoped per-SemanticManager, auto-assigned when a
// node is constructed without one, and reassigned if an explicit id collides
// with a different resident node.
// ---------------------------------------------------------------------------

TEST_CASE("nodes constructed without an id receive one on addChild",
          "[semantics][ids]")
{
    SemanticManager mgr;

    auto a = rcp<SemanticNode>(new SemanticNode());
    auto b = rcp<SemanticNode>(new SemanticNode());
    a->role(static_cast<uint32_t>(SemanticRole::group));
    b->role(static_cast<uint32_t>(SemanticRole::text));
    b->label("hello");

    CHECK(a->id() == 0);
    CHECK(b->id() == 0);

    mgr.addChild(nullptr, a);
    mgr.addChild(a, b);

    CHECK(a->id() != 0);
    CHECK(b->id() != 0);
    CHECK(a->id() != b->id());
}

TEST_CASE("two independent SemanticManagers do not share an id space",
          "[semantics][ids]")
{
    SemanticManager mgrA;
    SemanticManager mgrB;

    auto a0 = rcp<SemanticNode>(new SemanticNode());
    auto a1 = rcp<SemanticNode>(new SemanticNode());
    mgrA.addChild(nullptr, a0);
    mgrA.addChild(a0, a1);

    auto b0 = rcp<SemanticNode>(new SemanticNode());
    auto b1 = rcp<SemanticNode>(new SemanticNode());
    mgrB.addChild(nullptr, b0);
    mgrB.addChild(b0, b1);

    // Both managers start counting from 1 independently — the fresh manager
    // does not inherit any global state from the first.
    CHECK(a0->id() == b0->id());
    CHECK(a1->id() == b1->id());
    // But within a manager, ids remain unique.
    CHECK(mgrA.nodeById(a0->id()) == a0.get());
    CHECK(mgrB.nodeById(b0->id()) == b0.get());
}

TEST_CASE("explicit ids are preserved and bump the manager's watermark",
          "[semantics][ids]")
{
    SemanticManager mgr;

    // Pre-registered nodes with explicit ids (e.g. from tests or
    // serialized trees). The manager must keep the caller's id.
    auto explicit10 = rcp<SemanticNode>(new SemanticNode(10));
    mgr.addChild(nullptr, explicit10);
    CHECK(explicit10->id() == 10);

    // Subsequent auto-assigned nodes must not collide with 10.
    auto auto1 = rcp<SemanticNode>(new SemanticNode());
    mgr.addChild(explicit10, auto1);
    CHECK(auto1->id() > 10);

    // A fresh explicit id below the watermark is fine and preserved.
    auto explicit5 = rcp<SemanticNode>(new SemanticNode(5));
    mgr.addChild(explicit10, explicit5);
    CHECK(explicit5->id() == 5);

    // Next auto-assign still respects the high-water mark.
    auto auto2 = rcp<SemanticNode>(new SemanticNode());
    mgr.addChild(explicit10, auto2);
    CHECK(auto2->id() > 10);
    CHECK(auto2->id() != auto1->id());
}

TEST_CASE("explicit id that collides with a different resident node is "
          "reassigned",
          "[semantics][ids]")
{
    SemanticManager mgr;

    auto first = rcp<SemanticNode>(new SemanticNode(42));
    mgr.addChild(nullptr, first);
    REQUIRE(first->id() == 42);

    // A second node also constructed with id 42 (e.g. migrated from a
    // different manager) must be reassigned, not silently dropped.
    auto second = rcp<SemanticNode>(new SemanticNode(42));
    mgr.addChild(first, second);

    CHECK(second->id() != 0);
    CHECK(second->id() != 42);
    CHECK(mgr.nodeById(42) == first.get());
    CHECK(mgr.nodeById(second->id()) == second.get());
}

// ---------------------------------------------------------------------------
// Incremental refresh() paths: content-only and bounds-only dirt should take
// the incremental patch path and emit a minimal diff (no structural arrays).
// ---------------------------------------------------------------------------

TEST_CASE("pure content dirt emits only updatedSemantic",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    auto btn = makeNode(2, SemanticRole::button, "Submit");

    root->bounds(AABB(0, 0, 100, 100));
    btn->bounds(AABB(0, 0, 100, 40));

    mgr.addChild(nullptr, root);
    mgr.addChild(root, btn);

    // Drain first-frame (full-flatten) diff.
    (void)mgr.drainDiff();

    // Change only the label — no structure, no bounds.
    btn->label("Send");
    mgr.markNodeDirty(2, SemanticDirt::Content);

    auto diff = mgr.drainDiff();

    // Incremental path ⇒ only updatedSemantic is populated.
    CHECK(diff.added.empty());
    CHECK(diff.removed.empty());
    CHECK(diff.moved.empty());
    CHECK(diff.childrenUpdated.empty());
    CHECK(diff.updatedGeometry.empty());

    REQUIRE(diff.updatedSemantic.size() == 1);
    CHECK(diff.updatedSemantic[0].id == 2);
    CHECK(diff.updatedSemantic[0].label == "Send");
}

TEST_CASE("pure bounds dirt emits only updatedGeometry",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    auto btn = makeNode(2, SemanticRole::button, "Submit");

    root->bounds(AABB(0, 0, 100, 100));
    btn->bounds(AABB(0, 0, 100, 40));

    mgr.addChild(nullptr, root);
    mgr.addChild(root, btn);
    (void)mgr.drainDiff();

    // Move the button slightly — still before anything, so no reorder.
    btn->bounds(AABB(5, 5, 105, 45));
    mgr.markNodeDirty(2, SemanticDirt::Bounds);

    auto diff = mgr.drainDiff();

    CHECK(diff.added.empty());
    CHECK(diff.removed.empty());
    CHECK(diff.moved.empty());
    CHECK(diff.childrenUpdated.empty());
    CHECK(diff.updatedSemantic.empty());

    REQUIRE(diff.updatedGeometry.size() == 1);
    CHECK(diff.updatedGeometry[0].id == 2);
    CHECK(diff.updatedGeometry[0].minX == 5);
    CHECK(diff.updatedGeometry[0].minY == 5);
    CHECK(diff.updatedGeometry[0].maxX == 105);
    CHECK(diff.updatedGeometry[0].maxY == 45);
}

TEST_CASE("mixed content+bounds dirt on one node emits into both arrays",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    auto btn = makeNode(2, SemanticRole::button, "A");

    root->bounds(AABB(0, 0, 100, 100));
    btn->bounds(AABB(0, 0, 50, 50));

    mgr.addChild(nullptr, root);
    mgr.addChild(root, btn);
    (void)mgr.drainDiff();

    btn->label("B");
    btn->bounds(AABB(0, 0, 60, 60));
    mgr.markNodeDirty(2, SemanticDirt::Content | SemanticDirt::Bounds);

    auto diff = mgr.drainDiff();

    CHECK(diff.added.empty());
    CHECK(diff.removed.empty());
    CHECK(diff.moved.empty());
    CHECK(diff.childrenUpdated.empty());

    REQUIRE(diff.updatedSemantic.size() == 1);
    CHECK(diff.updatedSemantic[0].id == 2);
    CHECK(diff.updatedSemantic[0].label == "B");

    REQUIRE(diff.updatedGeometry.size() == 1);
    CHECK(diff.updatedGeometry[0].id == 2);
    CHECK(diff.updatedGeometry[0].maxX == 60);
}

TEST_CASE(
    "no-op content dirty mark produces no updatedSemantic entry (guarded)",
    "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    auto btn = makeNode(2, SemanticRole::button, "Submit");
    root->bounds(AABB(0, 0, 100, 100));
    btn->bounds(AABB(0, 0, 100, 40));
    mgr.addChild(nullptr, root);
    mgr.addChild(root, btn);
    (void)mgr.drainDiff();

    // Mark dirty without actually changing anything. patchContentNode
    // compares against the snapshot and must detect the no-op.
    mgr.markNodeDirty(2, SemanticDirt::Content);

    auto diff = mgr.drainDiff();

    CHECK(diff.updatedSemantic.empty());
    CHECK(diff.empty());
}

TEST_CASE("no-op bounds dirty mark produces no updatedGeometry entry (guarded)",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    auto btn = makeNode(2, SemanticRole::button, "Submit");
    root->bounds(AABB(0, 0, 100, 100));
    btn->bounds(AABB(0, 0, 100, 40));
    mgr.addChild(nullptr, root);
    mgr.addChild(root, btn);
    (void)mgr.drainDiff();

    mgr.markNodeDirty(2, SemanticDirt::Bounds);

    auto diff = mgr.drainDiff();

    CHECK(diff.updatedGeometry.empty());
    CHECK(diff.empty());
}

TEST_CASE("dirty mark on an unknown id is silently ignored",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    root->bounds(AABB(0, 0, 100, 100));
    mgr.addChild(nullptr, root);
    (void)mgr.drainDiff();

    // Id 9999 doesn't exist in this manager. patchContentNode /
    // patchBoundsNode should silently skip it.
    mgr.markNodeDirty(9999, SemanticDirt::Content | SemanticDirt::Bounds);

    auto diff = mgr.drainDiff();

    CHECK(diff.updatedSemantic.empty());
    CHECK(diff.updatedGeometry.empty());
    CHECK(diff.empty());
}

TEST_CASE("drainDiff after no changes returns an empty diff",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    auto root = makeNode(1, SemanticRole::group);
    root->bounds(AABB(0, 0, 100, 100));
    mgr.addChild(nullptr, root);
    (void)mgr.drainDiff();

    auto diff = mgr.drainDiff();
    CHECK(diff.empty());
}

// ---------------------------------------------------------------------------
// Escalation from incremental to full re-flatten: when a content change on
// an absorbed (flattened) child would invalidate the parent's derived label,
// refresh() must escalate and re-derive.
// ---------------------------------------------------------------------------

TEST_CASE("content change on an absorbed child escalates to re-derivation",
          "[semantics][incremental]")
{
    SemanticManager mgr;

    // Button with a single text child — text is absorbed, button derives
    // label "Play".
    auto btn = makeNode(1, SemanticRole::button); // no explicit label
    auto txt = makeNode(2, SemanticRole::text, "Play");
    btn->bounds(AABB(0, 0, 100, 40));
    txt->bounds(AABB(0, 0, 100, 40));

    mgr.addChild(nullptr, btn);
    mgr.addChild(btn, txt);

    auto first = mgr.drainDiff();
    // Sanity: button appears with derived "Play"; text is absorbed.
    auto* b1 = findAdded(first, 1);
    REQUIRE(b1 != nullptr);
    CHECK(b1->label == "Play");
    CHECK(findAdded(first, 2) == nullptr);

    // Change the absorbed text child's label. Content dirt on an id in
    // the excluded set must trigger needsRederivation → full re-flatten.
    txt->label("Pause");
    mgr.markNodeDirty(2, SemanticDirt::Content);

    auto diff = mgr.drainDiff();

    // Button's derived label should now reflect the child change. This is
    // only possible on the full-flatten path since the absorbed child is
    // not in the flat snapshot.
    bool sawButton = false;
    for (const auto& n : diff.updatedSemantic)
    {
        if (n.id == 1)
        {
            CHECK(n.label == "Pause");
            sawButton = true;
        }
    }
    CHECK(sawButton);

    // The absorbed child still must not appear in the diff as an added or
    // updated node (it's still absorbed after re-derivation).
    CHECK(findAdded(diff, 2) == nullptr);
    for (const auto& n : diff.updatedSemantic)
    {
        CHECK(n.id != 2);
    }
}

TEST_CASE("removed array is emitted in previous tree pre-order",
          "[semantics][ordering]")
{
    SemanticManager mgr;

    //    1
    //    ├── 2
    //    ├── 3
    //    └── 4
    auto root = makeNode(1, SemanticRole::group);
    auto a = makeNode(2, SemanticRole::text, "A");
    auto b = makeNode(3, SemanticRole::text, "B");
    auto c = makeNode(4, SemanticRole::text, "C");

    root->bounds(AABB(0, 0, 100, 300));
    a->bounds(AABB(0, 0, 100, 100));
    b->bounds(AABB(0, 100, 100, 200));
    c->bounds(AABB(0, 200, 100, 300));

    mgr.addChild(nullptr, root);
    mgr.addChild(root, a);
    mgr.addChild(root, b);
    mgr.addChild(root, c);

    (void)mgr.drainDiff();

    // Remove children in a non-tree-order to verify emission order doesn't
    // depend on removal order.
    mgr.removeChild(c);
    mgr.removeChild(a);

    auto diff = mgr.drainDiff();

    REQUIRE(diff.removed.size() == 2);
    // previousFlat is pre-order 1,2,3,4 → removed should appear as 2 then 4.
    CHECK(diff.removed[0] == 2);
    CHECK(diff.removed[1] == 4);
}
