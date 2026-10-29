/*
 * Copyright 2026 Rive
 */

// Direct Yoga node tests for reusing a container's measurement.
//
// A container that sizes to its content can reuse a measurement it made under
// other constraints when its measured size still fits the new ones, the way a
// node with a measure function already does. These check that it does, and
// that a relayout which reuses measurements lands exactly where a fresh
// layout of the same tree does, including for the containers that must not
// reuse them.

#include "yoga/YGNode.h"
#include "yoga/Yoga.h"
#include "yoga/event/event.h"
#include <catch.hpp>
#include <functional>
#include <unordered_map>

namespace
{
// Non-cached measures per node while counting is on.
bool s_counting = false;
std::unordered_map<const YGNode*, int> s_measures;

void countMeasures()
{
  static bool subscribed = false;
  if (!subscribed)
  {
    subscribed = true;
    facebook::yoga::Event::subscribe([](const YGNode& node,
                                        facebook::yoga::Event::Type type,
                                        facebook::yoga::Event::Data data) {
      if (!s_counting || type != facebook::yoga::Event::NodeLayout)
      {
        return;
      }
      auto& layout = data.get<facebook::yoga::Event::NodeLayout>();
      if (layout.layoutType == facebook::yoga::LayoutType::kMeasure)
      {
        s_measures[&node]++;
      }
    });
  }
  s_measures.clear();
  s_counting = true;
}

YGNodeRef box(float width, float height)
{
  YGNodeRef node = YGNodeNew();
  YGNodeStyleSetWidth(node, width);
  YGNodeStyleSetHeight(node, height);
  return node;
}

// A container that hugs its children: no size of its own.
YGNodeRef hug(YGFlexDirection direction)
{
  YGNodeRef node = YGNodeNew();
  YGNodeStyleSetFlexDirection(node, direction);
  YGNodeStyleSetFlexShrink(node, 0);
  return node;
}

void add(YGNodeRef parent, YGNodeRef child)
{
  YGNodeInsertChild(parent, child, YGNodeGetChildCount(parent));
}

void layout(YGNodeRef root)
{
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
}

// Every node of [a] has the same frame as its counterpart in [b].
void checkSameLayout(YGNodeRef a, YGNodeRef b)
{
  CHECK(YGNodeLayoutGetLeft(a) == Approx(YGNodeLayoutGetLeft(b)));
  CHECK(YGNodeLayoutGetTop(a) == Approx(YGNodeLayoutGetTop(b)));
  CHECK(YGNodeLayoutGetWidth(a) == Approx(YGNodeLayoutGetWidth(b)));
  CHECK(YGNodeLayoutGetHeight(a) == Approx(YGNodeLayoutGetHeight(b)));
  REQUIRE(YGNodeGetChildCount(a) == YGNodeGetChildCount(b));
  for (uint32_t i = 0; i < YGNodeGetChildCount(a); i++)
  {
    checkSameLayout(YGNodeGetChild(a, i), YGNodeGetChild(b, i));
  }
}

// A root of [size] holding a hug row whose content [fill] adds. An undefined
// width or height leaves the root without one.
YGNodeRef hugRowIn(YGSize size, const std::function<void(YGNodeRef)>& fill)
{
  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
  YGNodeStyleSetWidth(root, size.width);
  YGNodeStyleSetHeight(root, size.height);
  YGNodeRef row = hug(YGFlexDirectionRow);
  fill(row);
  add(root, row);
  return root;
}

// Lays [fill]'s tree out at [from], then again at [to], and checks it against
// a fresh tree laid out at [to].
void checkRelayoutMatchesFresh(
    const std::function<void(YGNodeRef)>& fill,
    YGSize from = {400, 100},
    YGSize to = {300, 100})
{
  YGNodeRef relaid = hugRowIn(from, fill);
  layout(relaid);
  YGNodeStyleSetWidth(relaid, to.width);
  YGNodeStyleSetHeight(relaid, to.height);
  layout(relaid);
  YGNodeRef fresh = hugRowIn(to, fill);
  layout(fresh);
  checkSameLayout(relaid, fresh);
  YGNodeFreeRecursive(relaid);
  YGNodeFreeRecursive(fresh);
}

// A row with a 10 margin all around and an empty row in it. Offered a height
// of 20 or less, it has nothing left past its margins, and Yoga measures a
// container offered at most nothing as the space it's offered in the other
// axis, not as its content.
YGNodeRef marginRow()
{
  YGNodeRef row = hug(YGFlexDirectionRow);
  YGNodeStyleSetMargin(row, YGEdgeAll, 10);
  add(row, hug(YGFlexDirectionRow));
  return row;
}
} // namespace

TEST_CASE(
    "a hug container is not measured again under stricter limits it fits",
    "[layout][yoga-measure-cache]")
{
  // The shape that made a long list slow: a list inside two hug columns, in a
  // row that spaces them out. Yoga measures each row under every limit the
  // columns pass down while resolving their own sizes: 6 times. All but 2
  // reuse a measurement: the first, and the one at two exact sizes, which is
  // quicker measured than looked up.
  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetJustifyContent(root, YGJustifySpaceBetween);
  YGNodeStyleSetWidth(root, 1000);
  YGNodeStyleSetHeight(root, 800);
  YGNodeRef outer = hug(YGFlexDirectionColumn);
  YGNodeRef inner = hug(YGFlexDirectionColumn);
  add(root, outer);
  add(outer, inner);
  constexpr int kRows = 20;
  std::vector<YGNodeRef> rows;
  for (int i = 0; i < kRows; i++)
  {
    YGNodeRef row = hug(YGFlexDirectionRow);
    add(row, box(100, 20));
    add(row, box(60, 20));
    add(inner, row);
    rows.push_back(row);
  }

  countMeasures();
  layout(root);
  s_counting = false;

  for (YGNodeRef row : rows)
  {
    CHECK(s_measures[row] == 2);
    CHECK(YGNodeLayoutGetWidth(row) == Approx(160));
    CHECK(YGNodeLayoutGetHeight(row) == Approx(20));
  }
  CHECK(YGNodeLayoutGetWidth(inner) == Approx(160));
  CHECK(YGNodeLayoutGetHeight(inner) == Approx(20 * kRows));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a relayout that reuses container measurements matches a fresh layout",
    "[layout][yoga-measure-cache]")
{
  SECTION("fixed content")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      add(row, box(100, 20));
      add(row, box(50, 30));
    });
  }
  SECTION("a nested hug column")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef column = hug(YGFlexDirectionColumn);
      add(column, box(80, 10));
      add(column, box(40, 10));
      add(row, column);
      add(row, box(30, 30));
    });
  }
  SECTION("a child sized as a percentage")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef child = YGNodeNew();
      YGNodeStyleSetWidthPercent(child, 50);
      YGNodeStyleSetHeight(child, 20);
      add(row, child);
    });
  }
  SECTION("a grandchild sized as a percentage")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef column = hug(YGFlexDirectionColumn);
      YGNodeRef child = YGNodeNew();
      YGNodeStyleSetWidthPercent(child, 25);
      YGNodeStyleSetHeight(child, 20);
      add(column, child);
      add(row, column);
    });
  }
  SECTION("a child with a percentage margin")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef child = box(40, 20);
      YGNodeStyleSetMarginPercent(child, YGEdgeLeft, 10);
      add(row, child);
    });
  }
  SECTION("a child with an aspect ratio")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef child = YGNodeNew();
      YGNodeStyleSetHeight(child, 20);
      YGNodeStyleSetAspectRatio(child, 3);
      add(row, child);
    });
  }
  SECTION("a scrolling child")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef child = hug(YGFlexDirectionRow);
      YGNodeStyleSetOverflow(child, YGOverflowScroll);
      add(child, box(500, 20));
      add(row, child);
    });
  }
  SECTION("a flexible child")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeRef child = box(40, 20);
      YGNodeStyleSetFlexGrow(child, 1);
      add(row, child);
      add(row, box(30, 20));
    });
  }
  SECTION("wrapping content")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeStyleSetFlexWrap(row, YGWrapWrap);
      for (int i = 0; i < 5; i++)
      {
        add(row, box(90, 20));
      }
    });
  }
  SECTION("wrapping content under a limit a fraction narrower")
  {
    // Both limits round to 100, but only the first fits the two boxes on a
    // line.
    checkRelayoutMatchesFresh(
        [](YGNodeRef row) {
          YGNodeStyleSetFlexWrap(row, YGWrapWrap);
          add(row, box(50.1f, 20));
          add(row, box(50.1f, 20));
        },
        {100.4f, 100},
        {100.1f, 100});
  }
  SECTION("a percentage minimum size of its own")
  {
    checkRelayoutMatchesFresh([](YGNodeRef row) {
      YGNodeStyleSetMinWidthPercent(row, 50);
      add(row, box(40, 20));
    });
  }
  SECTION("a limit that only just holds content with nothing empty in it")
  {
    checkRelayoutMatchesFresh(
        [](YGNodeRef row) {
          add(row, box(100, 20));
          add(row, box(60, 20));
        },
        {400, 100},
        {160, 100});
  }
  SECTION("a limit that only just holds the content")
  {
    // 200 by 500, the margin row measures as nothing and its parent as its
    // margins, 20 by 20. 20 high only just holds that, and leaves the margin
    // row nothing: it measures as the 180 left in the width.
    checkRelayoutMatchesFresh(
        [](YGNodeRef row) { add(row, marginRow()); },
        {200, 500},
        {200, 20});
  }
  SECTION("content that was sized to the space it was offered")
  {
    // With no width, the margin row is offered no width and nothing past its
    // margins in the height, and measures as nothing. 100 wide, it measures
    // as the 80 left. Its parent's 20 wide measurement fits the new limit but
    // was the margin row's offered space, not its content.
    checkRelayoutMatchesFresh(
        [](YGNodeRef row) { add(row, marginRow()); },
        {YGUndefined, 20},
        {100, 20});
  }
}

TEST_CASE(
    "a style change below a container reaches its next layout",
    "[layout][yoga-measure-cache]")
{
  // Laid out once, the column's flags say its content sizes it. A child
  // sized as a percentage since then resolves against the space offered.
  auto fill = [](YGNodeRef row, bool percent) {
    YGNodeRef column = hug(YGFlexDirectionColumn);
    YGNodeRef child = box(40, 20);
    if (percent)
    {
      YGNodeStyleSetWidthPercent(child, 50);
    }
    add(column, child);
    add(row, column);
  };
  YGNodeRef relaid = hugRowIn({400, 100}, [&](YGNodeRef row) {
    fill(row, false);
  });
  layout(relaid);
  YGNodeRef row = YGNodeGetChild(relaid, 0);
  YGNodeStyleSetWidthPercent(YGNodeGetChild(YGNodeGetChild(row, 0), 0), 50);
  YGNodeStyleSetWidth(relaid, 300);
  layout(relaid);
  YGNodeRef fresh = hugRowIn({300, 100}, [&](YGNodeRef row) {
    fill(row, true);
  });
  layout(fresh);
  checkSameLayout(relaid, fresh);
  YGNodeFreeRecursive(relaid);
  YGNodeFreeRecursive(fresh);
}

TEST_CASE(
    "a container offered an exact size is measured under it",
    "[layout][yoga-measure-cache]")
{
  // A row measured at most 500 by 500 is 82 high. The hug column around it
  // then measures it again exactly 82 high, which stretches the image to 82
  // high, and the image is twice as wide as it is high. The column's
  // measured width is the one the root lays it out at.
  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetFlexDirection(root, YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(root, YGAlignFlexStart);
  YGNodeStyleSetWidth(root, 500);
  YGNodeStyleSetHeight(root, 500);
  YGNodeRef column = hug(YGFlexDirectionColumn);
  YGNodeStyleSetAlignItems(column, YGAlignFlexStart);
  YGNodeRef row = hug(YGFlexDirectionRow);
  add(row, box(20, 82));
  YGNodeRef image = YGNodeNew();
  YGNodeSetMeasureFunc(
      image,
      [](YGNodeRef,
         float,
         YGMeasureMode,
         float height,
         YGMeasureMode heightMode) -> YGSize {
        if (heightMode == YGMeasureModeExactly)
        {
          return {height * 2, height};
        }
        return {10, 10};
      });
  add(row, image);
  add(column, row);
  add(root, column);

  layout(root);

  CHECK(YGNodeLayoutGetHeight(image) == Approx(82));
  CHECK(YGNodeLayoutGetWidth(image) == Approx(164));
  CHECK(YGNodeLayoutGetWidth(row) == Approx(184));
  CHECK(YGNodeLayoutGetWidth(column) == Approx(184));
  YGNodeFreeRecursive(root);
}
