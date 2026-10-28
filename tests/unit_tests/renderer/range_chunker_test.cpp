/*
 * Copyright 2025 Rive
 */

#include <catch.hpp>
#include "rive/renderer/range_chunker.hpp"
#include <algorithm>
#include <iterator>
#include <vector>

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

// depthAAStrokes draw twice: depth only, then color.
static std::vector<int32_t> passFlags(DrawType drawType)
{
  if (drawType == DrawType::depthAAStrokes)
  {
    return {DSVertexFlag_StrokeDepthPass | DSVertexFlag_DisableColorWrite, 0};
  }
  return {0};
}

// RenderContext premultiplies a depthStencil batch's baseElement into a base
// vertex with the patch flags ORed in, and fills in the index pattern.
static DrawBatch makeBatch(
    DrawType drawType,
    uint32_t patchCount,
    uint32_t firstPatch)
{
  DrawBatch batch(
      drawType,
      ShaderMiscFlags::none,
      DrawContents::none,
      patchCount,
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

TEST_CASE("one chunk", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const DrawBatch batch = makeBatch(drawType, 3, 7);
    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    auto it = iter.begin();
    for (int32_t flags : passFlags(drawType))
    {
      REQUIRE(it != iter.end());
      checkDraw(*it, drawType, 3, 7, flags);
      ++it;
    }
    CHECK(it == iter.end());
  }
}

TEST_CASE("base patch zero", "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSDrawTypes)
  {
    const DrawBatch batch = makeBatch(drawType, 1, 0);
    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    auto it = iter.begin();
    for (int32_t flags : passFlags(drawType))
    {
      REQUIRE(it != iter.end());
      // Nothing but flags, so a midpoint fan draws from 0.
      CHECK((*it).baseVertex == (patchFlags(drawType) | flags));
      checkDraw(*it, drawType, 1, 0, flags);
      ++it;
    }
    CHECK(it == iter.end());
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
    const uint32_t tail = 13;
    const DrawBatch batch = makeBatch(drawType, maxReps + tail, 0);
    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    auto it = iter.begin();
    for (int32_t flags : passFlags(drawType))
    {
      REQUIRE(it != iter.end());
      checkDraw(*it, drawType, maxReps, 0, flags);
      ++it;
      REQUIRE(it != iter.end());
      // The second draw restarts at the buffer's first patch; only
      // baseVertex advances.
      checkDraw(*it, drawType, tail, maxReps, flags);
      ++it;
    }
    CHECK(it == iter.end());
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

TEST_CASE("AA strokes draw depth before color", "[DSIndexRangeChunker]")
{
  const DrawBatch batch = makeBatch(DrawType::depthAAStrokes, 4, 2);
  auto iter = DSIndexRangeChunker::drawsForBatch(batch);
  auto it = iter.begin();
  REQUIRE(it != iter.end());
  CHECK(((*it).baseVertex & DSVertexFlag_StrokeDepthPass) != 0);
  CHECK(((*it).baseVertex & DSVertexFlag_DisableColorWrite) != 0);
  ++it;
  REQUIRE(it != iter.end());
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
    const size_t passCount = passFlags(drawType).size();
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
    CHECK(draws == 3 * passCount);
    CHECK(totalIndices == passCount * patchCount * patchIndexCount(drawType));
  }
}

// Range lists only exist for strokes, hairlines, and dynamic fills.
constexpr static DrawType DSListDrawTypes[] = {
    DrawType::depthStrokes,
    DrawType::depthAAStrokes,
};

TEST_CASE(
    "each range runs every pass before the next range",
    "[DSIndexRangeChunker]")
{
  for (DrawType drawType : DSListDrawTypes)
  {
    const uint32_t maxReps = dsPatchMaxReps(drawType);
    // {patchCount, firstPatch}: empty ranges at the front, middle, and end,
    // and one range that spans two chunks.
    const uint32_t spec[][2] = {
        {0, 1},
        {3, 10},
        {0, 50},
        {maxReps + 5, 100},
        {2, 7},
        {0, 9},
    };
    DrawBatch batch = makeBatch(drawType, spec[0][0], spec[0][1]);
    std::vector<DrawBatch::ElementRange> ranges;
    ranges.reserve(std::size(spec) - 1);
    DrawBatch::ElementRange* tail = &batch.elementRange;
    for (size_t i = 1; i < std::size(spec); ++i)
    {
      ranges.push_back(makeRange(drawType, spec[i][0], spec[i][1]));
      tail->next = &ranges.back();
      tail = tail->next;
    }

    auto iter = DSIndexRangeChunker::drawsForBatch(batch);
    auto it = iter.begin();
    for (const auto& [patchCount, firstPatch] : spec)
    {
      for (int32_t flags : passFlags(drawType))
      {
        for (uint32_t done = 0; done < patchCount;)
        {
          const uint32_t chunk = std::min(patchCount - done, maxReps);
          REQUIRE(it != iter.end());
          checkDraw(*it, drawType, chunk, firstPatch + done, flags);
          ++it;
          done += chunk;
        }
      }
    }
    CHECK(it == iter.end());
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
