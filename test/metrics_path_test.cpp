#include <catch.hpp>
#include <rive/shapes/metrics_path.hpp>

TEST_CASE("path metrics compute correctly", "[bezier]")
{
    // TODO: fix these based on new logic
    // Make a square with sides length 10.
    // rive::OnlyMetricsPath path;
    // path.moveTo(0, 0);
    // path.lineTo(10, 0);
    // path.lineTo(10, 10);
    // path.lineTo(0, 10);
    // path.close();

    // // Total length should be 40.
    // rive::Mat2D identity;
    // float length = path.computeLength(identity);
    // REQUIRE(length == 40);

    // // Make a path with a single cubic.
    // rive::OnlyMetricsPath cubicPath;
    // cubicPath.moveTo(102, 22);
    // cubicPath.cubicTo(10, 80, 120, 100, 150, 222);
    // cubicPath.close();

    // float cubicLength = cubicPath.computeLength(identity);
    // REQUIRE(cubicLength == 238.38698f);
}