/*
 * Copyright 2024 Rive
 */

#include "rive/math/vec2d.hpp"
#include "../src/eval_cubic.hpp"
#include <catch.hpp>

namespace rive::gpu
{
float pow2(float x) { return x * x; }
float pow3(float x) { return x * x * x; }
Vec2D eval_cubic(const Vec2D* p, float t)
{
    return pow3(1 - t) * p[0] + 3 * pow2(1 - t) * t * p[1] + 3 * (1 - t) * pow2(t) * p[2] +
           pow3(t) * p[3];
}

void check_eval_cubic(const EvalCubic& evalCubic, const Vec2D* cubic, float t0, float t1)
{
    float4 pp = evalCubic.at({t0, t0, t1, t1});
    Vec2D p0 = eval_cubic(cubic, t0);
    Vec2D p1 = eval_cubic(cubic, t1);
    CHECK(pp.x == Approx(p0.x).margin(1e-3f));
    CHECK(pp.y == Approx(p0.y).margin(1e-3f));
    CHECK(pp.z == Approx(p1.x).margin(1e-3f));
    CHECK(pp.w == Approx(p1.y).margin(1e-3f));
}

TEST_CASE("EvalCubic_at", "EvalCubic")
{
    Vec2D cubics[][4] = {
        {{199, 1225}, {197, 943}, {349, 607}, {549, 427}},
        {{549, 427}, {349, 607}, {197, 943}, {199, 1225}},
        {{460, 1060}, {403, -320}, {60, 660}, {1181, 634}},
        {{1181, 634}, {60, 660}, {403, -320}, {460, 1060}},
        {{0, 0}, {0, 0}, {0, 0}, {0, 0}},
        {{0, 0}, {0, 0}, {0, 0}, {100, 100}},
        {{0, 0}, {0, 0}, {100, 100}, {100, 100}},
        {{0, 0}, {100, 100}, {100, 100}, {0, 0}},
        {{-100, -100}, {100, 100}, {100, -100}, {-100, 100}}, // Cusp
    };
    for (auto cubic : cubics)
    {
        EvalCubic evalCubic(cubic);
        check_eval_cubic(evalCubic, cubic, 0, 1);
        for (float t = 0; t <= 1; t += 1e-2f)
        {
            check_eval_cubic(evalCubic, cubic, t, t + 3e-3f);
        }
    }
}
} // namespace rive::gpu
