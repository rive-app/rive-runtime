/*
 * Copyright 2026 Rive
 */

#include <rive/refcnt.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_role.hpp>
#include <catch.hpp>

using namespace rive;

static rcp<SemanticNode> makeNode(uint32_t id,
                                  SemanticRole role,
                                  const std::string& label = "")
{
    auto node = rcp<SemanticNode>(new SemanticNode(id));
    node->role(static_cast<uint32_t>(role));
    node->label(label);
    return node;
}

static const SemanticsDiffNode* findUpdated(const SemanticsDiff& diff,
                                            uint32_t id)
{
    for (const auto& n : diff.updatedSemantic)
    {
        if (n.id == id)
        {
            return &n;
        }
    }
    return nullptr;
}

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

// The full re-flatten path (taken whenever structure changes in the same
// frame) must compare all seven content fields, like the incremental path
// does. Dropping value/hint/headingLevel is permanent: the new snapshot is
// stored and the dirty sets cleared, so the update never re-emits.
TEST_CASE("full re-flatten emits value/hint/headingLevel changes",
          "[semantics][diff]")
{
    SemanticManager mgr;

    auto slider = makeNode(1, SemanticRole::slider, "Volume");
    slider->value("50");
    mgr.addChild(nullptr, slider);

    auto baseline = mgr.drainDiff();
    auto* added = findAdded(baseline, slider->id());
    REQUIRE(added != nullptr);
    CHECK(added->value == "50");

    // Same frame: content change on the slider AND a structural change
    // (a node appears) -> the diff is produced by the full re-flatten path.
    slider->value("75");
    slider->hint("Drag to adjust");
    slider->headingLevel(2);
    mgr.markNodeDirty(slider->id(), SemanticDirt::Content);

    auto text = makeNode(2, SemanticRole::text, "Other");
    mgr.addChild(nullptr, text);

    auto diff = mgr.drainDiff();
    CHECK(findAdded(diff, text->id()) != nullptr);

    auto* updated = findUpdated(diff, slider->id());
    REQUIRE(updated != nullptr);
    CHECK(updated->value == "75");
    CHECK(updated->hint == "Drag to adjust");
    CHECK(updated->headingLevel == 2);

    // The loss must not merely be delayed: with nothing further dirty, a
    // later drain has nothing to say about the slider.
    auto after = mgr.drainDiff();
    CHECK(findUpdated(after, slider->id()) == nullptr);
}

// Setting an explicit label at runtime (e.g. via data binding) on a node
// whose label was previously derived from an absorbed child must win over
// the stale derived label, and the child - no longer absorbed - must
// re-enter the tree.
TEST_CASE("explicit label set at runtime beats stale derived label",
          "[semantics][diff]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button);
    auto text = makeNode(2, SemanticRole::text, "Play");
    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    auto baseline = mgr.drainDiff();
    auto* btn = findAdded(baseline, button->id());
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Play"); // derived; the text child is absorbed
    CHECK(findAdded(baseline, text->id()) == nullptr);

    // Explicit label set on the previously-derived button.
    button->label("Pause");
    mgr.markNodeDirty(button->id(), SemanticDirt::Content);

    auto diff = mgr.drainDiff();

    auto* updated = findUpdated(diff, button->id());
    REQUIRE(updated != nullptr);
    CHECK(updated->label == "Pause");

    // Explicit label means no derivation and no absorption: the text child
    // becomes visible again.
    CHECK(findAdded(diff, text->id()) != nullptr);
}

// Clearing an explicit label must fall back to derivation again.
TEST_CASE("clearing an explicit label re-derives from children",
          "[semantics][diff]")
{
    SemanticManager mgr;

    auto button = makeNode(1, SemanticRole::button, "Authored");
    auto text = makeNode(2, SemanticRole::text, "Play");
    mgr.addChild(nullptr, button);
    mgr.addChild(button, text);

    auto baseline = mgr.drainDiff();
    auto* btn = findAdded(baseline, button->id());
    REQUIRE(btn != nullptr);
    CHECK(btn->label == "Authored"); // explicit; child not absorbed
    CHECK(findAdded(baseline, text->id()) != nullptr);

    button->label("");
    mgr.markNodeDirty(button->id(), SemanticDirt::Content);

    auto diff = mgr.drainDiff();

    auto* updated = findUpdated(diff, button->id());
    REQUIRE(updated != nullptr);
    CHECK(updated->label == "Play"); // derived again; child absorbed

    bool textRemoved = false;
    for (const auto id : diff.removed)
    {
        if (id == text->id())
        {
            textRemoved = true;
        }
    }
    CHECK(textRemoved);
}
