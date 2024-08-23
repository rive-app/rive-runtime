#include <catch.hpp>
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"

namespace rive
{
TEST_CASE("IAABB_join", "[IAABB]")
{
    CHECK(IAABB{1, -2, 99, 101}.join({0, 0, 100, 100}) == IAABB{0, -2, 100, 101});
    CHECK(IAABB{1, -2, 99, 101}.join({2, -3, 98, 103}) == IAABB{1, -3, 99, 103});
}

TEST_CASE("IAABB_intersect", "[IAABB]")
{
    CHECK(IAABB{1, -2, 99, 101}.intersect({0, 0, 100, 100}) == IAABB{1, 0, 99, 100});
    CHECK(IAABB{1, -2, 99, 101}.intersect({2, -3, 98, 103}) == IAABB{2, -2, 98, 101});
}

TEST_CASE("IAABB_empty", "[IAABB]")
{
    CHECK(IAABB{0, 0, 0, 0}.empty());
    CHECK(IAABB{0, 0, 0, 1}.empty());
    CHECK(IAABB{0, 0, 1, 0}.empty());
    CHECK(!IAABB{0, 0, 1, 1}.empty());
    CHECK(IAABB{0, 0, -1, -1}.empty());
    CHECK(IAABB{std::numeric_limits<int32_t>::max(),
                std::numeric_limits<int32_t>::max(),
                std::numeric_limits<int32_t>::min(),
                std::numeric_limits<int32_t>::min()}
              .empty());
}

TEST_CASE("isEmptyOrNaN", "[AABB]")
{
    auto inf = std::numeric_limits<float>::infinity();
    auto nan = std::numeric_limits<float>::quiet_NaN();
    CHECK(!AABB{0, 0, 1, 1}.isEmptyOrNaN());
    CHECK(!AABB{-inf, -inf, inf, inf}.isEmptyOrNaN());
    CHECK(AABB{0, 0, 0, 0}.isEmptyOrNaN());
    CHECK(AABB{0, 0, -1, -2}.isEmptyOrNaN());
    CHECK(AABB{inf, inf, -inf, -inf}.isEmptyOrNaN());
    CHECK(AABB{inf, -inf, -inf, inf}.isEmptyOrNaN());
    CHECK(AABB{-inf, inf, inf, -inf}.isEmptyOrNaN());
    CHECK(AABB{nan, 0, 10, 10}.isEmptyOrNaN());
    CHECK(AABB{0, nan, 10, 10}.isEmptyOrNaN());
    CHECK(AABB{0, 0, nan, 10}.isEmptyOrNaN());
    CHECK(AABB{0, 0, 10, nan}.isEmptyOrNaN());
    CHECK(AABB{nan, nan, 10, 10}.isEmptyOrNaN());
    CHECK(AABB{nan, nan, nan, 10}.isEmptyOrNaN());
    CHECK(AABB{nan, nan, nan, nan}.isEmptyOrNaN());
}

TEST_CASE("AABB contains", "[AABB]")
{
    CHECK(AABB{0, 0, 100, 100}.contains(Vec2D(20, 20)));
    CHECK(AABB{0, 0, 100, 100}.contains(Vec2D(0, 0)));
    CHECK(AABB{0, 0, 100, 100}.contains(Vec2D(100, 100)));
    CHECK(!AABB{0, 0, 100, 100}.contains(Vec2D(200, 200)));
    CHECK(!AABB{0, 0, 100, 100}.contains(Vec2D(-200, -200)));
    auto leftBoundary = 0.f;
    auto rightBoundary = 100.f;
    CHECK(!AABB{leftBoundary, 0, rightBoundary, 100.0}.contains(
        Vec2D(leftBoundary - std::numeric_limits<float>::epsilon(), 50)));
    CHECK(!AABB{leftBoundary, 0, rightBoundary, 100.0}.contains(
        Vec2D(rightBoundary + rightBoundary * std::numeric_limits<float>::epsilon(), 50)));
}
} // namespace rive
