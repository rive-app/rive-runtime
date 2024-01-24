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
} // namespace rive
