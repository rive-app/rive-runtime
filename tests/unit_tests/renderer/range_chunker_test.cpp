/*
 * Copyright 2025 Rive
 */

#include <catch.hpp>
#include "rive/renderer/range_chunker.hpp"
#include <algorithm>

using namespace rive;
using namespace rive::gpu;

TEST_CASE("maxPerDrawCommand==2, odd", "[RangeChunker]")
{
  RangeChunker c(5, 0, 2);
  auto it = c.begin();
  CHECK((*it).first == 0);
  CHECK((*it).count == 2);
  CHECK(it != c.end());
  ++it;
  CHECK((*it).first == 2);
  CHECK((*it).count == 2);
  CHECK(it != c.end());
  ++it;
  CHECK((*it).first == 4);
  CHECK((*it).count == 1);
  CHECK(it != c.end());
  for (size_t i = 0; i < 10; ++i)
  {
    ++it;
    CHECK((*it).first == 5);
    CHECK((*it).count == 0);
    CHECK(it == c.end());
  }
}

TEST_CASE("maxPerDrawCommand==2, even", "[RangeChunker]")
{
  RangeChunker c(6, 0, 2);
  auto it = c.begin();
  CHECK((*it).first == 0);
  CHECK((*it).count == 2);
  CHECK(it != c.end());
  ++it;
  CHECK((*it).first == 2);
  CHECK((*it).count == 2);
  CHECK(it != c.end());
  ++it;
  CHECK((*it).first == 4);
  CHECK((*it).count == 2);
  CHECK(it != c.end());
  for (size_t i = 0; i < 10; ++i)
  {
    ++it;
    CHECK((*it).first == 6);
    CHECK((*it).count == 0);
    CHECK(it == c.end());
  }
}

TEST_CASE("maxPerDrawCommand==0xffffffff", "[RangeChunker]")
{
  RangeChunker c(5, 0, 0xffffffff);
  auto it = c.begin();
  CHECK((*it).first == 0);
  CHECK((*it).count == 5);
  CHECK(it != c.end());
  for (size_t i = 0; i < 10; ++i)
  {
    ++it;
    CHECK((*it).first == 5);
    CHECK((*it).count == 0);
    CHECK(it == c.end());
  }
}

TEST_CASE("maxPerDrawCommand==0", "[RangeChunker]")
{
  RangeChunker c(5, 0, 0);
  auto it = c.begin();
  for (size_t i = 0; i < 10; ++i)
  {
    CHECK(it != c.end());
    CHECK((*it).first == 0);
    CHECK((*it).count == 0);
    ++it;
  }
}

TEST_CASE("empty", "[RangeChunker]")
{
  RangeChunker c(0, 0, 5);
  auto it = c.begin();
  for (size_t i = 0; i < 10; ++i)
  {
    CHECK(it == c.end());
    ++it;
  }
}

TEST_CASE("empty,maxPerDrawCommand==0", "[RangeChunker]")
{
  RangeChunker c(0, 0, 0);
  auto it = c.begin();
  for (size_t i = 0; i < 10; ++i)
  {
    CHECK(it == c.end());
    ++it;
  }
}

// DSIndexRangeChunker turns a depthStencil batch into draw arguments. The four
// index patterns differ in index count, patch stride, reps per buffer, and
// flags, so every case runs all of them: transposing any one inside the
// iterator would still compile.
constexpr static DrawType DSDrawTypes[] = {
    DrawType::stencilMidpointFans,
    DrawType::stencilOuterCubics,
    DrawType::depthStrokes,
    DrawType::depthAAStrokes,
};

static uint32_t patchIndexCount(DrawType drawType)
{
  switch (drawType)
  {
    case DrawType::stencilMidpointFans:
    case DrawType::stencilDynamicMidpointFans:
      return DSMidpointFanFillPatchIndexCount;
    case DrawType::stencilOuterCubics:
    case DrawType::stencilDynamicOuterCubics:
      return DSOuterCubicFillPatchIndexCount;
    case DrawType::depthStrokes:
      return DSStrokePatchIndexCount;
    case DrawType::depthAAStrokes:
      return DSAAStrokePatchIndexCount;
    default:
      RIVE_UNREACHABLE();
  }
}

static int32_t patchFlags(DrawType drawType)
{
  switch (drawType)
  {
    case DrawType::stencilOuterCubics:
    case DrawType::stencilDynamicOuterCubics:
      return DSVertexFlag_OuterCubicFill;
    case DrawType::depthAAStrokes:
      return DSVertexFlag_AAPolarStroke;
    default:
      return 0;
  }
}

// Strokes walk their run of the virtual patch buffer, which has an entry per
// patch per pass. (depthAAStrokes draw twice: depth only, then color.)
static uint32_t drawnPatchCount(DrawType drawType, uint32_t patchCount)
{
  return drawType == DrawType::depthAAStrokes ? patchCount * 2 : patchCount;
}

// RenderContext premultiplies a depthStencil batch's baseElement into a base
// vertex with the patch flags ORed in, and fills in the index pattern. Fills
// draw their patches directly; by draw time a stroke's range is its run of
// "virtual" patches (one per patch per pass), so it looks the same.
static DrawBatch makeBatch(
    DrawType drawType,
    uint32_t patchCount,
    uint32_t firstPatch)
{
  DrawBatch batch(
      drawType,
      ShaderMiscFlags::none,
      DrawContents::none,
      drawnPatchCount(drawType, patchCount),
      (firstPatch << dsPatchStrideLog2(drawType)) | patchFlags(drawType),
      BlendMode::srcOver,
      ImageSampler::LinearClamp(),
      BarrierFlags::none);
  batch.indexCountPerInstance = patchIndexCount(drawType);
  return batch;
}

// A range appended to a batch's list (base premultiplied like makeBatch()).
static DrawBatch::ElementRange makeRange(
    DrawType drawType,
    uint32_t patchCount,
    uint32_t firstPatch)
{
  return DrawBatch::ElementRange(
      patchCount,
      (firstPatch << dsPatchStrideLog2(drawType)) | patchFlags(drawType));
}

static void checkDraw(
    DSIndexRangeChunker::Draw draw,
    DrawType drawType,
    uint32_t patchCount,
    uint32_t firstPatch,
    int32_t extraFlags = 0)
{
  CHECK(draw.indexCount == patchCount * patchIndexCount(drawType));
  CHECK(
      draw.baseVertex ==
      ((static_cast<int32_t>(firstPatch) << dsPatchStrideLog2(drawType)) |
       patchFlags(drawType) | extraFlags));
}

// Checks every draw of a batch of 'patchCount' patches starting at
// 'firstPatch'.
static void checkDraws(
    const DSIndexRangeChunker& iter,
    DrawType drawType,
    uint32_t patchCount,
    uint32_t firstPatch,
    int32_t extraFlags = 0)
{
  const uint32_t maxReps = dsPatchMaxReps(drawType);
  const uint32_t drawnCount = drawnPatchCount(drawType, patchCount);
  auto it = iter.begin();
  for (uint32_t done = 0; done < drawnCount; done += maxReps)
  {
    REQUIRE(it != iter.end());
    // Each draw restarts at the buffer's first patch; only baseVertex
    // advances.
    checkDraw(
        *it,
        drawType,
        std::min(maxReps, drawnCount - done),
        firstPatch + done,
        extraFlags);
    ++it;
  }
  CHECK(it == iter.end());
}

TEST_CASE("one chunk", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const DrawBatch batch = makeBatch(drawType, 3, 7);
    checkDraws(DSIndexRangeChunker::drawsForBatch(batch), drawType, 3, 7);
  }
}

TEST_CASE("base patch zero", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const DrawBatch batch = makeBatch(drawType, 1, 0);
    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    REQUIRE(iter.begin() != iter.end());
    // Nothing but flags, so a midpoint fan draws from 0.
    CHECK((*iter.begin()).baseVertex == patchFlags(drawType));
    checkDraws(iter, drawType, 1, 0);
  }
}

TEST_CASE("empty batch", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const DrawBatch batch = makeBatch(drawType, 0, 4);
    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    CHECK(iter.begin() == iter.end());
  }
}

TEST_CASE("splits at the buffer's rep count", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const uint32_t maxReps = dsPatchMaxReps(drawType);
    const DrawBatch batch = makeBatch(drawType, maxReps + 13, 0);
    checkDraws(
        DSIndexRangeChunker::drawsForBatch(batch),
        drawType,
        maxReps + 13,
        0);
  }
}

TEST_CASE(
    "dynamic fill subpasses carry the color-write flag",
    "[DSIndexRangeChunker]")
{
  for (DrawType drawType :
       {DrawType::stencilDynamicMidpointFans,
        DrawType::stencilDynamicOuterCubics})
  {
    const DrawBatch batch = makeBatch(drawType, 2, 5);
    for (bool colorWriteEnabled : {false, true})
    {
      PipelineState subpassState;
      subpassState.colorWriteEnabled = colorWriteEnabled;
      auto iter = DSIndexRangeChunker::drawsForDynamicFillSubpass(
          batch,
          batch.elementRange,
          subpassState);
      auto it = iter.begin();
      REQUIRE(it != iter.end());
      checkDraw(
          *it,
          drawType,
          2,
          5,
          colorWriteEnabled ? 0 : DSVertexFlag_DisableColorWrite);
      ++it;
      CHECK(it == iter.end());
    }
  }
}

TEST_CASE("AA strokes draw every pass in one run", "[DSIndexRangeChunker]")
{
  // The pass flags live in the virtual patch buffer, not the draw.
  const DrawBatch batch = makeBatch(DrawType::depthAAStrokes, 4, 2);
  auto iter = DSIndexRangeChunker::drawsForBatch(batch);
  auto it = iter.begin();
  REQUIRE(it != iter.end());
  checkDraw(*it, DrawType::depthAAStrokes, 8, 2);
  CHECK(((*it).baseVertex & DSVertexFlag_StrokeDepthPass) == 0);
  CHECK(((*it).baseVertex & DSVertexFlag_DisableColorWrite) == 0);
  ++it;
  CHECK(it == iter.end());
}

TEST_CASE(
    "range-for covers every patch exactly once per pass",
    "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const uint32_t maxReps = dsPatchMaxReps(drawType);
    const uint32_t patchCount = maxReps * 2 + 3;
    const uint32_t drawnCount = drawnPatchCount(drawType, patchCount);
    uint32_t totalIndices = 0;
    uint32_t draws = 0;
    // The chunker points into the batch, so the batch must outlive the loop.
    const DrawBatch batch = makeBatch(drawType, patchCount, 0);
    for (auto [indexCount, baseVertex] :
         DSIndexRangeChunker::drawsForBatch(batch))
    {
      totalIndices += indexCount;
      ++draws;
    }
    CHECK(draws == (drawnCount + maxReps - 1) / maxReps);
    CHECK(totalIndices == drawnCount * patchIndexCount(drawType));
  }
}

TEST_CASE(
    "a dynamic fill subpass stops at the next range",
    "[DSIndexRangeChunker]")
{
  for (DrawType drawType :
       {DrawType::stencilDynamicMidpointFans,
        DrawType::stencilDynamicOuterCubics})
  {
    const uint32_t maxReps = dsPatchMaxReps(drawType);
    DrawBatch batch = makeBatch(drawType, 2, 5);
    DrawBatch::ElementRange second = makeRange(drawType, maxReps + 1, 40);
    batch.elementRange.next = &second;
    const PipelineState subpassState;

    auto first = DSIndexRangeChunker::drawsForDynamicFillSubpass(
        batch,
        batch.elementRange,
        subpassState);
    auto it = first.begin();
    REQUIRE(it != first.end());
    checkDraw(*it, drawType, 2, 5);
    ++it;
    CHECK(it == first.end());

    auto rest = DSIndexRangeChunker::drawsForDynamicFillSubpass(
        batch,
        second,
        subpassState);
    it = rest.begin();
    REQUIRE(it != rest.end());
    checkDraw(*it, drawType, maxReps, 40);
    ++it;
    REQUIRE(it != rest.end());
    checkDraw(*it, drawType, 1, 40 + maxReps);
    ++it;
    CHECK(it == rest.end());
  }
}
