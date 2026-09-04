/*
 * Copyright 2026 Rive
 */

// zero_area_semantics.riv - authored fixture for the zero-area semantics
// contracts. One artboard with:
//   - "Invisible group": an empty Node (no geometry of its own) holding
//     "Button A" and "Button B".
//   - "Collapsing card": a shape whose rectangle PATH width animates
//     200 -> 0 -> 200 over 3s ("collapse"), holding "Inside collapsing".
//   - "Scaling group": a Node whose SCALE animates 1 -> 0 -> 1 over 3s
//     ("scalePulse"), holding "Scaled button".
//   - "Hidden group": Hidden state set, holding "Hidden button".
//   - "Empty root": a top-level empty Node holding "Root child button".
// Listeners write A / B / inside / scaled / root-child into the view
// model string lastPressed on tap (pointer and semantic).

#include "rive_file_reader.hpp"
#include "semantic_test_helpers.hpp"
#include <rive/viewmodel/viewmodel_instance_string.hpp>
#include <catch.hpp>

#include <string>
#include <unordered_map>

using namespace rive;
using namespace rive::semantic_test;

namespace
{

struct Fixture
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> sm;
    rcp<ViewModelInstance> vmi;

    // Local mirror of node content and bounds, maintained from diffs the
    // way a platform layer would.
    struct Node
    {
        std::string label;
        uint32_t role = 0;
        float minX = 0, minY = 0, maxX = 0, maxY = 0;
    };
    std::unordered_map<uint32_t, Node> nodes;

    void apply(const SemanticsDiff& diff)
    {
        for (const auto id : diff.removed)
        {
            nodes.erase(id);
        }
        for (const auto& n : diff.added)
        {
            nodes[n.id] = {n.label, n.role, n.minX, n.minY, n.maxX, n.maxY};
        }
        for (const auto& n : diff.updatedSemantic)
        {
            auto it = nodes.find(n.id);
            if (it != nodes.end())
            {
                it->second.label = n.label;
                it->second.role = n.role;
            }
        }
        for (const auto& b : diff.updatedGeometry)
        {
            auto it = nodes.find(b.id);
            if (it != nodes.end())
            {
                it->second.minX = b.minX;
                it->second.minY = b.minY;
                it->second.maxX = b.maxX;
                it->second.maxY = b.maxY;
            }
        }
        for (const auto& n : diff.moved)
        {
            auto it = nodes.find(n.id);
            if (it != nodes.end())
            {
                it->second.minX = n.minX;
                it->second.minY = n.minY;
                it->second.maxX = n.maxX;
                it->second.maxY = n.maxY;
            }
        }
    }

    void drain()
    {
        auto* manager = sm->semanticManager();
        if (manager != nullptr)
        {
            apply(manager->drainDiff());
        }
    }

    const Node* byLabel(const std::string& label) const
    {
        for (const auto& [id, node] : nodes)
        {
            if (node.label == label)
            {
                return &node;
            }
        }
        return nullptr;
    }

    uint32_t idByLabel(const std::string& label) const
    {
        for (const auto& [id, node] : nodes)
        {
            if (node.label == label)
            {
                return id;
            }
        }
        return 0;
    }

    std::string lastPressed() const
    {
        auto value = vmi->propertyValue("lastPressed");
        if (value == nullptr || !value->is<ViewModelInstanceString>())
        {
            return std::string();
        }
        return value->as<ViewModelInstanceString>()->propertyValue();
    }
};

Fixture makeFixture()
{
    Fixture f;
    f.file = ReadRiveFile("assets/semantic/zero_area_semantics.riv");
    f.artboard = f.file->artboardDefault();
    f.sm = f.artboard->stateMachineAt(0);
    REQUIRE(f.sm != nullptr);
    f.sm->enableSemantics();
    f.vmi = f.file->createDefaultViewModelInstance(f.artboard.get());
    REQUIRE(f.vmi != nullptr);
    f.artboard->bindViewModelInstance(f.vmi);
    f.sm->bindViewModelInstance(f.vmi);
    // One tiny advance so layout/bounds reconcile without moving the
    // animations meaningfully.
    f.sm->advanceAndApply(0.001f);
    f.drain();
    return f;
}

} // namespace

TEST_CASE("zero_area fixture: tree shape, labels, and hidden pruning",
          "[semantics][zeroarea]")
{
    auto f = makeFixture();

    // Every authored, non-hidden element is present and labelled.
    for (const char* label : {"Invisible group",
                              "Button A",
                              "Button B",
                              "Collapsing card",
                              "Inside collapsing",
                              "Scaling group",
                              "Scaled button",
                              "Empty root",
                              "Root child button"})
    {
        CAPTURE(label);
        CHECK(f.byLabel(label) != nullptr);
    }
    CHECK(f.nodes.size() == 9);

    // The Hidden subtree is pruned by core - it never reaches the diff.
    CHECK(f.byLabel("Hidden group") == nullptr);
    CHECK(f.byLabel("Hidden button") == nullptr);
}

TEST_CASE("zero_area fixture: empty groups get container bounds from "
          "their children",
          "[semantics][zeroarea]")
{
    auto f = makeFixture();

    // An empty Node has no geometry of its own; core computes its
    // semantic bounds as the union of its children, so platforms never
    // see a degenerate rect for a static empty group.
    const auto* group = f.byLabel("Invisible group");
    const auto* a = f.byLabel("Button A");
    const auto* b = f.byLabel("Button B");
    REQUIRE(group != nullptr);
    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    CHECK(group->minX == std::min(a->minX, b->minX));
    CHECK(group->minY == std::min(a->minY, b->minY));
    CHECK(group->maxX == std::max(a->maxX, b->maxX));
    CHECK(group->maxY == std::max(a->maxY, b->maxY));
    CHECK(group->maxX > group->minX);
    CHECK(group->maxY > group->minY);

    // Same for the top-level empty node.
    const auto* root = f.byLabel("Empty root");
    const auto* child = f.byLabel("Root child button");
    REQUIRE(root != nullptr);
    REQUIRE(child != nullptr);
    CHECK(root->minX == child->minX);
    CHECK(root->maxX == child->maxX);
}

TEST_CASE("zero_area fixture: semantic tap drives listeners into the "
          "view model",
          "[semantics][zeroarea]")
{
    auto f = makeFixture();
    CHECK(f.lastPressed() == "none");

    f.sm->fireSemanticAction(f.idByLabel("Button A"), SemanticActionType::tap);
    advance(f.sm.get(), 2, 0.001f);
    CHECK(f.lastPressed() == "A");

    f.sm->fireSemanticAction(f.idByLabel("Root child button"),
                             SemanticActionType::tap);
    advance(f.sm.get(), 2, 0.001f);
    CHECK(f.lastPressed() == "root-child");

    f.sm->fireSemanticAction(f.idByLabel("Inside collapsing"),
                             SemanticActionType::tap);
    advance(f.sm.get(), 2, 0.001f);
    CHECK(f.lastPressed() == "inside");
}

TEST_CASE("zero_area fixture: node-scale animation collapses bounds "
          "through geometry diffs",
          "[semantics][zeroarea]")
{
    auto f = makeFixture();
    const auto* scaled = f.byLabel("Scaled button");
    REQUIRE(scaled != nullptr);
    const float initialWidth = scaled->maxX - scaled->minX;
    CHECK(initialWidth > 100.0f);

    // scalePulse: 1 -> 0 -> 1 over 3s; land near the zero crossing.
    advance(f.sm.get(), 150, 0.01f);
    f.drain();

    const auto* atZero = f.byLabel("Scaled button");
    REQUIRE(atZero != nullptr);
    CHECK(atZero->maxX - atZero->minX < 2.0f);

    // And back out.
    advance(f.sm.get(), 150, 0.01f);
    f.drain();
    const auto* restored = f.byLabel("Scaled button");
    REQUIRE(restored != nullptr);
    CHECK(restored->maxX - restored->minX > initialWidth - 2.0f);
}
