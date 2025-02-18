/*
 * Copyright 2024 Rive
 */

#include "rive/math/math_types.hpp"
#include "rive/renderer/gpu.hpp"
#include "shaders/constants.glsl"
#include <catch.hpp>

namespace rive
{
TEST_CASE("find_transformed_area", "[gpu]")
{
    AABB unitSquare{0, 0, 1, 1};
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D()) == 1);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(2, 2)) == 4);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(2, 1)) == 2);
    CHECK(gpu::find_transformed_area(unitSquare, Mat2D::fromScale(0, 1)) == 0);
    CHECK(gpu::find_transformed_area(unitSquare,
                                     Mat2D::fromRotation(math::PI / 4)) ==
          Approx(1.f));
    CHECK(gpu::find_transformed_area(
              unitSquare,
              Mat2D::fromRotation(math::PI / 8).scale({2, 2})) == Approx(4.f));
    CHECK(gpu::find_transformed_area(
              unitSquare,
              Mat2D::fromRotation(math::PI / 16).scale({2, 1})) == Approx(2.f));
    CHECK(
        gpu::find_transformed_area(unitSquare, {1, .87f, 8, 8 * .87f, 0, 0}) ==
        Approx(0.f).margin(math::EPSILON));
}

TEST_CASE("gaussian_integral_table", "[gpu]")
{
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; i += 4)
    {
        float4 f32s = gpu::cast_f16_to_f32(
            simd::load<uint16_t, 4>(gpu::g_gaussianIntegralTableF16 + i));
        simd::store(gaussianTable + i, f32s);
    };

    CHECK(gaussianTable[0] >= 0);
    CHECK(gaussianTable[0] <= expf(-.5f * FEATHER_TEXTURE_STDDEVS));
    CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] <= 1);
    CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] >=
          1 - expf(-.5f * FEATHER_TEXTURE_STDDEVS));
    if (gpu::GAUSSIAN_TABLE_SIZE & 1)
    {
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] == .5f);
    }
    else
    {
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] <= .5f);
        CHECK(gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] >= .5f);
        CHECK((gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] +
               gaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2]) /
                  2 ==
              Approx(.5f).margin(1e-3f));
    }
    for (int i = 1; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        CHECK(gaussianTable[i - 1] <= gaussianTable[i]);
    }
    for (int i = 0; i < (gpu::GAUSSIAN_TABLE_SIZE + 1) / 2; ++i)
    {
        CHECK(gaussianTable[i] +
                  gaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1 - i] ==
              Approx(1).margin(1e-3f));
    }
}

TEST_CASE("inverse_gaussian_integral_table", "[gpu]")
{
    float gaussianTable[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; i += 4)
    {
        float4 f32s = gpu::cast_f16_to_f32(
            simd::load<uint16_t, 4>(gpu::g_gaussianIntegralTableF16 + i));
        simd::store(gaussianTable + i, f32s);
    };

    auto checkTable = [gaussianTable](const float(
                          &inverseGaussianTable)[gpu::GAUSSIAN_TABLE_SIZE]) {
        CHECK(inverseGaussianTable[0] == 0);
        CHECK(inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1] == 1);
        if (gpu::GAUSSIAN_TABLE_SIZE & 1)
        {
            CHECK(inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2] == .5f);
        }
        else
        {
            CHECK((inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2 - 1] +
                   inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE / 2]) /
                      2 ==
                  Approx(.5f).margin(1e-4f));
        }
        for (int i = 1; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
        {
            CHECK(inverseGaussianTable[i - 1] <= inverseGaussianTable[i]);
        }
        for (int i = 0; i < (gpu::GAUSSIAN_TABLE_SIZE + 1) / 2 - 4; ++i)
        {
            CHECK(inverseGaussianTable[i] +
                      inverseGaussianTable[gpu::GAUSSIAN_TABLE_SIZE - 1 - i] ==
                  Approx(1).margin(i > 100 ? 5e-4f
                                   : i > 4 ? 1e-3f
                                           : 1e-2f));
        }

        // Check that the inverse table is actually an inverse of the gaussian
        // integral.
        float M = 21;
        for (float x = 0; x <= 1; x += 1.f / (gpu::GAUSSIAN_TABLE_SIZE * M))
        {
            float y = gpu::function_table_lookup(x, gaussianTable);
            // The inverse table loses precision at the inner and outer cells.
            float margin = x > .125f && x < .875f ? 1.f / 512
                           : x > .04f && x < .96f ? 1.f / 256
                           : x > .02f && x < .98f ? 1.f / 128
                                                  : 1.f / 95;
            CHECK(gpu::function_table_lookup(y, inverseGaussianTable) ==
                  Approx(x).margin(margin));
            CHECK(gpu::inverse_gaussian_integral(y) ==
                  Approx(x).margin(margin));
        }

        // Check inverse_gaussian_integral edge cases.
        CHECK(gpu::function_table_lookup(-1, inverseGaussianTable) == 0);
        CHECK(gpu::inverse_gaussian_integral(-1) == 0);

        CHECK(gpu::function_table_lookup(2, inverseGaussianTable) == 1);
        CHECK(gpu::inverse_gaussian_integral(2) == 1);

        CHECK(
            gpu::function_table_lookup(-std::numeric_limits<float>::infinity(),
                                       inverseGaussianTable) == 0);
        CHECK(gpu::inverse_gaussian_integral(
                  -std::numeric_limits<float>::infinity()) == 0);

        CHECK(gpu::function_table_lookup(std::numeric_limits<float>::infinity(),
                                         inverseGaussianTable) == 1);
        CHECK(gpu::inverse_gaussian_integral(
                  std::numeric_limits<float>::infinity()) == 1);

        CHECK(
            gpu::function_table_lookup(std::numeric_limits<float>::quiet_NaN(),
                                       inverseGaussianTable) == 0);
        CHECK(gpu::inverse_gaussian_integral(
                  std::numeric_limits<float>::quiet_NaN()) == 0);
    };
    checkTable(gpu::g_inverseGaussianIntegralTableF32);

    gpu::InverseGaussianIntegralTableF16 inverseGaussianTableF16;
    float inverseGaussianTableFromF16[gpu::GAUSSIAN_TABLE_SIZE];
    for (int i = 0; i < gpu::GAUSSIAN_TABLE_SIZE; ++i)
    {
        inverseGaussianTableFromF16[i] =
            gpu::cast_f16_to_f32(inverseGaussianTableF16.data[i]).x;
    }
    checkTable(inverseGaussianTableFromF16);
}
} // namespace rive
