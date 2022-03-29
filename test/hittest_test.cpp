/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/math/hit_test.hpp>

#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("hittest-basics", "[hittest]") {
    HitTester tester;
    tester.reset({10, 10, 12, 12});
    tester.move({0, 0});
    tester.line({20, 0});
    tester.line({20, 20});
    tester.line({0, 20});
    tester.close();
    REQUIRE(tester.test());

    IAABB area = {81, 156, 84, 159};
    
    Vec2D pts[] = {
        {29.9785, 32.5261},
        {231.102, 32.5261},
        {231.102, 269.898},
        {29.9785, 269.898},
    };
    tester.reset(area);
    
    tester.move(pts[0]);
    for (int i = 1; i < 4; ++i) {
        tester.line(pts[i]);
    }
    tester.close();
    REQUIRE(tester.test());
}

TEST_CASE("hittest-mesh", "[hittest]") {

    const IAABB area{10, 10, 12, 12};
    
    Vec2D verts[] = {
        {0, 0}, {20, 10}, {0, 20},
    };
    uint16_t indices[] = {
        0, 1, 2,
    };
    REQUIRE(HitTester::testMesh(area, Span(verts, 3), Span(indices, 3)));
}
