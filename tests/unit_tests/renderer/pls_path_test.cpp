/*
 * Copyright 2024 Rive
 */

#include "rive/math/math_types.hpp"
#include "../src/rive_render_path.hpp"
#include <catch.hpp>

namespace rive::gpu
{
class PLSTestPath : public RiveRenderPath
{
public:
    void addRect(float x,
                 float y,
                 float width,
                 float height,
                 PathDirection wind)
    {
        RawPath rawPath;
        rawPath.addRect({x, y, x + width, y + height}, wind);
        RiveRenderPath path2(FillRule::nonZero, rawPath);
        addRenderPath(&path2, Mat2D());
    }

    void addCircle(float x, float y, float radius, PathDirection wind)
    {
        RawPath rawPath;
        rawPath.addOval({x - radius, y - radius, x + radius, y + radius}, wind);
        RiveRenderPath path2(FillRule::nonZero, rawPath);
        addRenderPath(&path2, Mat2D());
    }
};

// Check that RiveRenderPath::getCoarseArea() is positive for clockwise paths
// and negative for counterclockwise. Also check that it's within a margin of
// error of the true area for curves.
TEST_CASE("getCoarseArea", "[RiveRenderPath]")
{
    PLSTestPath path;
    CHECK(path.getCoarseArea() == 0);

    path.addRect(10, 20, 10, 200, PathDirection::clockwise); // 2000 px
    CHECK(path.getCoarseArea() == 2000);

    path.addRect(15, 30, 2, 20, PathDirection::counterclockwise); // -40 px
    CHECK(path.getCoarseArea() == (2000 - 40));

    path.addRect(0, 0, 1, 1, PathDirection::clockwise); // +1 px
    CHECK(path.getCoarseArea() == (2000 - 40 + 1));

    path.addRect(-1,
                 -1,
                 300,
                 100,
                 PathDirection::counterclockwise); // -30000 px
    CHECK(path.getCoarseArea() == (2000 - 40 + 1 - 30000));
    CHECK(path.getCoarseArea() == (2000 - 40 + 1 - 30000));

    path.rewind();
    CHECK(path.getCoarseArea() == 0);
    CHECK(path.getCoarseArea() == 0);

    path.addCircle(0, 0, 1000, PathDirection::clockwise);
    CHECK(path.getCoarseArea() / (math::PI * 1000 * 1000) ==
          Approx(1).margin(1e-2f));
    path.rewind();

    path.addCircle(30, -100, 900, PathDirection::counterclockwise);
    CHECK(path.getCoarseArea() / (math::PI * 900 * 900) ==
          Approx(-1).margin(1e-2f));

    path.addCircle(0, 0, 1000, PathDirection::clockwise);
    CHECK(path.getCoarseArea() /
              (math::PI * 1000 * 1000 - math::PI * 900 * 900) ==
          Approx(1).margin(1e-2f));
}
} // namespace rive::gpu
