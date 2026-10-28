/*
 * Copyright 2026 Rive
 */

// Direct Yoga node tests for the explicit aspect ratio axis.
//
// Every ratio here is 2: the width is twice the height. Auto is Yoga's own
// rule, the owner's main axis is the anchor and the cross axis is recomputed
// from it. Width and height anchor that axis whatever the owner's direction;
// the other axis is always derived, so a cross-axis anchor pins the main size
// to the derived value and never flexes.

#include "yoga/Yoga.h"
#include <catch.hpp>

namespace
{
constexpr float kRatio = 2.0f;

YGNodeRef container(
    YGFlexDirection direction,
    float width = 200,
    float height = 120)
{
  YGNodeRef node = YGNodeNew();
  YGNodeStyleSetFlexDirection(node, direction);
  YGNodeStyleSetAlignItems(node, YGAlignFlexStart);
  if (width > 0)
  {
    YGNodeStyleSetWidth(node, width);
  }
  if (height > 0)
  {
    YGNodeStyleSetHeight(node, height);
  }
  return node;
}

YGNodeRef ratioNode(YGAspectRatioAxis axis)
{
  YGNodeRef node = YGNodeNew();
  YGNodeStyleSetAspectRatio(node, kRatio);
  YGNodeStyleSetAspectRatioAxis(node, axis);
  return node;
}

YGNodeRef ratioBox(YGAspectRatioAxis axis, float width, float height)
{
  YGNodeRef node = ratioNode(axis);
  YGNodeStyleSetWidth(node, width);
  YGNodeStyleSetHeight(node, height);
  return node;
}

// What a hugging axis measures: 30x20 content.
void addContent(YGNodeRef node)
{
  YGNodeRef content = YGNodeNew();
  YGNodeStyleSetWidth(content, 30);
  YGNodeStyleSetHeight(content, 20);
  YGNodeInsertChild(node, content, YGNodeGetChildCount(node));
}

void layout(YGNodeRef root)
{
  YGNodeCalculateLayout(root, YGUndefined, YGUndefined, YGDirectionLTR);
}

void checkSize(YGNodeRef node, float width, float height)
{
  CHECK(YGNodeLayoutGetWidth(node) == Approx(width));
  CHECK(YGNodeLayoutGetHeight(node) == Approx(height));
}
} // namespace

TEST_CASE(
    "aspect ratio axis auto anchors the owner's main axis",
    "[layout][aspect-ratio-axis]")
{
  // A row keeps the 60 width and halves the height; a column keeps the 40
  // height and doubles the width. Neither is the authored box.
  {
    YGNodeRef root = container(YGFlexDirectionRow);
    YGNodeRef child = ratioBox(YGAspectRatioAxisAuto, 60, 40);
    YGNodeInsertChild(root, child, 0);
    layout(root);
    checkSize(child, 60, 30);
    YGNodeFreeRecursive(root);
  }
  {
    YGNodeRef root = container(YGFlexDirectionColumn);
    YGNodeRef child = ratioBox(YGAspectRatioAxisAuto, 60, 40);
    YGNodeInsertChild(root, child, 0);
    layout(root);
    checkSize(child, 80, 40);
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "aspect ratio axis width anchors the width in either direction",
    "[layout][aspect-ratio-axis]")
{
  for (YGFlexDirection direction : {YGFlexDirectionRow, YGFlexDirectionColumn})
  {
    YGNodeRef root = container(direction);
    YGNodeRef child = ratioBox(YGAspectRatioAxisWidth, 60, 40);
    YGNodeInsertChild(root, child, 0);
    layout(root);
    checkSize(child, 60, 30);
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "aspect ratio axis height anchors the height in either direction",
    "[layout][aspect-ratio-axis]")
{
  for (YGFlexDirection direction : {YGFlexDirectionRow, YGFlexDirectionColumn})
  {
    YGNodeRef root = container(direction);
    YGNodeRef child = ratioBox(YGAspectRatioAxisHeight, 60, 40);
    YGNodeInsertChild(root, child, 0);
    layout(root);
    checkSize(child, 80, 40);
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "a cross-axis anchor does not flex and leaves the free space to "
    "its siblings",
    "[layout][aspect-ratio-axis]")
{
  // The anchored child would fill the row; its width is 80 from the ratio
  // instead, and the flexible sibling takes everything else. Both passes of
  // the free-space distribution must agree on that, or the sibling lands
  // short.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef anchored = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetHeight(anchored, 40);
  YGNodeStyleSetFlexGrow(anchored, 1);
  YGNodeStyleSetFlexShrink(anchored, 1);
  YGNodeRef sibling = YGNodeNew();
  YGNodeStyleSetFlexGrow(sibling, 1);
  YGNodeStyleSetFlexShrink(sibling, 1);
  YGNodeStyleSetFlexBasis(sibling, 0);
  YGNodeStyleSetHeight(sibling, 10);
  YGNodeInsertChild(root, anchored, 0);
  YGNodeInsertChild(root, sibling, 1);
  layout(root);
  checkSize(anchored, 80, 40);
  CHECK(YGNodeLayoutGetLeft(sibling) == Approx(80));
  checkSize(sibling, 120, 10);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a cross-axis anchor that fills takes the line's cross size",
    "[layout][aspect-ratio-axis]")
{
  // Stretched to the row's 120 height, the width follows at 240 and the row
  // overflows: the derived axis never shrinks.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeStyleSetAlignItems(root, YGAlignStretch);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetFlexGrow(child, 1);
  YGNodeStyleSetFlexShrink(child, 1);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 240, 120);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging cross-axis anchor measures its content first",
    "[layout][aspect-ratio-axis]")
{
  // The height is the content's 20, measured with the width unconstrained,
  // and the width is 40 from it rather than the content's 30.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging main-axis anchor measures its content and derives the "
    "cross axis",
    "[layout][aspect-ratio-axis]")
{
  // The width is the content's 30; the fixed 40 height is overridden by 15.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetHeight(child, 40);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 30, 15);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a main-axis anchor that fills derives the cross axis from the "
    "flexed size",
    "[layout][aspect-ratio-axis]")
{
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef sibling = YGNodeNew();
  YGNodeStyleSetWidth(sibling, 60);
  YGNodeStyleSetHeight(sibling, 40);
  YGNodeRef child = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetHeight(child, 40);
  YGNodeStyleSetFlexGrow(child, 1);
  YGNodeStyleSetFlexShrink(child, 1);
  YGNodeInsertChild(root, sibling, 0);
  YGNodeInsertChild(root, child, 1);
  layout(root);
  checkSize(child, 140, 70);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a cross-axis anchor ignores flex grow on the derived axis",
    "[layout][aspect-ratio-axis]")
{
  // A column: the height is the main axis, and it is derived from the fixed
  // 60 width however much the child asks to grow.
  YGNodeRef root = container(YGFlexDirectionColumn);
  YGNodeRef sibling = YGNodeNew();
  YGNodeStyleSetWidth(sibling, 60);
  YGNodeStyleSetHeight(sibling, 40);
  YGNodeRef child = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetWidth(child, 60);
  YGNodeStyleSetFlexGrow(child, 1);
  YGNodeStyleSetFlexShrink(child, 1);
  YGNodeInsertChild(root, sibling, 0);
  YGNodeInsertChild(root, child, 1);
  layout(root);
  checkSize(child, 60, 30);
  CHECK(YGNodeLayoutGetTop(child) == Approx(40));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "min and max on the derived axis clamp it and break the ratio",
    "[layout][aspect-ratio-axis]")
{
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetHeight(child, 40);
  YGNodeStyleSetMaxWidth(child, 50);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 50, 40);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a cross-axis anchor that fills a hugging owner falls back to its "
    "content",
    "[layout][aspect-ratio-axis]")
{
  // The row has no height to stretch to, so the anchored height is the
  // content's 20 and the row hugs it.
  YGNodeRef root = container(YGFlexDirectionRow, 200, 0);
  YGNodeStyleSetAlignItems(root, YGAlignStretch);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetFlexGrow(child, 1);
  YGNodeStyleSetFlexShrink(child, 1);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  CHECK(YGNodeLayoutGetHeight(root) == Approx(20));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "an absolute child computes from the anchor even when both sizes "
    "are set",
    "[layout][aspect-ratio-axis]")
{
  struct Case
  {
    YGAspectRatioAxis axis;
    float width;
    float height;
  };
  // Auto keeps the pre-lock rule: both set, nothing derived.
  for (const Case& c :
       {Case{YGAspectRatioAxisAuto, 60, 40},
        Case{YGAspectRatioAxisWidth, 60, 30},
        Case{YGAspectRatioAxisHeight, 80, 40}})
  {
    YGNodeRef root = container(YGFlexDirectionRow);
    YGNodeRef child = ratioBox(c.axis, 60, 40);
    YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
    YGNodeStyleSetPosition(child, YGEdgeLeft, 40);
    YGNodeStyleSetPosition(child, YGEdgeTop, 25);
    YGNodeInsertChild(root, child, 0);
    layout(root);
    checkSize(child, c.width, c.height);
    CHECK(YGNodeLayoutGetLeft(child) == Approx(40));
    CHECK(YGNodeLayoutGetTop(child) == Approx(25));
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "an absolute child with a hugging anchor measures it first",
    "[layout][aspect-ratio-axis]")
{
  // The height anchor has no size, so it is the content's 20 and the fixed
  // 60 width gives way to 40.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetWidth(child, 60);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "an absolute anchor is bounded by its own max before the other "
    "side follows",
    "[layout][aspect-ratio-axis]")
{
  // A declared 100 width with a max of 60 is 60 in the end, so the height
  // follows the 60, not the 100 it was declared at.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioBox(YGAspectRatioAxisWidth, 100, 40);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  YGNodeStyleSetMaxWidth(child, 60);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 60, 30);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging cross-axis anchor is not measured again at the derived "
    "main size",
    "[layout][aspect-ratio-axis]")
{
  // Two 30x20 children measure 60x20 unconstrained, so the height anchor is
  // 20 and the width 40. Laid out at 40 the content would wrap to 40 tall;
  // the anchor holds at 20 instead.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a cross-axis anchor that fills a hugging owner keeps its content "
    "size beside a taller sibling",
    "[layout][aspect-ratio-axis]")
{
  // The owner's height comes from the 40-tall sibling, too late for the
  // anchor: it was derived from its content's 20 with its basis and stays
  // 40x20, rather than stretching to 40 and leaving its width behind.
  YGNodeRef root = container(YGFlexDirectionRow, 200, 0);
  YGNodeStyleSetAlignItems(root, YGAlignStretch);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  YGNodeRef sibling = YGNodeNew();
  YGNodeStyleSetWidth(sibling, 60);
  YGNodeStyleSetHeight(sibling, 40);
  YGNodeInsertChild(root, sibling, 1);
  layout(root);
  checkSize(child, 40, 20);
  CHECK(YGNodeLayoutGetHeight(root) == Approx(40));
  CHECK(YGNodeLayoutGetLeft(sibling) == Approx(40));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging cross-axis anchor measures free of the computed axis's "
    "max",
    "[layout][aspect-ratio-axis]")
{
  // At most 30 tall the wrapping column would break into two columns and
  // the width anchor would read 60; measured free it is 30, so 30x15, and
  // the max does not bind the derived 15.
  YGNodeRef root = container(YGFlexDirectionColumn);
  YGNodeRef child = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionColumn);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  YGNodeStyleSetMaxHeight(child, 30);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 30, 15);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging main-axis anchor measures free of the computed axis's "
    "authored size",
    "[layout][aspect-ratio-axis]")
{
  // The same node as the cross-axis case above, in a column: the height
  // anchor is now the owner's main axis. At the authored 30 width the
  // wrapping row would stack and the anchor would read 40; measured free it
  // is 20, so 40x20 in a column as in a row.
  YGNodeRef root = container(YGFlexDirectionColumn);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetWidth(child, 30);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging main-axis anchor measures free of the owner's width",
    "[layout][aspect-ratio-axis]")
{
  // With no width of its own the child would be offered the 30-wide
  // column's width to wrap in, and the height anchor would read 40. The
  // width the ratio computes takes no such offer: 40x20.
  YGNodeRef root = container(YGFlexDirectionColumn, 30, 120);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging main-axis anchor measures free of the computed axis's max",
    "[layout][aspect-ratio-axis]")
{
  // The mirror of the cross-axis max case, in a row: at most 30 tall the
  // wrapping column would break into two columns and the width anchor would
  // read 60. Measured free it is 30, so 30x15, and the max does not bind
  // the derived 15.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionColumn);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  YGNodeStyleSetMaxHeight(child, 30);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 30, 15);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "an absolute hugging anchor measures free of the computed axis's "
    "authored size",
    "[layout][aspect-ratio-axis]")
{
  // At the authored 30 width the wrapping row would stack its children and
  // the height anchor would read 40; measured free it is 20, so 40x20.
  YGNodeRef root = container(YGFlexDirectionRow);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  YGNodeStyleSetWidth(child, 30);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a cross-axis anchor keeps its size through a wrapping owner's line "
    "stretch",
    "[layout][aspect-ratio-axis]")
{
  // The anchor shares its 40-tall line with a sibling and a third child
  // wraps to a second line; the multi-line pass stretches the line's
  // children to 40 but leaves the anchor at its 40x20.
  YGNodeRef root = container(YGFlexDirectionRow, 120, 0);
  YGNodeStyleSetAlignItems(root, YGAlignStretch);
  YGNodeStyleSetFlexWrap(root, YGWrapWrap);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  for (int i = 1; i <= 2; i++)
  {
    YGNodeRef sibling = YGNodeNew();
    YGNodeStyleSetWidth(sibling, 60);
    YGNodeStyleSetHeight(sibling, 40);
    YGNodeInsertChild(root, sibling, i);
  }
  layout(root);
  checkSize(child, 40, 20);
  CHECK(YGNodeLayoutGetHeight(root) == Approx(80));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "an absolute hugging anchor in a column measures free of the "
    "owner's width",
    "[layout][aspect-ratio-axis]")
{
  // A column owner offers its width to an absolute child with none of its
  // own; at 30 the wrapping row would stack and the height anchor would
  // read 40. The width the ratio computes takes no such offer: 40x20.
  YGNodeRef root = container(YGFlexDirectionColumn, 30, 120);
  YGNodeRef child = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetPositionType(child, YGPositionTypeAbsolute);
  YGNodeStyleSetFlexDirection(child, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(child, YGWrapWrap);
  addContent(child);
  addContent(child);
  YGNodeInsertChild(root, child, 0);
  layout(root);
  checkSize(child, 40, 20);
  YGNodeFreeRecursive(root);
}

namespace
{
// A 200x120 grid of 60|140 columns and 40|80 rows, with the item in the
// 140x40 cell at column 2, row 1.
YGNodeRef grid(YGNodeRef item)
{
  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetDisplay(root, YGDisplayGrid);
  YGNodeStyleSetWidth(root, 200);
  YGNodeStyleSetHeight(root, 120);
  YGNodeStyleSetGridTemplateColumnsCount(root, 2);
  YGNodeStyleSetGridTemplateColumn(root, 0, YGGridTrackTypePoints, 60);
  YGNodeStyleSetGridTemplateColumn(root, 1, YGGridTrackTypePoints, 140);
  YGNodeStyleSetGridTemplateRowsCount(root, 2);
  YGNodeStyleSetGridTemplateRow(root, 0, YGGridTrackTypePoints, 40);
  YGNodeStyleSetGridTemplateRow(root, 1, YGGridTrackTypePoints, 80);
  YGNodeStyleSetGridColumnStart(item, 2);
  YGNodeStyleSetGridRowStart(item, 1);
  YGNodeInsertChild(root, item, 0);
  return root;
}
} // namespace

TEST_CASE(
    "a grid item stretched both ways computes from the anchor",
    "[layout][aspect-ratio-axis]")
{
  struct Case
  {
    YGAspectRatioAxis axis;
    float width;
    float height;
  };
  // Auto derives the width from the height; a width anchor keeps the
  // stretched 140 and derives the height.
  for (const Case& c :
       {Case{YGAspectRatioAxisAuto, 80, 40},
        Case{YGAspectRatioAxisWidth, 140, 70},
        Case{YGAspectRatioAxisHeight, 80, 40}})
  {
    YGNodeRef item = ratioNode(c.axis);
    YGNodeStyleSetJustifySelf(item, YGJustifyStretch);
    YGNodeStyleSetAlignSelf(item, YGAlignStretch);
    YGNodeRef root = grid(item);
    layout(root);
    checkSize(item, c.width, c.height);
    CHECK(YGNodeLayoutGetLeft(item) == Approx(60));
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "a fixed grid item ignores the ratio under auto and follows the "
    "anchor otherwise",
    "[layout][aspect-ratio-axis]")
{
  struct Case
  {
    YGAspectRatioAxis axis;
    float width;
    float height;
  };
  for (const Case& c :
       {Case{YGAspectRatioAxisAuto, 60, 40},
        Case{YGAspectRatioAxisWidth, 60, 30},
        Case{YGAspectRatioAxisHeight, 80, 40}})
  {
    YGNodeRef item = ratioBox(c.axis, 60, 40);
    YGNodeStyleSetJustifySelf(item, YGJustifyFlexStart);
    YGNodeStyleSetAlignSelf(item, YGAlignFlexStart);
    YGNodeRef root = grid(item);
    layout(root);
    checkSize(item, c.width, c.height);
    YGNodeFreeRecursive(root);
  }
}

TEST_CASE(
    "a hugging grid anchor measures its content first",
    "[layout][aspect-ratio-axis]")
{
  // The width anchor hugs the content's 30, and the stretched height gives
  // way to 15.
  YGNodeRef item = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetJustifySelf(item, YGJustifyFlexStart);
  YGNodeStyleSetAlignSelf(item, YGAlignStretch);
  addContent(item);
  YGNodeRef root = grid(item);
  layout(root);
  checkSize(item, 30, 15);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a stretched grid anchor is bounded by its own max before the "
    "other side follows",
    "[layout][aspect-ratio-axis]")
{
  // The 140 cell stretches the width anchor, its max of 60 bounds it, and
  // the height follows the 60.
  YGNodeRef item = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetJustifySelf(item, YGJustifyStretch);
  YGNodeStyleSetAlignSelf(item, YGAlignStretch);
  YGNodeStyleSetMaxWidth(item, 60);
  YGNodeRef root = grid(item);
  layout(root);
  checkSize(item, 60, 30);
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging grid anchor sizes an auto track by the derived side",
    "[layout][aspect-ratio-axis]")
{
  // The width anchor hugs its content's 30, so the item is 30x15 and the
  // auto row it sits in is 15 tall -- not the 40 it was authored at, which
  // the sibling stretched to the row would otherwise show.
  YGNodeRef root = YGNodeNew();
  YGNodeStyleSetDisplay(root, YGDisplayGrid);
  YGNodeStyleSetWidth(root, 200);
  YGNodeStyleSetHeight(root, 120);
  YGNodeStyleSetGridTemplateColumnsCount(root, 2);
  YGNodeStyleSetGridTemplateColumn(root, 0, YGGridTrackTypePoints, 60);
  YGNodeStyleSetGridTemplateColumn(root, 1, YGGridTrackTypePoints, 140);
  YGNodeStyleSetGridTemplateRowsCount(root, 2);
  YGNodeStyleSetGridTemplateRow(root, 0, YGGridTrackTypeAuto, 0);
  YGNodeStyleSetGridTemplateRow(root, 1, YGGridTrackTypePoints, 80);
  YGNodeRef item = ratioNode(YGAspectRatioAxisWidth);
  YGNodeStyleSetHeight(item, 40);
  YGNodeStyleSetJustifySelf(item, YGJustifyFlexStart);
  YGNodeStyleSetAlignSelf(item, YGAlignFlexStart);
  addContent(item);
  YGNodeStyleSetGridColumnStart(item, 1);
  YGNodeStyleSetGridRowStart(item, 1);
  YGNodeInsertChild(root, item, 0);
  YGNodeRef sibling = YGNodeNew();
  YGNodeStyleSetAlignSelf(sibling, YGAlignStretch);
  YGNodeStyleSetGridColumnStart(sibling, 2);
  YGNodeStyleSetGridRowStart(sibling, 1);
  YGNodeInsertChild(root, sibling, 1);
  layout(root);
  checkSize(item, 30, 15);
  CHECK(YGNodeLayoutGetHeight(sibling) == Approx(15));
  YGNodeFreeRecursive(root);
}

TEST_CASE(
    "a hugging grid anchor measures free of the computed axis's "
    "authored size",
    "[layout][aspect-ratio-axis]")
{
  // At the authored 30 width the wrapping row would stack its children and
  // the height anchor would read 40; measured free it is 20, so 40x20.
  YGNodeRef item = ratioNode(YGAspectRatioAxisHeight);
  YGNodeStyleSetWidth(item, 30);
  YGNodeStyleSetFlexDirection(item, YGFlexDirectionRow);
  YGNodeStyleSetFlexWrap(item, YGWrapWrap);
  YGNodeStyleSetJustifySelf(item, YGJustifyFlexStart);
  YGNodeStyleSetAlignSelf(item, YGAlignFlexStart);
  addContent(item);
  addContent(item);
  YGNodeRef root = grid(item);
  layout(root);
  checkSize(item, 40, 20);
  YGNodeFreeRecursive(root);
}
