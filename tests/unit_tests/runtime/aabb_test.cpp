#include <catch.hpp>
#include "rive/math/mat2d.hpp"
#include "rive/math/math_types.hpp"

namespace rive
{
TEST_CASE("IAABB_join", "[IAABB]")
{
    CHECK(IAABB{1, -2, 99, 101}.join(IAABB{0, 0, 100, 100}) ==
          IAABB{0, -2, 100, 101});
    CHECK(IAABB{1, -2, 99, 101}.join(IAABB{2, -3, 98, 103}) ==
          IAABB{1, -3, 99, 103});
}

TEST_CASE("IAABB_intersect", "[IAABB]")
{
    CHECK(IAABB{1, -2, 99, 101}.intersect(IAABB{0, 0, 100, 100}) ==
          IAABB{1, 0, 99, 100});
    CHECK(IAABB{1, -2, 99, 101}.intersect(IAABB{2, -3, 98, 103}) ==
          IAABB{2, -2, 98, 101});
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
    CHECK(!AABB{leftBoundary, 0, rightBoundary, 100.0}.contains(Vec2D(
        rightBoundary + rightBoundary * std::numeric_limits<float>::epsilon(),
        50)));
}

TEST_CASE("IAABB overlaps", "[AABB]")
{
    // Completely contained
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{10, 10, 90, 90}));

    // Coincident
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{0, 0, 100, 100}));

    // One edge out of range
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{-1000, 10, 90, 90}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{10, -1000, 90, 90}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{10, 10, 1000, 90}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{10, 10, 90, 1000}));

    // One edge still in range
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{-1000, -1000, 1000, 90}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{-1000, -1000, 90, 1000}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{-1000, 10, 1000, 1000}));
    CHECK(IAABB{0, 0, 100, 100}.overlaps(IAABB{10, -1000, 1000, 1000}));

    // Disjoint
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{110, 10, 190, 90}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{10, 110, 90, 190}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{-110, 10, -10, 90}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{10, -110, 90, -10}));

    // Abutting, but disjoint
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{-10, 10, 0, 90}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{10, -10, 90, 0}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{100, 10, 190, 90}));
    CHECK(!IAABB{0, 0, 100, 100}.overlaps(IAABB{10, 100, 190, 90}));
}

TEST_CASE("AABB overlaps", "[AABB]")
{
    // Completely contained
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, 10.0f, 90.0f, 90.0f}));

    // Coincident
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{0.0f, 0.0f, 100.0f, 100.0f}));

    // One edge out of range
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-1000.0f, 10.0f, 90.0f, 90.0f}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, -1000.0f, 90.0f, 90.0f}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, 10.0f, 1000.0f, 90.0f}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, 10.0f, 90.0f, 1000.0f}));

    // One edge still in range
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-1000, -1000, 1000, 90}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-1000, -1000, 90, 1000}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-1000.0f, 10.0f, 1000.0f, 1000.0f}));
    CHECK(AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, -1000.0f, 1000.0f, 1000.0f}));

    // Disjoint
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{110.0f, 10.0f, 190.0f, 90.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, 110.0f, 90.0f, 190.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-110.0f, 10.0f, -10.0f, 90.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, -110.0f, 90.0f, -10.0f}));

    // Abuting, but disjoint
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{-10.0f, 10.0f, 0.0f, 90.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, -10.0f, 90.0f, 0.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{100.0f, 10.0f, 190.0f, 90.0f}));
    CHECK(!AABB{0.0f, 0.0f, 100.0f, 100.0f}.overlaps(
        AABB{10.0f, 100.0f, 190.0f, 90.0f}));
}

TEST_CASE("TAABB::makeMaximal", "[AABB]")
{
    auto testType = [](auto valueForType) {
        using T = decltype(valueForType);
        std::ignore = valueForType;

        constexpr auto Min = std::numeric_limits<T>::min();
        constexpr auto Max = std::numeric_limits<T>::max();

        CHECK(TAABB<T>::makeMaximal() == TAABB<T>{Min, Min, Max, Max});
    };

    testType(int16_t{});
    testType(uint16_t{});
    testType(int32_t{});
    testType(uint32_t{});
    testType(int64_t{});
    testType(uint64_t{});
}

TEST_CASE("TAABB::makeMaximallyNegative", "[AABB]")
{
    auto testType = [](auto valueForType) {
        using T = decltype(valueForType);
        std::ignore = valueForType;

        constexpr auto Min = std::numeric_limits<T>::min();
        constexpr auto Max = std::numeric_limits<T>::max();

        CHECK(TAABB<T>::makeMaximallyNegative() ==
              TAABB<T>{Max, Max, Min, Min});
    };

    testType(int16_t{});
    testType(uint16_t{});
    testType(int32_t{});
    testType(uint32_t{});
    testType(int64_t{});
    testType(uint64_t{});
}
} // namespace rive
