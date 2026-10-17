/*
* Copyright 2026 Rive
*/

// Semantics survive hide/show round trips of nested artboards, and sibling
// SemanticData never parent each other.
//
// nested_collapse.riv is simpsons.riv plus two button SemanticData on the
// root of the innermost artboard, so the tree is
// Main -> characterCard (list item) -> Characters (Deep A, Deep B).
// Collapsing a characterCard host walks the semantic subtree and removes all
// three nodes; uncollapsing has to reach back into Characters to re-add the
// two buttons. The two root-level SemanticData also cover the sibling case:
// resolving them to each other would detach both from the tree.

#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/semantic/semantic_manager.hpp>
#include <rive/semantic/semantic_snapshot.hpp>
#include "rive_file_reader.hpp"
#include "semantic_test_helpers.hpp"
#include <catch.hpp>

#include <set>
#include <string>

using namespace rive;
using namespace rive::semantic_test;

namespace
{

static std::set<std::string> addedLabels(const SemanticsDiff& diff)
{
  std::set<std::string> labels;
  for (const auto& n : diff.added)
  {
    labels.insert(n.label);
  }
  return labels;
}

static NestedArtboard* firstActiveHost(Artboard* artboard)
{
  for (auto* host : artboard->nestedArtboards())
  {
    if (!host->isCollapsed() && host->artboardInstance(0) != nullptr)
    {
      return host;
    }
  }
  return nullptr;
}

} // namespace

TEST_CASE("Sibling SemanticData on one component both reach the tree",
     "[semantics][collapse]")
{
  auto f = loadFixture("assets/semantic/nested_collapse.riv");
  REQUIRE(f.sm != nullptr);
  auto* mgr = f.sm->semanticManager();
  REQUIRE(mgr != nullptr);

  const auto diff = mgr->drainDiff();
  const auto labels = addedLabels(diff);
  // Five characterCard instances, each hosting one Characters artboard.
  CHECK(labels.count("Deep A") == 1);
  CHECK(labels.count("Deep B") == 1);

  // Both sit under the same list item, not under each other.
  int deepParentMismatch = 0;
  for (const auto& n : diff.added)
  {
    if (n.label != "Deep A" && n.label != "Deep B")
    {
      continue;
    }
    bool parentIsDeep = false;
    for (const auto& p : diff.added)
    {
      if (static_cast<int32_t>(p.id) == n.parentId &&
        (p.label == "Deep A" || p.label == "Deep B"))
      {
        parentIsDeep = true;
      }
    }
    if (parentIsDeep)
    {
      deepParentMismatch++;
    }
  }
  CHECK(deepParentMismatch == 0);
}

TEST_CASE("Uncollapsing a host re-adds semantics from artboards nested two "
     "levels deep",
     "[semantics][collapse]")
{
  auto f = loadFixture("assets/semantic/nested_collapse.riv");
  REQUIRE(f.sm != nullptr);
  auto* mgr = f.sm->semanticManager();
  REQUIRE(mgr != nullptr);
  mgr->drainDiff();

  auto* host = firstActiveHost(f.artboard.get());
  REQUIRE(host != nullptr);

  for (int cycle = 0; cycle < 2; ++cycle)
  {
    host->collapse(true);
    auto removed = settleAndDrainDiff(f.sm.get(), 3);
    // The card's own list-item node plus Deep A and Deep B.
    CHECK(removed.removed.size() == 3);

    host->collapse(false);
    auto added = settleAndDrainDiff(f.sm.get(), 3);
    const auto labels = addedLabels(added);
    CHECK(added.added.size() == 3);
    CHECK(labels.count("Deep A") == 1);
    CHECK(labels.count("Deep B") == 1);
    for (const auto& n : added.added)
    {
      if (n.label == "Deep A" || n.label == "Deep B")
      {
        CHECK(n.maxX > n.minX);
        CHECK(n.maxY > n.minY);
      }
    }
  }
}
