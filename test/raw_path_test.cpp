/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/math/raw_path.hpp>
#include "no_op_renderer.hpp"

#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("rawpath-basics", "[rawpath]") {
    RawPath path;

    REQUIRE(path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 0, 0});

    path.move({1, 2});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{1, 2, 1, 2});

    path = RawPath();
    REQUIRE(path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 0, 0});

    path.move({1, -2});
    path.line({3, 4});
    path.line({-1, 5});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{-1, -2, 3, 5});
}

TEST_CASE("rawpath-add-helpers", "[rawpath]") {
    RawPath path;

    path.addRect({1, 1, 5, 6});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{1, 1, 5, 6});
    REQUIRE(path.points().size() == 4);
    REQUIRE(path.verbs().size() == 5); // move, line, line, line, close

    path = RawPath();
    path.addOval({0, 0, 3, 6});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 3, 6});
    REQUIRE(path.points().size() == 13);
    REQUIRE(path.verbs().size() == 6); // move, cubic, cubic, cubic, cubic, close

    const Vec2D pts[] = {
        {1, 2},
        {4, 5},
        {3, 2},
        {100, -100},
    };
    constexpr auto size = sizeof(pts) / sizeof(pts[0]);

    for (auto isClosed : {false, true}) {
        path = RawPath();
        path.addPoly({pts, size}, isClosed);
        REQUIRE(path.bounds() == AABB{1, -100, 100, 5});
        REQUIRE(path.points().size() == size);
        REQUIRE(path.verbs().size() == size + isClosed);

        for (size_t i = 0; i < size; ++i) {
            REQUIRE(path.points()[i] == pts[i]);
        }
        REQUIRE(path.verbs()[0] == PathVerb::move);
        for (size_t i = 1; i < size; ++i) {
            REQUIRE(path.verbs()[i] == PathVerb::line);
        }
        if (isClosed) {
            REQUIRE(path.verbs()[size] == PathVerb::close);
        }
    }
}
