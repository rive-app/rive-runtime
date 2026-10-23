/*
 * Copyright 2026 Rive
 */

// Hiding a host hides every semantic node below it, however deep the nested
// artboards go, and showing it restores exactly what was there: same nodes,
// same parents, same order. Nodes hidden for a reason of their own stay out
// until that reason clears.
//
// nested_collapse.riv (source: assets/rml/semantic_nested_collapse/). The
// Collapse artboard keys its Screen box's display from `visible`; Screen
// hosts Outer, which hosts Middle, which hosts Inner. Outer's view model
// (`outer`) drives a box its own layout hides (`details`), a Solo
// (`soloSwitched`), a component list's own layout (`listShown`) and a slot
// holding Bare (`bare`), an artboard with no SemanticData above the Leaf it
// hosts, and two placements at the same spot (Tie A, Tie B). Inner carries
// two SemanticData on the artboard itself, two on one box, and a placement
// of Deep on the artboard itself. A semantic tap on Outer sets `tapped`.
// Middle's node is focusable.

#include <rive/animation/semantic_listener_group.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/drawable.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/semantic/semantic_data.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_node.hpp>
#include <rive/semantic/semantic_state.hpp>
#include <rive/solo.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include <rive/viewmodel/viewmodel_instance_list.hpp>
#include <rive/viewmodel/viewmodel_instance_viewmodel.hpp>
#include "semantic_test_helpers.hpp"
#include <catch.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace rive;
using namespace rive::semantic_test;

namespace
{

constexpr const char* kFixture = "assets/semantic/nested_collapse.riv";

// Everything the screen exposes while shown, with the defaults authored in
// the RML source. Outer details and Inner details are hidden by their own
// layouts and Solo inactive by its Solo.
const std::set<std::string> kShown = {
    "Hide or show",
    "Screen",
    "Outer",
    "Solo active",
    "Row one",
    "Row two",
    "Tie A",
    "Tie B",
    "Bare header",
    "Bare leaf",
    "Bare footer",
    "Middle",
    "Inner",
    "Root A",
    "Root B",
    "Deep leaf",
    "Sibling A",
    "Sibling B",
};

// The tree a platform holds, rebuilt by applying each diff in turn.
struct LiveTree
{
  std::map<uint32_t, SemanticsDiffNode> nodes;

  void apply(const SemanticsDiff& diff)
  {
    for (auto id : diff.removed)
    {
      nodes.erase(id);
    }
    for (const auto& n : diff.added)
    {
      // A node linked twice shows up twice; a set of labels would hide it.
      INFO("duplicate add: " << n.label);
      CHECK(nodes.count(n.id) == 0);
      nodes[n.id] = n;
    }
    for (const auto& list : {diff.moved, diff.updatedSemantic})
    {
      for (const auto& n : list)
      {
        nodes[n.id] = n;
      }
    }
    for (const auto& g : diff.updatedGeometry)
    {
      auto it = nodes.find(g.id);
      if (it != nodes.end())
      {
        it->second.minX = g.minX;
        it->second.minY = g.minY;
        it->second.maxX = g.maxX;
        it->second.maxY = g.maxY;
      }
    }
  }

  std::set<std::string> labels() const
  {
    std::set<std::string> result;
    for (const auto& entry : nodes)
    {
      result.insert(entry.second.label);
    }
    return result;
  }

  const SemanticsDiffNode* find(const std::string& label) const
  {
    for (const auto& entry : nodes)
    {
      if (entry.second.label == label)
      {
        return &entry.second;
      }
    }
    return nullptr;
  }

  // Each node's label mapped to its parent's label.
  std::map<std::string, std::string> parents() const
  {
    std::map<std::string, std::string> result;
    for (const auto& entry : nodes)
    {
      auto parent = nodes.find(static_cast<uint32_t>(entry.second.parentId));
      result[entry.second.label] = parent == nodes.end() ? "<root>"
                                                         : parent->second.label;
    }
    return result;
  }
};

ViewModelInstanceBoolean* boolean(ViewModelInstance* vmi, const char* name)
{
  auto* value = vmi->propertyValue(name);
  REQUIRE(value != nullptr);
  REQUIRE(value->is<ViewModelInstanceBoolean>());
  return value->as<ViewModelInstanceBoolean>();
}

struct CollapseScene
{
  Fixture f = loadFixture(kFixture);
  SemanticManager* mgr = nullptr;
  rcp<ViewModelInstance> outer;
  LiveTree tree;

  CollapseScene()
  {
    REQUIRE(f.sm != nullptr);
    REQUIRE(f.vmi != nullptr);
    mgr = f.sm->semanticManager();
    REQUIRE(mgr != nullptr);
    auto* ref = f.vmi->propertyValue("outer");
    REQUIRE(ref != nullptr);
    REQUIRE(ref->is<ViewModelInstanceViewModel>());
    outer = ref->as<ViewModelInstanceViewModel>()->referenceViewModelInstance();
    REQUIRE(outer != nullptr);
    tree.apply(mgr->drainDiff());
  }

  void setVisible(bool value)
  {
    boolean(f.vmi.get(), "visible")->propertyValue(value);
    settle();
  }

  void setOuter(const char* name, bool value)
  {
    boolean(outer.get(), name)->propertyValue(value);
    settle();
  }

  void settle() { tree.apply(settleAndDrainDiff(f.sm.get())); }

  ArtboardInstance* outerInstance() const
  {
    REQUIRE(f.artboard->nestedArtboards().size() == 1);
    auto* instance = f.artboard->nestedArtboards()[0]->artboardInstance();
    REQUIRE(instance != nullptr);
    return instance;
  }

  SemanticData* data(const std::string& label) const
  {
    const auto* entry = tree.find(label);
    REQUIRE(entry != nullptr);
    auto* node = mgr->nodeById(entry->id);
    REQUIRE(node != nullptr);
    REQUIRE(node->semanticData() != nullptr);
    return node->semanticData();
  }

  // Labels in the order a screen reader visits them: the manager's flat
  // snapshot is in tree pre-order.
  std::vector<std::string> order() const
  {
    std::vector<std::string> result;
    for (const auto& b : mgr->boundsSnapshot())
    {
      auto it = tree.nodes.find(b.id);
      REQUIRE(it != tree.nodes.end());
      result.push_back(it->second.label);
    }
    return result;
  }
};

// The SemanticData labelled `label` in this artboard or one nested below it.
SemanticData* findData(Artboard* artboard, const std::string& label)
{
  for (auto* object : artboard->objects())
  {
    if (object != nullptr && object->is<SemanticData>() &&
        object->as<SemanticData>()->label() == label)
    {
      return object->as<SemanticData>();
    }
  }
  for (auto* host : artboard->nestedArtboards())
  {
    if (auto* nested = host->artboardInstance())
    {
      if (auto* data = findData(nested, label))
      {
        return data;
      }
    }
  }
  return nullptr;
}

bool readBefore(
    const std::vector<std::string>& order,
    const std::string& first,
    const std::string& second)
{
  auto a = std::find(order.begin(), order.end(), first);
  auto b = std::find(order.begin(), order.end(), second);
  return a != order.end() && b != order.end() && a < b;
}

std::set<std::string> without(
    std::set<std::string> labels,
    std::initializer_list<const char*> drop)
{
  for (const auto* label : drop)
  {
    labels.erase(label);
  }
  return labels;
}

std::set<std::string> with(
    std::set<std::string> labels,
    std::initializer_list<const char*> add)
{
  labels.insert(add.begin(), add.end());
  return labels;
}

} // namespace

TEST_CASE(
    "The initial tree holds what is shown, in visual order",
    "[semantics][collapse]")
{
  CollapseScene s;
  CHECK(s.tree.labels() == kShown);

  // Leaf sits between Bare's header and footer on screen and is read there,
  // even though Bare has no SemanticData above the Leaf it hosts.
  auto order = s.order();
  CHECK(readBefore(order, "Bare header", "Bare leaf"));
  CHECK(readBefore(order, "Bare leaf", "Bare footer"));
}

TEST_CASE(
    "Showing a hidden layout restores the tree exactly",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto parents = s.tree.parents();
  const auto order = s.order();
  const auto registered = s.mgr->nodeCount();

  for (int cycle = 0; cycle < 2; ++cycle)
  {
    INFO("cycle " << cycle);
    s.setVisible(false);
    CHECK(s.tree.labels() == std::set<std::string>{"Hide or show"});
    // The one root the platform sees, though Screen is still a raw root.
    const auto* button = s.tree.find("Hide or show");
    REQUIRE(button != nullptr);
    CHECK(s.mgr->rootId() == button->id);

    s.setVisible(true);
    CHECK(s.tree.labels() == kShown);
    CHECK(s.tree.parents() == parents);
    CHECK(s.order() == order);
    CHECK(s.mgr->rootId() == 0);
    // Hiding registers nothing new and loses nothing.
    CHECK(s.mgr->nodeCount() == registered);
    for (const auto& entry : s.tree.nodes)
    {
      INFO(entry.second.label);
      CHECK(entry.second.maxX > entry.second.minX);
      CHECK(entry.second.maxY > entry.second.minY);
    }
  }
}

TEST_CASE(
    "Uncollapsing a host directly restores the tree exactly",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto parents = s.tree.parents();
  const auto order = s.order();
  // Collapse's only host is the Outer placement inside Screen.
  REQUIRE(s.f.artboard->nestedArtboards().size() == 1);
  auto* host = s.f.artboard->nestedArtboards()[0];

  for (int cycle = 0; cycle < 2; ++cycle)
  {
    INFO("cycle " << cycle);
    host->collapse(true);
    s.settle();
    CHECK(s.tree.labels() == std::set<std::string>{"Hide or show", "Screen"});

    host->collapse(false);
    s.settle();
    CHECK(s.tree.labels() == kShown);
    CHECK(s.tree.parents() == parents);
    CHECK(s.order() == order);
  }
}

TEST_CASE(
    "SemanticData sharing a component are siblings under their real "
    "parent",
    "[semantics][collapse]")
{
  CollapseScene s;
  for (int cycle = 0; cycle < 2; ++cycle)
  {
    INFO("cycle " << cycle);
    auto parents = s.tree.parents();
    // On Inner's artboard itself: nothing above them in Inner, so they sit
    // under the closest node above Inner's host, Middle.
    CHECK(parents["Root A"] == "Middle");
    CHECK(parents["Root B"] == "Middle");
    // On one box inside Inner's root group.
    CHECK(parents["Sibling A"] == "Inner");
    CHECK(parents["Sibling B"] == "Inner");
    // An artboard placed on Inner's artboard itself hangs under Inner's
    // boundary, so it reads under the closest node above Inner's host.
    CHECK(parents["Deep leaf"] == "Middle");

    s.setVisible(false);
    s.setVisible(true);
  }
}

TEST_CASE(
    "Nodes hidden for their own reason return when that reason clears",
    "[semantics][collapse]")
{
  CollapseScene s;

  SECTION("a box hidden by its own layout, shown after the outer show")
  {
    s.setVisible(false);
    s.setVisible(true);
    s.setOuter("details", true);
    CHECK(s.tree.labels() == with(kShown, {"Outer details"}));
    s.setOuter("details", false);
    CHECK(s.tree.labels() == kShown);
  }

  SECTION("a Hidden flag set and cleared while the outer is hidden")
  {
    auto* header = s.data("Bare header");
    s.setVisible(false);
    header->isHidden(true);
    s.settle();
    header->isHidden(false);
    s.settle();
    s.setVisible(true);
    CHECK(s.tree.labels() == kShown);
  }

  SECTION("a Hidden flag set while the outer is hidden")
  {
    auto* header = s.data("Bare header");
    s.setVisible(false);
    header->isHidden(true);
    s.setVisible(true);
    CHECK(s.tree.labels() == without(kShown, {"Bare header"}));
    header->isHidden(false);
    s.settle();
    CHECK(s.tree.labels() == kShown);
  }

  SECTION("a Solo switched after build")
  {
    s.setOuter("soloSwitched", true);
    const auto switched = with(
        without(kShown, {"Solo active"}),
        {"Solo inactive"});
    CHECK(s.tree.labels() == switched);
    s.setVisible(false);
    s.setVisible(true);
    CHECK(s.tree.labels() == switched);
    s.setOuter("soloSwitched", false);
    CHECK(s.tree.labels() == kShown);
  }

  SECTION("a Solo switched while the outer is hidden")
  {
    // A collapsed host does not advance, so switch the Solo directly. The
    // Inactive artboard now shows inside Outer, which is still hidden, so it
    // must stay out of the tree.
    auto* solo = s.outerInstance()->find<Solo>("Solo");
    REQUIRE(solo != nullptr);
    const auto registered = s.mgr->nodeCount();
    s.setVisible(false);
    solo->updateByName("Inactive");
    s.settle();
    CHECK(s.tree.labels() == std::set<std::string>{"Hide or show"});
    // On show, Outer's state machine keys the Solo back to Active.
    s.setVisible(true);
    CHECK(s.tree.labels() == kShown);
    CHECK(s.mgr->nodeCount() == registered);
  }

  SECTION("a component list hidden by its own layout")
  {
    s.setOuter("listShown", false);
    const auto noRows = without(kShown, {"Row one", "Row two"});
    CHECK(s.tree.labels() == noRows);
    s.setVisible(false);
    s.setVisible(true);
    CHECK(s.tree.labels() == noRows);
    s.setOuter("listShown", true);
    CHECK(s.tree.labels() == kShown);
  }
}

TEST_CASE(
    "Hiding a host hides artboards nested below it with no "
    "SemanticData above them",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto order = s.order();
  s.setOuter("bare", false);
  CHECK(
      s.tree.labels() ==
      without(kShown, {"Bare header", "Bare leaf", "Bare footer"}));
  s.setOuter("bare", true);
  CHECK(s.tree.labels() == kShown);
  CHECK(s.order() == order);
}

TEST_CASE(
    "Semantic actions do not reach nodes under a hidden host",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto* node = s.tree.find("Outer");
  REQUIRE(node != nullptr);
  const auto outerId = node->id;
  auto* tapped = boolean(s.outer.get(), "tapped");

  s.setVisible(false);
  s.f.sm->fireSemanticAction(outerId, SemanticActionType::tap);
  s.settle();
  s.setVisible(true);
  CHECK_FALSE(tapped->propertyValue());

  s.f.sm->fireSemanticAction(outerId, SemanticActionType::tap);
  s.settle();
  CHECK(tapped->propertyValue());
}

TEST_CASE(
    "A list row removed while its host is hidden stays unreachable, "
    "then is released",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto* row = s.tree.find("Row one");
  REQUIRE(row != nullptr);
  const auto rowId = row->id;
  auto* rows = s.outer->propertyValue("rows");
  REQUIRE(rows != nullptr);
  REQUIRE(rows->is<ViewModelInstanceList>());

  // A collapsed host does not advance, so the row's artboard outlives the
  // removal until the show; until then it must not be reachable.
  s.setVisible(false);
  rows->as<ViewModelInstanceList>()->removeItem(0);
  s.settle();
  CHECK_FALSE(s.mgr->isInTree(s.mgr->nodeById(rowId)));

  s.setVisible(true);
  CHECK(s.mgr->nodeById(rowId) == nullptr);
  CHECK(s.tree.labels() == without(kShown, {"Row one"}));
}

TEST_CASE(
    "Cleaning up a hidden artboard releases every node below it",
    "[semantics][collapse]")
{
  CollapseScene s;
  std::vector<uint32_t> below;
  for (const auto& entry : s.tree.nodes)
  {
    const auto& label = entry.second.label;
    if (label != "Hide or show" && label != "Screen")
    {
      below.push_back(entry.first);
    }
  }
  REQUIRE(below.size() == kShown.size() - 2);
  auto* outerInstance = s.outerInstance();

  // What removing or swapping the hosted artboard does, while its host is
  // hidden.
  s.setVisible(false);
  outerInstance->cleanupSemanticTree();
  for (auto id : below)
  {
    CHECK(s.mgr->nodeById(id) == nullptr);
  }
  // Boundary nodes and nodes hidden for their own reason never reach the
  // platform tree but are registered too; only Collapse's own two nodes may
  // stay.
  CHECK(s.mgr->nodeCount() == 2);
}

TEST_CASE(
    "A Hidden node stays out when its own layout hides and shows",
    "[semantics][collapse]")
{
  CollapseScene s;
  s.setOuter("details", true);
  REQUIRE(s.tree.find("Outer details") != nullptr);
  s.data("Outer details")->isHidden(true);
  s.settle();
  CHECK(s.tree.labels() == kShown);

  s.setOuter("details", false);
  s.setOuter("details", true);
  CHECK(s.tree.labels() == kShown);
}

TEST_CASE(
    "An anchor hidden for its own reason is released on cleanup",
    "[semantics][collapse]")
{
  CollapseScene s;
  auto* outerData = s.data("Outer");
  const auto* outerNode = s.tree.find("Outer");
  REQUIRE(outerNode != nullptr);
  const auto outerId = outerNode->id;
  auto* outer = s.outerInstance();
  // Outer's node is the anchor Middle's boundary hangs under.
  outerData->isHidden(true);
  s.settle();
  auto* middle = outer->nestedArtboard("Middle");
  middle->collapse(true);
  s.settle();
  middle->collapse(false);
  s.settle();

  outer->cleanupSemanticTree();
  CHECK(s.mgr->nodeById(outerId) == nullptr);
}

TEST_CASE(
    "Hiding one of two placements at the same spot keeps their order",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto order = s.order();
  REQUIRE(readBefore(order, "Tie A", "Tie B"));
  auto* tieA = s.outerInstance()->nestedArtboard("Tie A host");
  for (int cycle = 0; cycle < 2; ++cycle)
  {
    INFO("cycle " << cycle);
    tieA->collapse(true);
    s.settle();
    CHECK(s.tree.find("Tie A") == nullptr);
    tieA->collapse(false);
    s.settle();
    CHECK(s.order() == order);
  }
}

TEST_CASE(
    "requestFocus refuses a node under a hidden host",
    "[semantics][collapse]")
{
  CollapseScene s;
  const auto* middle = s.tree.find("Middle");
  REQUIRE(middle != nullptr);
  const auto middleId = middle->id;
  CHECK(s.mgr->requestFocus(middleId));
  s.setVisible(false);
  CHECK_FALSE(s.mgr->requestFocus(middleId));
}

namespace
{
// A virtualized carousel of 20 rows, 5 realized at a time. Each row is a
// hosted artboard, so each has a semantic boundary node.
struct PooledList
{
  Fixture f = loadFixture("assets/component_list_virtualized.riv");
  ArtboardComponentList* list = nullptr;
  SemanticManager* mgr = nullptr;

  PooledList()
  {
    REQUIRE(f.sm != nullptr);
    mgr = f.sm->semanticManager();
    REQUIRE(mgr != nullptr);
    list = f.artboard->find<ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    REQUIRE(realizedInTree() == 5);
  }

  // A row's boundary is never in the flattened tree itself; the row shows
  // when its boundary is registered here and its host is not collapsed.
  bool rowInTree(int index) const
  {
    auto* row = list->artboardInstance(index);
    if (row == nullptr || row->semanticBoundaryNode() == nullptr)
    {
      return false;
    }
    auto* boundary = row->semanticBoundaryNode().get();
    return mgr->nodeById(boundary->id()) == boundary && !row->isHostCollapsed();
  }

  int realizedInTree() const
  {
    int count = 0;
    for (int i = 0; i < list->artboardCount(); i++)
    {
      if (rowInTree(i))
      {
        count++;
      }
    }
    return count;
  }
};
} // namespace

TEST_CASE(
    "Destroying a pooled list row leaves the tree safe to flatten",
    "[semantics][collapse]")
{
  PooledList p;
  // A pooled row keeps its boundary node registered. Destroying the pool
  // frees the row's artboard, and flattening must not read it.
  p.list->removeVirtualizable(4);
  p.list->clear();
  p.mgr->markDirty(SemanticDirt::Structure);
  auto diff = p.mgr->drainDiff();
  CHECK(diff.added.empty());
}

TEST_CASE(
    "A row pooled while its list is hidden comes back when reused",
    "[semantics][collapse]")
{
  PooledList p;
  p.list->collapse(true);
  p.list->addVirtualizable(10);
  CHECK_FALSE(p.rowInTree(10));
  p.list->removeVirtualizable(10);

  p.list->collapse(false);
  p.list->addVirtualizable(11);
  CHECK(p.rowInTree(11));
}

TEST_CASE(
    "A node hidden by its own layout survives a new state machine",
    "[semantics][collapse]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(vmi != nullptr);
  artboard->bindViewModelInstance(vmi);

  // Outer details is hidden by its own layout while the first state
  // machine, and its semantic manager, are destroyed.
  {
    auto first = artboard->stateMachineAt(0);
    first->enableSemantics();
    first->bindViewModelInstance(vmi);
    advance(first.get());
  }

  auto sm = artboard->stateMachineAt(0);
  sm->enableSemantics();
  sm->bindViewModelInstance(vmi);
  LiveTree tree;
  tree.apply(settleAndDrainDiff(sm.get()));
  CHECK(tree.labels() == kShown);

  // The hidden nodes belong to the new manager, not the destroyed one.
  auto* mgr = sm->semanticManager();
  for (const char* label : {"Outer details", "Inner details"})
  {
    INFO(label);
    auto* data = findData(artboard.get(), label);
    REQUIRE(data != nullptr);
    REQUIRE(data->hasSemanticNode());
    auto node = data->semanticNode();
    CHECK(node->manager() == mgr);
    CHECK(mgr->nodeById(node->id()) == node.get());
    CHECK_FALSE(mgr->isInTree(node.get()));
  }

  auto* outerRef = vmi->propertyValue("outer");
  REQUIRE(outerRef != nullptr);
  REQUIRE(outerRef->is<ViewModelInstanceViewModel>());
  auto outer = outerRef->as<ViewModelInstanceViewModel>()
                   ->referenceViewModelInstance();
  boolean(outer.get(), "details")->propertyValue(true);
  tree.apply(settleAndDrainDiff(sm.get()));
  CHECK(tree.labels() == with(kShown, {"Outer details"}));
}

TEST_CASE(
    "A layout shown while no state machine runs is shown in the next one",
    "[semantics][collapse]")
{
  auto file = ReadRiveFile(kFixture);
  auto artboard = file->artboardDefault();
  REQUIRE(artboard != nullptr);
  auto vmi = file->createDefaultViewModelInstance(artboard.get());
  REQUIRE(vmi != nullptr);
  artboard->bindViewModelInstance(vmi);

  // Outer details' own layout hides it while the first state machine
  // builds its node.
  {
    auto first = artboard->stateMachineAt(0);
    first->enableSemantics();
    first->bindViewModelInstance(vmi);
    advance(first.get());
  }

  // The layout is shown while the node belongs to no manager.
  auto* outerRef = vmi->propertyValue("outer");
  REQUIRE(outerRef != nullptr);
  REQUIRE(outerRef->is<ViewModelInstanceViewModel>());
  auto outer = outerRef->as<ViewModelInstanceViewModel>()
                   ->referenceViewModelInstance();
  boolean(outer.get(), "details")->propertyValue(true);
  artboard->advance(0.0f);

  auto sm = artboard->stateMachineAt(0);
  sm->enableSemantics();
  sm->bindViewModelInstance(vmi);
  LiveTree tree;
  tree.apply(settleAndDrainDiff(sm.get()));
  CHECK(tree.labels() == with(kShown, {"Outer details"}));
}

TEST_CASE(
    "Nodes keep tracking their position while an ancestor is hidden",
    "[semantics][collapse]")
{
  // The reference: Bare's slot hidden, so everything below it moves up.
  CollapseScene reference;
  reference.setOuter("bare", false);

  CollapseScene s;
  auto* outer = s.data("Outer");
  outer->isHidden(true);
  s.settle();
  s.setOuter("bare", false);
  outer->isHidden(false);
  s.settle();

  for (const char* label : {"Middle", "Inner", "Root A", "Deep leaf"})
  {
    INFO(label);
    const auto* expected = reference.tree.find(label);
    const auto* actual = s.tree.find(label);
    REQUIRE(expected != nullptr);
    REQUIRE(actual != nullptr);
    CHECK(actual->minY == Approx(expected->minY));
    CHECK(actual->maxY == Approx(expected->maxY));
  }
}

TEST_CASE(
    "A drawable flagged Hidden at runtime keeps its node",
    "[semantics][collapse]")
{
  // ScriptedTransition flags a page Hidden while it composites the page
  // itself, so the page stays on screen and must stay in the tree.
  CollapseScene s;
  auto* box = s.data("Deep leaf")->parent();
  REQUIRE(box->is<Drawable>());
  auto* drawable = box->as<Drawable>();
  const auto flags = drawable->drawableFlags();
  drawable->drawableFlags(flags | static_cast<uint32_t>(DrawableFlag::Hidden));
  REQUIRE(drawable->isHidden());

  s.mgr->markDirty(SemanticDirt::Structure);
  s.settle();
  CHECK(s.tree.labels() == kShown);

  drawable->drawableFlags(flags);
  s.mgr->markDirty(SemanticDirt::Structure);
  s.settle();
  CHECK(s.tree.labels() == kShown);
}
