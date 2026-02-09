/*
 * Copyright 2014 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from
 * skia:b4171f5ba83048039097bbc664eaa076190f6239@tests/GpuRectanizerTest.cpp
 *
 * Copyright 2024 Rive
 */

#include "rive/renderer/sk_rectanizer_skyline.hpp"

#include "rive/math/aabb.hpp"
#include "common/rand.hpp"
#include <catch.hpp>

struct GrContextOptions;

static const int kWidth = 1024;
static const int kHeight = 1024;

struct SkISize
{
    int32_t fWidth = 0;
    int32_t fHeight = 0;
};

// Basic test of a Rectanizer-derived class' functionality
static void test_rectanizer_basic(rive::RectanizerSkyline* rectanizer)
{
    CHECK(kWidth == rectanizer->width());
    CHECK(kHeight == rectanizer->height());

    int16_t x, y;

    CHECK(rectanizer->addRect(50, 50, &x, &y));
    CHECK(rectanizer->percentFull() > 0.0f);
    rectanizer->reset();
    CHECK(rectanizer->percentFull() == 0.0f);
}

static void test_rectanizer_inserts(rive::RectanizerSkyline* rectanizer,
                                    const std::vector<SkISize>& rects)
{
    std::vector<rive::IAABB> boxes;
    int i;
    float percentFull = 0;
    for (i = 0; i < rects.size(); ++i)
    {
        int16_t x, y;
        if (!rectanizer->addRect(rects[i].fWidth, rects[i].fHeight, &x, &y))
        {
            rectanizer->reset();
            boxes.clear();
            percentFull = 0;
            REQUIRE(
                rectanizer->addRect(rects[i].fWidth, rects[i].fHeight, &x, &y));
        }
        CHECK(x >= 0);
        CHECK(y >= 0);
        CHECK(x + rects[i].fWidth <= kWidth);
        CHECK(y + rects[i].fHeight <= kHeight);

        rive::IAABB box = {x, y, rects[i].fWidth, rects[i].fHeight};
        for (rive::IAABB oldbox : boxes)
        {
            CHECK(box.intersect(oldbox).empty());
        }
        boxes.push_back(box);

        CHECK(rectanizer->percentFull() > percentFull);
        percentFull = rectanizer->percentFull();
        // printf("\n***%d %f\n", i, percentFull);
    }
}

static void test_skyline(const std::vector<SkISize>& rects)
{
    rive::RectanizerSkyline skylineRectanizer(kWidth, kHeight);

    test_rectanizer_basic(&skylineRectanizer);
    test_rectanizer_inserts(&skylineRectanizer, rects);
}

#if 0
static void test_pow2(skiatest::Reporter* reporter,
                      const SkTDArray<SkISize>& rects)
{
    RectanizerPow2 pow2Rectanizer(kWidth, kHeight);

    test_rectanizer_basic(reporter, &pow2Rectanizer);
    test_rectanizer_inserts(reporter, &pow2Rectanizer, rects);
}
#endif

TEST_CASE("RectanizerSkyline", "basic_tests")
{
    std::vector<SkISize> rects;
    Rand rand;

    for (int i = 0; i < 50; i++)
    {
        rects.push_back(
            {static_cast<int32_t>(powf(rand.f32(), 2.5f) * .75f * kWidth + 1),
             static_cast<int32_t>(powf(rand.f32(), 2.5f) * .75f * kHeight +
                                  1)});
    }

    test_skyline(rects);
#if 0
    test_pow2(rects);
#endif
}
