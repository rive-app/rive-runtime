#include "yoga/Yoga.h"
#include <algorithm>
#include <catch.hpp>
#include <initializer_list>
#include <string>
#include <thread>
#include <vector>

// A grid sizes an intrinsic track from its items' min-content, which it gets
// by laying each item out in zero space (TrackSizing::measureItemMinContent).
// That probe is cached in its own slot so a stack's subtree is reused across
// frames; these hold what the probe must still compute, and what the cache
// must not answer stale.

namespace
{
struct Tree
{
  YGConfigRef config = YGConfigNew();
  YGNodeRef root = nullptr;

  Tree() { YGConfigSetPointScaleFactor(config, 0); }
  ~Tree()
  {
    if (root != nullptr)
    {
      YGNodeFreeRecursive(root);
    }
    YGConfigFree(config);
  }

  YGNodeRef node() { return YGNodeNewWithConfig(config); }

  // What the runtime compiles a stack to: one 1fr column, one 1fr row.
  YGNodeRef stack()
  {
    YGNodeRef stack = node();
    YGNodeStyleSetDisplay(stack, YGDisplayGrid);
    YGNodeStyleSetGridTemplateColumnsCount(stack, 1);
    YGNodeStyleSetGridTemplateColumn(stack, 0, YGGridTrackTypeFr, 1);
    YGNodeStyleSetGridTemplateRowsCount(stack, 1);
    YGNodeStyleSetGridTemplateRow(stack, 0, YGGridTrackTypeFr, 1);
    return stack;
  }

  void layout(float width = YGUndefined, float height = YGUndefined)
  {
    YGNodeCalculateLayout(root, width, height, YGDirectionLTR);
  }
};

// Every node's box under [node], depth first, to compare two layouts whole.
std::string boxes(YGNodeRef node)
{
  std::string out = std::to_string(YGNodeLayoutGetLeft(node)) + "," +
                    std::to_string(YGNodeLayoutGetTop(node)) + " " +
                    std::to_string(YGNodeLayoutGetWidth(node)) + "x" +
                    std::to_string(YGNodeLayoutGetHeight(node)) + "\n";
  for (uint32_t i = 0; i < YGNodeGetChildCount(node); i++)
  {
    out += boxes(YGNodeGetChild(node, i));
  }
  return out;
}

int s_probeMeasures = 0;

// A 60 wide leaf that counts the measures a probe makes of it.
YGSize measureCountingProbes(
    YGNodeRef node,
    float width,
    YGMeasureMode widthMode,
    float,
    YGMeasureMode)
{
  if (YGConfigIsMeasuringMinContent(YGNodeGetConfig(node)))
  {
    s_probeMeasures++;
  }
  return {
      widthMode == YGMeasureModeUndefined ? 60.0f : std::min(width, 60.0f),
      20.0f};
}

int s_probesLayingOut = 0;
std::vector<std::string> s_keptLayouts;
bool s_otherThreadSawProbe = false;

// Lays out and frees a tree on [config] whose leaf counts the probes it sees.
void layOutTemporaryTree(YGConfigRef config)
{
  YGNodeRef other = YGNodeNewWithConfig(config);
  YGNodeRef leaf = YGNodeNewWithConfig(config);
  YGNodeSetMeasureFunc(leaf, measureCountingProbes);
  YGNodeInsertChild(other, leaf, 0);
  YGNodeCalculateLayout(other, 200, 200, YGDirectionLTR);
  YGNodeFreeRecursive(other);
}

// A 60 wide leaf whose measure lays out another tree on its config, as a
// scripted layout advancing an artboard does: the tree in its context, whose
// boxes it records each time, or a temporary one.
YGSize measureLayingOutAnotherTree(
    YGNodeRef node,
    float,
    YGMeasureMode,
    float,
    YGMeasureMode)
{
  if (YGConfigIsMeasuringMinContent(YGNodeGetConfig(node)))
  {
    s_probesLayingOut++;
  }
  YGNodeRef kept = static_cast<YGNodeRef>(YGNodeGetContext(node));
  if (kept == nullptr)
  {
    layOutTemporaryTree(YGNodeGetConfig(node));
  }
  else
  {
    YGNodeCalculateLayout(kept, 200, 200, YGDirectionLTR);
    s_keptLayouts.push_back(boxes(kept));
  }
  return {60, 20};
}

// A 60 wide leaf whose measure waits on another thread laying out a tree on
// its config, as a second worker does on the config every Rive node shares.
YGSize measureOnAnotherThread(
    YGNodeRef node,
    float,
    YGMeasureMode,
    float,
    YGMeasureMode)
{
  YGConfigRef config = YGNodeGetConfig(node);
  if (YGConfigIsMeasuringMinContent(config))
  {
    s_probesLayingOut++;
  }
  std::thread([config]() {
    s_otherThreadSawProbe = s_otherThreadSawProbe ||
                            YGConfigIsMeasuringMinContent(config);
    layOutTemporaryTree(config);
  }).join();
  return {60, 20};
}
} // namespace

// The probe lays the item out, so a non-stretch descendant is laid out too and
// reports the content it overflows with. A measure pass would hand it yoga's
// zero-space shortcut and the track would collapse.
TEST_CASE(
    "Grid min-content sees a non-stretch descendant's overflow",
    "[layout][grid]")
{
  SECTION("width")
  {
    Tree t;
    t.root = t.stack();
    YGNodeStyleSetWidth(t.root, 100);
    YGNodeRef item = t.node();
    YGNodeStyleSetHeight(item, 40);
    YGNodeRef row = t.node();
    YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
    YGNodeStyleSetAlignSelf(row, YGAlignFlexStart);
    YGNodeStyleSetHeight(row, 10);
    YGNodeRef leaf = t.node();
    YGNodeStyleSetWidth(leaf, 600);
    YGNodeStyleSetHeight(leaf, 10);
    YGNodeStyleSetFlexShrink(leaf, 0);
    YGNodeInsertChild(row, leaf, 0);
    YGNodeInsertChild(item, row, 0);
    YGNodeInsertChild(t.root, item, 0);
    t.layout();
    CHECK(YGNodeLayoutGetWidth(item) == 600);
  }
  SECTION("height")
  {
    Tree t;
    t.root = t.stack();
    YGNodeStyleSetHeight(t.root, 100);
    YGNodeRef item = t.node();
    YGNodeStyleSetFlexDirection(item, YGFlexDirectionRow);
    YGNodeStyleSetWidth(item, 40);
    YGNodeRef column = t.node();
    YGNodeStyleSetAlignSelf(column, YGAlignFlexStart);
    YGNodeStyleSetWidth(column, 10);
    YGNodeRef leaf = t.node();
    YGNodeStyleSetWidth(leaf, 10);
    YGNodeStyleSetHeight(leaf, 600);
    YGNodeStyleSetFlexShrink(leaf, 0);
    YGNodeInsertChild(column, leaf, 0);
    YGNodeInsertChild(item, column, 0);
    YGNodeInsertChild(t.root, item, 0);
    t.layout();
    CHECK(YGNodeLayoutGetHeight(item) == 600);
  }
}

// A layout resolves flex lengths, so a child's max-width holds. A measure pass
// with an exact cross axis skips that and sums raw flex bases.
TEST_CASE("Grid min-content clamps a child by its max size", "[layout][grid]")
{
  Tree t;
  t.root = t.stack();
  YGNodeStyleSetWidth(t.root, 50);
  YGNodeRef item = t.node();
  YGNodeStyleSetFlexDirection(item, YGFlexDirectionRow);
  YGNodeStyleSetHeight(item, 100);
  YGNodeRef leaf = t.node();
  YGNodeStyleSetWidth(leaf, 200);
  YGNodeStyleSetMaxWidth(leaf, 100);
  YGNodeStyleSetHeight(leaf, 10);
  YGNodeStyleSetFlexShrink(leaf, 0);
  YGNodeInsertChild(item, leaf, 0);
  YGNodeInsertChild(t.root, item, 0);
  t.layout();
  CHECK(YGNodeLayoutGetWidth(item) == 100);
  CHECK(YGNodeLayoutGetWidth(leaf) == 100);
}

// The item's percent padding resolves against its column. Widening the
// column leaves the item clean and its probe's available sizes unchanged, so
// only the owner size says the answer moved.
TEST_CASE("Grid min-content follows a resized grid area", "[layout][grid]")
{
  Tree t;
  t.root = t.node();
  YGNodeStyleSetDisplay(t.root, YGDisplayGrid);
  YGNodeStyleSetGridTemplateColumnsCount(t.root, 1);
  YGNodeStyleSetGridTemplateColumn(t.root, 0, YGGridTrackTypePoints, 100);
  YGNodeStyleSetGridTemplateRowsCount(t.root, 1);
  YGNodeStyleSetGridTemplateRow(t.root, 0, YGGridTrackTypeFr, 1);
  YGNodeRef item = t.node();
  YGNodeStyleSetWidth(item, 100);
  YGNodeStyleSetPaddingPercent(item, YGEdgeTop, 10);
  YGNodeInsertChild(t.root, item, 0);
  t.layout();
  CHECK(YGNodeLayoutGetHeight(item) == 10);
  YGNodeStyleSetGridTemplateColumn(t.root, 0, YGGridTrackTypePoints, 200);
  t.layout();
  CHECK(YGNodeLayoutGetHeight(item) == 20);
  CHECK(YGNodeLayoutGetHeight(t.root) == 20);
}

// Not grid-specific: a percent min/max size clamps against the owner, which
// the cache key leaves out. Resizing the root dirties nothing and gives the
// child the same available sizes, yet its height has to follow.
TEST_CASE("A percent min size follows its resized owner", "[layout]")
{
  Tree t;
  t.root = t.node();
  YGNodeStyleSetFlexDirection(t.root, YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(t.root, YGAlignFlexStart);
  YGNodeRef child = t.node();
  YGNodeStyleSetWidth(child, 20);
  YGNodeStyleSetHeight(child, 50);
  YGNodeStyleSetMinHeightPercent(child, 20);
  YGNodeInsertChild(t.root, child, 0);
  for (float height : {300.0f, 150.0f, 400.0f, 150.0f})
  {
    t.layout(200, height);
    CHECK(YGNodeLayoutGetHeight(child) == std::max(50.0f, height / 5));
  }
}

// A node that reads its owner's size is measured afresh each time, against
// the owner of that measure. The row above it is then laid out from cache, so
// nothing lays the node out again to put its padding back.
TEST_CASE(
    "A percent padding keeps its laid out size through a measure",
    "[layout]")
{
  Tree t;
  t.root = t.node();
  YGNodeStyleSetFlexDirection(t.root, YGFlexDirectionRow);
  YGNodeRef row = t.node();
  YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
  YGNodeRef child = t.node();
  YGNodeStyleSetWidth(child, 50);
  YGNodeStyleSetHeight(child, 20);
  YGNodeStyleSetPaddingPercent(child, YGEdgeLeft, 10);
  YGNodeInsertChild(row, child, 0);
  YGNodeInsertChild(t.root, row, 0);
  for (float width : {300.0f, 400.0f})
  {
    t.layout(width, YGUndefined);
    CHECK(YGNodeLayoutGetPadding(child, YGEdgeLeft) == Approx(5));
  }
}

TEST_CASE(
    "A measure with a percent padding reports its overflow to its owner",
    "[layout]")
{
  // The row is measured before it's stretched, and only that measure passes
  // its overflow up.
  Tree t;
  t.root = t.node();
  YGNodeStyleSetFlexDirection(t.root, YGFlexDirectionRow);
  YGNodeRef row = t.node();
  YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
  YGNodeStyleSetWidth(row, 50);
  YGNodeStyleSetPaddingPercent(row, YGEdgeLeft, 10);
  YGNodeRef wide = t.node();
  YGNodeStyleSetWidth(wide, 100);
  YGNodeStyleSetHeight(wide, 10);
  YGNodeStyleSetFlexShrink(wide, 0);
  YGNodeInsertChild(row, wide, 0);
  YGNodeInsertChild(t.root, row, 0);
  t.layout(300, YGUndefined);
  CHECK(YGNodeLayoutGetHadOverflow(row));
  CHECK(YGNodeLayoutGetHadOverflow(t.root));
}

// A grid that isn't a containing block used to skip its hidden children
// without cleaning them, and a child left dirty stops the dirtying that
// showing it again sends up.
TEST_CASE("A nested grid lays out a child shown again", "[layout][grid]")
{
  Tree t;
  t.root = t.node();
  YGNodeRef grid = t.node();
  YGNodeStyleSetDisplay(grid, YGDisplayGrid);
  YGNodeStyleSetWidth(grid, 100);
  YGNodeStyleSetGridTemplateColumnsCount(grid, 1);
  YGNodeStyleSetGridTemplateColumn(grid, 0, YGGridTrackTypeFr, 1);
  YGNodeRef shown = t.node();
  YGNodeStyleSetHeight(shown, 16);
  YGNodeRef hidden = t.node();
  YGNodeStyleSetHeight(hidden, 16);
  YGNodeStyleSetDisplay(hidden, YGDisplayNone);
  YGNodeInsertChild(grid, shown, 0);
  YGNodeInsertChild(grid, hidden, 1);
  YGNodeInsertChild(t.root, grid, 0);
  t.layout(300);
  YGNodeStyleSetDisplay(hidden, YGDisplayFlex);
  t.layout(400);
  CHECK(YGNodeLayoutGetHeight(grid) == Approx(32));
  CHECK(YGNodeLayoutGetTop(hidden) == Approx(16));
  CHECK(YGNodeLayoutGetWidth(hidden) == Approx(100));
}

// A percent gap resolves against the owner's width when justification spaces
// the children (YGJustifyMainAxis). Widening the column leaves the item clean
// and its probe's available sizes unchanged.
TEST_CASE(
    "Grid min-content follows a resized grid area through a percent gap",
    "[layout][grid]")
{
  Tree t;
  t.root = t.node();
  YGNodeStyleSetDisplay(t.root, YGDisplayGrid);
  YGNodeStyleSetGridTemplateColumnsCount(t.root, 1);
  YGNodeStyleSetGridTemplateColumn(t.root, 0, YGGridTrackTypePoints, 100);
  YGNodeStyleSetGridTemplateRowsCount(t.root, 1);
  YGNodeStyleSetGridTemplateRow(t.root, 0, YGGridTrackTypeFr, 1);
  YGNodeRef item = t.node();
  YGNodeStyleSetWidth(item, 100);
  YGNodeStyleSetGapPercent(item, YGGutterRow, 10);
  for (uint32_t i = 0; i < 2; i++)
  {
    YGNodeRef leaf = t.node();
    YGNodeStyleSetHeight(leaf, 10);
    YGNodeInsertChild(item, leaf, i);
  }
  YGNodeInsertChild(t.root, item, 0);
  t.layout();
  CHECK(YGNodeLayoutGetHeight(item) == 30);
  YGNodeStyleSetGridTemplateColumn(t.root, 0, YGGridTrackTypePoints, 200);
  t.layout();
  CHECK(YGNodeLayoutGetHeight(item) == 40);
  CHECK(YGNodeLayoutGetTop(YGNodeGetChild(item, 1)) == 30);
}

// A baseline reads measured sizes deep in its child's subtree, and a probe
// leaves them at its own sizes. Once the probe re-lays a clean node out, the
// layout after it has to as well, rather than answer it from cache with those
// sizes still beneath it. Reduced from a fuzzed case.
TEST_CASE(
    "Grid min-content keeps a cached subtree's baselines current",
    "[layout][grid]")
{
  auto build = [](Tree& t, YGNodeRef* box) {
    YGNodeRef item = t.node();
    YGNodeStyleSetGap(item, YGGutterAll, 3);
    YGNodeRef row = t.node();
    YGNodeStyleSetFlexDirection(row, YGFlexDirectionRow);
    YGNodeStyleSetAlignItems(row, YGAlignBaseline);
    YGNodeStyleSetMaxHeight(row, 120);
    *box = t.node();
    YGNodeInsertChild(row, *box, 0);
    YGNodeRef inner = t.node();
    YGNodeStyleSetFlexDirection(inner, YGFlexDirectionRow);
    YGNodeRef first = t.node();
    YGNodeStyleSetHeight(first, 20);
    YGNodeStyleSetFlexBasis(first, 0);
    YGNodeRef grandchild = t.node();
    YGNodeStyleSetMinHeightPercent(grandchild, 20);
    YGNodeInsertChild(first, grandchild, 0);
    YGNodeInsertChild(inner, first, 0);
    YGNodeRef second = t.node();
    YGNodeStyleSetHeight(second, 160);
    YGNodeInsertChild(inner, second, 1);
    YGNodeInsertChild(row, inner, 1);
    YGNodeInsertChild(item, row, 0);
    YGNodeInsertChild(item, t.node(), 1);
    t.root = t.stack();
    YGNodeInsertChild(t.root, item, 0);
  };
  Tree t;
  YGNodeRef box;
  build(t, &box);
  t.layout(0, 150);
  t.layout(400, 0);
  Tree fresh;
  YGNodeRef freshBox;
  build(fresh, &freshBox);
  fresh.layout(400, 0);
  CHECK(YGNodeLayoutGetTop(freshBox) == 24);
  CHECK(YGNodeLayoutGetTop(box) == 24);
  CHECK(boxes(t.root) == boxes(fresh.root));
}

// Laying out a shallow clone clones its shared children first
// (YGNode::cloneChildrenIfNeeded), so a probe in the clone writes positions to
// those copies. They are what has to go back, not the originals. Reduced from
// a fuzzed case.
TEST_CASE(
    "Grid min-content restores a cloned tree's positions",
    "[layout][grid]")
{
  auto build = [](Tree& t) {
    YGNodeRef inner = t.node();
    YGNodeStyleSetDisplay(inner, YGDisplayGrid);
    YGNodeRef aligned = t.node();
    YGNodeStyleSetAlignItems(aligned, YGAlignFlexEnd);
    YGNodeStyleSetWidthPercent(aligned, 50);
    YGNodeStyleSetMarginPercent(aligned, YGEdgeBottom, 10);
    YGNodeRef leaf = t.node();
    YGNodeStyleSetMinWidth(leaf, 40);
    YGNodeInsertChild(aligned, leaf, 0);
    YGNodeInsertChild(inner, aligned, 0);
    YGNodeRef wrapper = t.node();
    YGNodeInsertChild(wrapper, inner, 0);
    YGNodeRef item = t.node();
    YGNodeInsertChild(item, wrapper, 0);
    t.root = t.node();
    YGNodeStyleSetDisplay(t.root, YGDisplayGrid);
    YGNodeStyleSetGridTemplateRowsCount(t.root, 1);
    YGNodeStyleSetGridTemplateRow(t.root, 0, YGGridTrackTypeFr, 1);
    YGNodeInsertChild(t.root, item, 0);
  };
  Tree t;
  build(t);
  t.layout(YGUndefined, 300);
  YGNodeRef copy = YGNodeClone(t.root);
  YGNodeCalculateLayout(copy, YGUndefined, 60, YGDirectionLTR);
  Tree fresh;
  build(fresh);
  fresh.layout(YGUndefined, 60);
  CHECK(boxes(copy) == boxes(fresh.root));
  YGNodeFreeRecursive(copy);
}

TEST_CASE(
    "A grid's probes reuse a percent padded item they laid out before",
    "[layout]")
{
  Tree t;
  t.root = t.node();
  YGNodeStyleSetDisplay(t.root, YGDisplayGrid);
  YGNodeStyleSetWidth(t.root, 300);
  YGNodeStyleSetGridTemplateColumnsCount(t.root, 2);
  YGNodeStyleSetGridTemplateColumn(t.root, 0, YGGridTrackTypeAuto, 0);
  YGNodeStyleSetGridTemplateColumn(t.root, 1, YGGridTrackTypeAuto, 0);
  YGNodeRef text = t.node();
  YGNodeSetMeasureFunc(text, measureCountingProbes);
  YGNodeStyleSetPaddingPercent(text, YGEdgeLeft, 10);
  YGNodeRef other = t.node();
  YGNodeStyleSetWidth(other, 40);
  YGNodeStyleSetHeight(other, 10);
  YGNodeInsertChild(t.root, text, 0);
  YGNodeInsertChild(t.root, other, 1);
  t.layout();

  s_probeMeasures = 0;
  YGNodeStyleSetHeight(other, 12);
  t.layout();
  CHECK(s_probeMeasures == 0);
  CHECK(YGNodeLayoutGetPadding(text, YGEdgeLeft) == Approx(6));
}

// A layout a measure starts inside a probe used to run as part of it: the
// tree's measures saw the probe, and the probe put the tree's nodes back after
// it. A tree the measure kept lost its layout, and one it freed was written to
// once it was gone.
TEST_CASE(
    "Grid min-content leaves alone a tree its measure lays out",
    "[layout][grid]")
{
  SECTION("freed")
  {
    Tree t;
    t.root = t.stack();
    YGNodeStyleSetWidth(t.root, 100);
    YGNodeRef item = t.node();
    YGNodeSetMeasureFunc(item, measureLayingOutAnotherTree);
    YGNodeInsertChild(t.root, item, 0);
    s_probesLayingOut = 0;
    s_probeMeasures = 0;
    t.layout();
    CHECK(s_probesLayingOut > 0);
    CHECK(s_probeMeasures == 0);
  }
  SECTION("kept")
  {
    Tree t;
    YGNodeRef kept = t.node();
    YGNodeStyleSetAlignItems(kept, YGAlignFlexStart);
    YGNodeRef leaf = t.node();
    YGNodeSetMeasureFunc(leaf, measureCountingProbes);
    YGNodeStyleSetMargin(leaf, YGEdgeLeft, 7);
    YGNodeInsertChild(kept, leaf, 0);
    t.root = t.stack();
    YGNodeStyleSetWidth(t.root, 100);
    YGNodeRef item = t.node();
    YGNodeSetMeasureFunc(item, measureLayingOutAnotherTree);
    YGNodeSetContext(item, kept);
    YGNodeInsertChild(t.root, item, 0);
    s_probesLayingOut = 0;
    s_probeMeasures = 0;
    s_keptLayouts.clear();
    t.layout();
    CHECK(s_probesLayingOut > 0);
    CHECK(s_probeMeasures == 0);
    const std::string laidOut = "0.000000,0.000000 200.000000x200.000000\n"
                                "7.000000,0.000000 60.000000x20.000000\n";
    for (const std::string& layout : s_keptLayouts)
    {
      CHECK(layout == laidOut);
    }
    CHECK(boxes(kept) == laidOut);
    YGNodeFreeRecursive(kept);
  }
}

// The probe used to be raised on the config, so another thread laying out on
// it mid probe measured as part of it, and saved its nodes where nothing put
// them back.
TEST_CASE("Grid min-content stays on the thread probing", "[layout][grid]")
{
  Tree t;
  t.root = t.stack();
  YGNodeStyleSetWidth(t.root, 100);
  YGNodeRef item = t.node();
  YGNodeSetMeasureFunc(item, measureOnAnotherThread);
  YGNodeInsertChild(t.root, item, 0);
  s_probesLayingOut = 0;
  s_probeMeasures = 0;
  s_otherThreadSawProbe = false;
  t.layout();
  CHECK(s_probesLayingOut > 0);
  CHECK_FALSE(s_otherThreadSawProbe);
  CHECK(s_probeMeasures == 0);
}
