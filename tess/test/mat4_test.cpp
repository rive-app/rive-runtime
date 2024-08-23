#include <catch.hpp>
#include "rive/math/mat4.hpp"
#include "rive/math/mat2d.hpp"

TEST_CASE("Mat2D to Mat4 works", "[mat4]")
{
    rive::Mat2D matrix2D(0.1f, 0.2f, 0.0f, 2.0f, 22.0f, 33.0f);
    rive::Mat4 matrix4x4 = matrix2D;
    REQUIRE(matrix4x4[0] == 0.1f);
    REQUIRE(matrix4x4[1] == 0.2f);
    REQUIRE(matrix4x4[4] == 0.0f);
    REQUIRE(matrix4x4[5] == 2.0f);
    REQUIRE(matrix4x4[12] == 22.0f);
    REQUIRE(matrix4x4[13] == 33.0f);
}

TEST_CASE("Mat4 times Mat2 works", "[mat4]")
{
    rive::Mat2D matrix2D(0.1f, 0.2f, 0.0f, 2.0f, 22.0f, 33.0f);
    rive::Mat4 identity4x4;
    rive::Mat4 matrix4x4 = identity4x4 * matrix2D;
    REQUIRE(matrix4x4[0] == 0.1f);
    REQUIRE(matrix4x4[1] == 0.2f);
    REQUIRE(matrix4x4[4] == 0.0f);
    REQUIRE(matrix4x4[5] == 2.0f);
    REQUIRE(matrix4x4[12] == 22.0f);
    REQUIRE(matrix4x4[13] == 33.0f);
}

TEST_CASE("Mat4 times Mat4 works", "[mat4]")
{
    rive::Mat4 a(
        // clang-format off
        5.0f, 7.0f, 9.0f, 10.0f,
        2.0f, 3.0f, 3.0f, 8.0f,
        8.0f, 10.0f, 2.0f, 3.0f, 
        3.0f, 3.0f, 4.0f, 8.0f
        // clang-format on
    );
    rive::Mat4 b(
        // clang-format off
        3.0f, 10.0f, 12.0f, 18.0f,
        12.0f, 1.0f, 4.0f, 9.0f,
        9.0f, 10.0f, 12.0f, 2.0f, 
        3.0f, 12.0f, 4.0f, 10.0f
        // clang-format on
    );

    auto result = b * a;
    REQUIRE(result[0] == 210.0f);
    REQUIRE(result[1] == 267.0f);
    REQUIRE(result[2] == 236.0f);
    REQUIRE(result[3] == 271.0f);

    REQUIRE(result[4] == 93.0f);
    REQUIRE(result[5] == 149.0f);
    REQUIRE(result[6] == 104.0f);
    REQUIRE(result[7] == 149.0f);

    REQUIRE(result[8] == 171.0f);
    REQUIRE(result[9] == 146.0f);
    REQUIRE(result[10] == 172.0f);
    REQUIRE(result[11] == 268.0f);

    REQUIRE(result[12] == 105.0f);
    REQUIRE(result[13] == 169.0f);
    REQUIRE(result[14] == 128.0f);
    REQUIRE(result[15] == 169.0f);
}