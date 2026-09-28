/*
 * Copyright 2025 Rive
 */

#include <catch.hpp>
#include "rive/renderer/range_chunker.hpp"
#include "shaders/constants.glsl"

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

// DSIndexRangeChunker turns each chunk into draw arguments. The two patch types
// differ in index count, patch stride, and reps per buffer, so every case runs
// both: transposing any of the three inside the iterator would still compile.
static void checkDraw(DSIndexRangeChunker::Draw draw,
                      bool outerCubic,
                      uint32_t patchCount,
                      uint32_t firstPatch,
                      int32_t extraFlags = 0)
{
    CHECK(draw.indexCount == patchCount * dsFillPatchIndexCount(outerCubic));
    CHECK(draw.baseVertex ==
          ((static_cast<int32_t>(firstPatch)
            << DS_PATCH_STRIDE_LOG2(outerCubic)) |
           extraFlags | (outerCubic ? VERTEX_FLAG_OUTER_CUBIC : 0)));
}

static DrawType dsFillDrawType(bool outerCubic)
{
    return outerCubic ? DrawType::stencilOuterCubics
                      : DrawType::stencilMidpointFans;
}

TEST_CASE("one chunk", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        DSIndexRangeChunker iter(dsFillDrawType(outerCubic), 3, 7);
        auto it = iter.begin();
        REQUIRE(it != iter.end());
        checkDraw(*it, outerCubic, 3, 7);
        ++it;
        CHECK(it == iter.end());
    }
}

TEST_CASE("base patch zero", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        DSIndexRangeChunker iter(dsFillDrawType(outerCubic), 1, 0);
        auto it = iter.begin();
        REQUIRE(it != iter.end());
        // Nothing but the patch-type flag, so a midpoint fan draws from 0.
        CHECK((*it).baseVertex == (outerCubic ? VERTEX_FLAG_OUTER_CUBIC : 0));
        checkDraw(*it, outerCubic, 1, 0);
    }
}

TEST_CASE("empty batch", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        DSIndexRangeChunker iter(dsFillDrawType(outerCubic), 0, 4);
        CHECK(iter.begin() == iter.end());
    }
}

TEST_CASE("splits at the buffer's rep count", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        const uint32_t maxReps = dsFillPatchMaxReps(outerCubic);
        const uint32_t tail = 13;
        DSIndexRangeChunker iter(dsFillDrawType(outerCubic), maxReps + tail, 0);
        auto it = iter.begin();
        REQUIRE(it != iter.end());
        checkDraw(*it, outerCubic, maxReps, 0);
        ++it;
        REQUIRE(it != iter.end());
        // The second draw restarts at the buffer's first patch; only
        // baseVertex advances.
        checkDraw(*it, outerCubic, tail, maxReps);
        ++it;
        CHECK(it == iter.end());
    }
}

TEST_CASE("carries the color-write flag", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        DSIndexRangeChunker iter(dsFillDrawType(outerCubic),
                                 2,
                                 5,
                                 VERTEX_FLAG_DISABLE_COLOR_WRITE);
        auto it = iter.begin();
        REQUIRE(it != iter.end());
        checkDraw(*it, outerCubic, 2, 5, VERTEX_FLAG_DISABLE_COLOR_WRITE);
        CHECK(((*it).baseVertex & VERTEX_FLAG_DISABLE_COLOR_WRITE) != 0);
    }
}

TEST_CASE("range-for covers every patch exactly once", "[DSIndexRangeChunker]")
{
    for (bool outerCubic : {false, true})
    {
        const uint32_t maxReps = dsFillPatchMaxReps(outerCubic);
        const uint32_t patchCount = maxReps * 2 + 3;
        uint32_t totalIndices = 0;
        uint32_t draws = 0;
        for (auto [indexCount, baseVertex] :
             DSIndexRangeChunker(dsFillDrawType(outerCubic), patchCount, 0))
        {
            totalIndices += indexCount;
            ++draws;
        }
        CHECK(draws == 3);
        CHECK(totalIndices == patchCount * dsFillPatchIndexCount(outerCubic));
    }
}
