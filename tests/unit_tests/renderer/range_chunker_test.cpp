/*
 * Copyright 2025 Rive
 */

#include <catch.hpp>
#include "rive/renderer/range_chunker.hpp"
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
            return DSMidpointFanFillPatchIndexCount;
        case DrawType::stencilOuterCubics:
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
            return DSVertexFlag_OuterCubicFill;
        case DrawType::depthAAStrokes:
            return DSVertexFlag_AAStroke;
        default:
            return 0;
    }
}

// depthAAStrokes draw twice: depth only, then color.
static std::vector<int32_t> passFlags(DrawType drawType)
{
    if (drawType == DrawType::depthAAStrokes)
    {
        return {DSVertexFlag_StrokeDepthPass | DSVertexFlag_DisableColorWrite,
                0};
    }
    return {0};
}

// RenderContext premultiplies a depthStencil batch's baseElement into a base
// vertex with the patch flags ORed in, and fills in the index pattern.
static DrawBatch makeBatch(DrawType drawType,
                           uint32_t patchCount,
                           uint32_t firstPatch)
{
    DrawBatch batch(drawType,
                    ShaderMiscFlags::none,
                    DrawContents::none,
                    patchCount,
                    (firstPatch << dsPatchStrideLog2(drawType)) |
                        patchFlags(drawType),
                    BlendMode::srcOver,
                    ImageSampler::LinearClamp(),
                    BarrierFlags::none);
    batch.indexCountPerInstance = patchIndexCount(drawType);
    return batch;
}

static void checkDraw(DSIndexRangeChunker::Draw draw,
                      DrawType drawType,
                      uint32_t patchCount,
                      uint32_t firstPatch,
                      int32_t extraFlags = 0)
{
    CHECK(draw.indexCount == patchCount * patchIndexCount(drawType));
    CHECK(draw.baseVertex ==
          ((static_cast<int32_t>(firstPatch) << dsPatchStrideLog2(drawType)) |
           patchFlags(drawType) | extraFlags));
}

TEST_CASE("one chunk", "[DSIndexRangeChunker]")
{
    for (DrawType drawType : DSDrawTypes)
    {
        DSIndexRangeChunker iter(makeBatch(drawType, 3, 7));
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
        DSIndexRangeChunker iter(makeBatch(drawType, 1, 0));
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
        DSIndexRangeChunker iter(makeBatch(drawType, 0, 4));
        CHECK(iter.begin() == iter.end());
    }
}

TEST_CASE("splits at the buffer's rep count", "[DSIndexRangeChunker]")
{
    for (DrawType drawType : DSDrawTypes)
    {
        const uint32_t maxReps = dsPatchMaxReps(drawType);
        const uint32_t tail = 13;
        DSIndexRangeChunker iter(makeBatch(drawType, maxReps + tail, 0));
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

TEST_CASE("carries the color-write flag", "[DSIndexRangeChunker]")
{
    for (DrawType drawType : DSDrawTypes)
    {
        DSIndexRangeChunker iter(makeBatch(drawType, 2, 5),
                                 DSVertexFlag_DisableColorWrite);
        auto it = iter.begin();
        for (int32_t flags : passFlags(drawType))
        {
            REQUIRE(it != iter.end());
            checkDraw(*it,
                      drawType,
                      2,
                      5,
                      flags | DSVertexFlag_DisableColorWrite);
            CHECK(((*it).baseVertex & DSVertexFlag_DisableColorWrite) != 0);
            ++it;
        }
        CHECK(it == iter.end());
    }
}

TEST_CASE("AA strokes draw depth before color", "[DSIndexRangeChunker]")
{
    DSIndexRangeChunker iter(makeBatch(DrawType::depthAAStrokes, 4, 2));
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

TEST_CASE("range-for covers every patch exactly once per pass",
          "[DSIndexRangeChunker]")
{
    for (DrawType drawType : DSDrawTypes)
    {
        const uint32_t maxReps = dsPatchMaxReps(drawType);
        const uint32_t patchCount = maxReps * 2 + 3;
        const size_t passCount = passFlags(drawType).size();
        uint32_t totalIndices = 0;
        uint32_t draws = 0;
        for (auto [indexCount, baseVertex] :
             DSIndexRangeChunker(makeBatch(drawType, patchCount, 0)))
        {
            totalIndices += indexCount;
            ++draws;
        }
        CHECK(draws == 3 * passCount);
        CHECK(totalIndices ==
              passCount * patchCount * patchIndexCount(drawType));
    }
}
