#include "rive/math/vec2d.hpp"
#include "rive_testing.hpp"
#include "rive/math/rectangles_to_contour.hpp"

using namespace rive;

TEST_CASE("RectanglesToContour generates correct contour", "[text]")
{
    RectanglesToContour rectanglesToContour;
    rectanglesToContour.addRect(AABB(10.0f, 10.0f, 20.0f, 20.0f));
    rectanglesToContour.addRect(AABB(20.0f, 10.0f, 30.0f, 20.0f));
    rectanglesToContour.computeContours();
    CHECK(rectanglesToContour.contourCount() == 1);
    CHECK(rectanglesToContour.contour(0).size() == 4);
    CHECK_VEC2D(rectanglesToContour.contour(0).point(0), Vec2D(10.0f, 10.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(1), Vec2D(10.0f, 20.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(2), Vec2D(30.0f, 20.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(3), Vec2D(30.0f, 10.0f));

    rectanglesToContour.reset();
    rectanglesToContour.addRect(AABB(10.0f, 10.0f, 20.0f, 20.0f));
    rectanglesToContour.addRect(AABB(20.0f, 10.0f, 30.0f, 20.0f));
    rectanglesToContour.addRect(AABB(20.0f, 40.0f, 30.0f, 50.0f));

    rectanglesToContour.computeContours();
    CHECK(rectanglesToContour.contourCount() == 2);
    CHECK(rectanglesToContour.contour(0).size() == 4);
    CHECK_VEC2D(rectanglesToContour.contour(0).point(0), Vec2D(10.0f, 10.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(1), Vec2D(10.0f, 20.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(2), Vec2D(30.0f, 20.0f));
    CHECK_VEC2D(rectanglesToContour.contour(0).point(3), Vec2D(30.0f, 10.0f));

    CHECK(rectanglesToContour.contour(1).size() == 4);
    CHECK_VEC2D(rectanglesToContour.contour(1).point(0), Vec2D(20.0f, 40.0f));
    CHECK_VEC2D(rectanglesToContour.contour(1).point(1), Vec2D(20.0f, 50.0f));
    CHECK_VEC2D(rectanglesToContour.contour(1).point(2), Vec2D(30.0f, 50.0f));
    CHECK_VEC2D(rectanglesToContour.contour(1).point(3), Vec2D(30.0f, 40.0f));
}