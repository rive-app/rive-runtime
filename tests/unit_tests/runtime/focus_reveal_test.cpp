#include "rive/input/focus_manager.hpp"
#include "rive/input/focus_node.hpp"
#include "rive/input/focusable.hpp"
#include <catch.hpp>
#include <algorithm>
#include <vector>

// Focus traversal through a scope whose owner hasn't built all of its
// children: a virtualized list keeps a focus row per item but only the rows
// in its window hold focus nodes. Tab must reach the next item in order, not
// the next one that happens to be built, so the walk asks the owner to bring
// an unbuilt row in and waits for it (Focusable::revealFocusChild).

using namespace rive;

namespace
{
// Stands in for the list: says which rows are unbuilt and records the ones
// it was asked to bring in.
class FakeListScope : public Focusable
{
public:
  std::vector<FocusNode*> unbuilt;
  std::vector<FocusNode*> revealed;

  bool keyInput(Key, KeyModifiers, bool, bool) override { return false; }
  bool textInput(const std::string&) override { return false; }
  void focused() override {}
  void blurred() override {}
  bool isEligibleForFocusTraversal() const override { return false; }
  bool isFocusContent() const override { return false; }
  bool revealFocusChild(FocusNode* child, bool begin) override
  {
    if (std::find(unbuilt.begin(), unbuilt.end(), child) == unbuilt.end())
    {
      return false;
    }
    if (begin)
    {
      revealed.push_back(child);
    }
    return true;
  }
};

// A scope of `count` rows, of which those in [builtStart, builtEnd) hold one
// focus stop each, between a stop before the scope and one after it.
struct RevealTree
{
  FocusManager manager;
  FakeListScope list;
  rcp<FocusNode> before = make_rcp<FocusNode>();
  rcp<FocusNode> scope = FocusNode::makeStructuralScope();
  rcp<FocusNode> after = make_rcp<FocusNode>();
  std::vector<rcp<FocusNode>> rows;
  std::vector<rcp<FocusNode>> stops;

  RevealTree(size_t count, size_t builtStart, size_t builtEnd)
  {
    scope->setFocusable(&list);
    manager.addChild(nullptr, before);
    manager.addChild(nullptr, scope);
    manager.addChild(nullptr, after);
    stops.resize(count);
    for (size_t i = 0; i < count; i++)
    {
      rows.push_back(FocusNode::makeStructuralScope());
      manager.addChild(scope, rows[i]);
      if (i >= builtStart && i < builtEnd)
      {
        build(i);
      }
      else
      {
        list.unbuilt.push_back(rows[i].get());
      }
    }
  }

  ~RevealTree() { scope->clearFocusable(); }

  // The row's content arrives, with a stop unless withStop is false.
  void build(size_t i, bool withStop = true)
  {
    list.unbuilt.erase(
        std::remove(list.unbuilt.begin(), list.unbuilt.end(), rows[i].get()),
        list.unbuilt.end());
    if (withStop)
    {
      stops[i] = make_rcp<FocusNode>();
      manager.addChild(rows[i], stops[i]);
    }
  }

  // The row leaves the window.
  void unbuild(size_t i)
  {
    if (stops[i] != nullptr)
    {
      manager.removeChild(stops[i]);
      stops[i] = nullptr;
    }
    list.unbuilt.push_back(rows[i].get());
  }

  void midFrame() { manager.processAllPendingFocusRequests(); }
  void endFrame() { manager.finishAllPendingFocusRequests(); }
};
} // namespace

TEST_CASE("Tab waits for the next unbuilt row", "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  tree.manager.setFocus(tree.stops[1]);

  // Row 2 is next in order and isn't built: it is asked for, and focus
  // stays put rather than skipping to the stop after the scope.
  CHECK(tree.manager.focusNext());
  REQUIRE(tree.list.revealed.size() == 1);
  CHECK(tree.list.revealed[0] == tree.rows[2].get());
  CHECK(tree.manager.primaryFocus() == tree.stops[1]);
  CHECK(tree.manager.hasPendingFocusRequests(nullptr));

  // Frames pass before the row arrives.
  tree.midFrame();
  tree.endFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[1]);

  tree.build(2);
  tree.midFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[2]);
  CHECK_FALSE(tree.manager.hasPendingFocusRequests(nullptr));
  // Asked for once.
  CHECK(tree.list.revealed.size() == 1);
}

TEST_CASE(
    "Tab waits for a recycled row that kept structural nodes",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  // Row 2 was built once and left the window: its stops are gone, a scope
  // of its content is not.
  auto leftover = FocusNode::makeStructuralScope();
  tree.manager.addChild(tree.rows[2], leftover);
  tree.manager.setFocus(tree.stops[1]);

  CHECK(tree.manager.focusNext());
  REQUIRE(tree.list.revealed.size() == 1);
  CHECK(tree.list.revealed[0] == tree.rows[2].get());
  CHECK(tree.manager.primaryFocus() == tree.stops[1]);

  tree.build(2);
  tree.midFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[2]);
}

TEST_CASE(
    "Shift+Tab waits for the previous unbuilt row",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 3, 5);
  tree.manager.setFocus(tree.stops[3]);

  CHECK(tree.manager.focusPrevious());
  REQUIRE(tree.list.revealed.size() == 1);
  CHECK(tree.list.revealed[0] == tree.rows[2].get());

  tree.build(2);
  tree.midFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[2]);
}

TEST_CASE(
    "Tab into a scope reveals its first row, Shift+Tab its last",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 2, 4);

  tree.manager.setFocus(tree.before);
  CHECK(tree.manager.focusNext());
  REQUIRE(tree.list.revealed.size() == 1);
  CHECK(tree.list.revealed[0] == tree.rows[0].get());

  tree.manager.setFocus(tree.after);
  CHECK(tree.manager.focusPrevious());
  REQUIRE(tree.list.revealed.size() == 2);
  CHECK(tree.list.revealed[1] == tree.rows[5].get());
}

TEST_CASE(
    "The row focus was on leaving doesn't end the wait",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());

  // Scrolling row 2 in pushes row 1 out of the window.
  tree.unbuild(1);
  CHECK(tree.manager.primaryFocus() == nullptr);
  tree.endFrame();

  tree.build(2);
  tree.midFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[2]);
}

TEST_CASE("Focus sent elsewhere ends the wait", "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());

  tree.manager.setFocus(tree.before);
  tree.build(2);
  tree.midFrame();
  tree.endFrame();
  CHECK(tree.manager.primaryFocus() == tree.before);
}

TEST_CASE("A row built with no stop is walked past", "[focus][focus_reveal]")
{
  RevealTree tree(4, 0, 2);
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());

  // Row 2 arrives empty. Mid frame that isn't final, as its stops may not
  // be eligible yet; at the end of the frame the walk goes on to row 3.
  tree.build(2, false);
  tree.midFrame();
  CHECK(tree.list.revealed.size() == 1);
  tree.endFrame();
  REQUIRE(tree.list.revealed.size() == 2);
  CHECK(tree.list.revealed[1] == tree.rows[3].get());

  // Row 3 arrives empty too: on to the stop after the scope.
  tree.build(3, false);
  tree.endFrame();
  CHECK(tree.manager.primaryFocus() == tree.after);
}

TEST_CASE(
    "Tab again while waiting goes one row further",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());
  CHECK(tree.manager.focusNext());
  REQUIRE(tree.list.revealed.size() == 2);
  CHECK(tree.list.revealed[1] == tree.rows[3].get());

  tree.build(2);
  tree.build(3);
  tree.midFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[3]);
}

TEST_CASE("A row that never arrives is given up on", "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());
  for (int i = 0; i <= FocusManager::maxRevealFrames; i++)
  {
    tree.endFrame();
  }
  CHECK_FALSE(tree.manager.hasPendingFocusRequests(nullptr));

  // Arriving later doesn't pull focus to it.
  tree.build(2);
  tree.midFrame();
  tree.endFrame();
  CHECK(tree.manager.primaryFocus() == tree.stops[1]);
}

TEST_CASE(
    "Empty rows the owner doesn't claim are skipped as before",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  // Items with nothing focusable: the list answers no for them.
  tree.list.unbuilt.clear();
  tree.manager.setFocus(tree.stops[1]);
  CHECK(tree.manager.focusNext());
  CHECK(tree.list.revealed.empty());
  CHECK(tree.manager.primaryFocus() == tree.after);
}

TEST_CASE(
    "A scope's backing alone isn't focusable content",
    "[focus][focus_reveal]")
{
  FocusManager manager;
  FakeListScope list;
  auto scope = FocusNode::makeStructuralScope();
  scope->setFocusable(&list);
  manager.addChild(nullptr, scope);
  CHECK_FALSE(manager.hasFocusableContent());
  scope->clearFocusable();
}

TEST_CASE(
    "A row being waited for counts as focusable content",
    "[focus][focus_reveal]")
{
  // Nothing to focus but the scope's rows, one built.
  FocusManager manager;
  FakeListScope list;
  auto scope = FocusNode::makeStructuralScope();
  scope->setFocusable(&list);
  manager.addChild(nullptr, scope);
  auto built = FocusNode::makeStructuralScope();
  auto unbuilt = FocusNode::makeStructuralScope();
  manager.addChild(scope, built);
  manager.addChild(scope, unbuilt);
  list.unbuilt.push_back(unbuilt.get());
  auto stop = make_rcp<FocusNode>();
  manager.addChild(built, stop);
  manager.setFocus(stop);
  REQUIRE(manager.hasFocusableContent());

  // Scrolling to the awaited row takes the only built stop out of the
  // window. A host that reads no content here gives up keyboard focus
  // before the row arrives.
  CHECK(manager.focusNext());
  manager.removeChild(stop);
  CHECK(manager.primaryFocus() == nullptr);
  CHECK(manager.hasFocusableContent());

  // Once the wait is given up there is nothing left.
  for (int i = 0; i <= FocusManager::maxRevealFrames; i++)
  {
    manager.finishAllPendingFocusRequests();
  }
  CHECK_FALSE(manager.hasFocusableContent());
  scope->clearFocusable();
}

namespace
{
// A stop that sends focus elsewhere from inside its blur.
class RefocusOnBlur : public Focusable
{
public:
  FocusManager* manager = nullptr;
  rcp<FocusNode> next;

  bool keyInput(Key, KeyModifiers, bool, bool) override { return false; }
  bool textInput(const std::string&) override { return false; }
  void focused() override {}
  void blurred() override
  {
    if (next != nullptr)
    {
      manager->setFocus(next);
    }
  }
};
} // namespace

TEST_CASE(
    "Focus sent elsewhere from a leaving row's blur ends the wait",
    "[focus][focus_reveal]")
{
  RevealTree tree(6, 0, 2);
  RefocusOnBlur refocus;
  refocus.manager = &tree.manager;
  refocus.next = tree.before;
  tree.stops[1]->setFocusable(&refocus);
  tree.manager.setFocus(tree.stops[1]);
  REQUIRE(tree.manager.primaryFocus() == tree.stops[1]);
  CHECK(tree.manager.focusNext());
  REQUIRE(tree.manager.hasPendingFocusRequests(nullptr));

  // Row 1 leaves the window, and its blur moves focus to another control.
  auto leaving = tree.stops[1];
  tree.unbuild(1);
  leaving->clearFocusable();
  CHECK(tree.manager.primaryFocus() == tree.before);
  CHECK_FALSE(tree.manager.hasPendingFocusRequests(nullptr));

  // The row that was waited for doesn't take focus back when it arrives.
  tree.build(2);
  tree.midFrame();
  tree.endFrame();
  CHECK(tree.manager.primaryFocus() == tree.before);
}
