#include <catch.hpp>
#include <rive/math/math_types.hpp>
#include <rive/shapes/metrics_path.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/trim_path.hpp>
#include <utils/no_op_factory.hpp>

using namespace rive;

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

// Regression test for a crash found by fuzzing.
TEST_CASE("fuzz_issue_7295", "[MetricsPath]")
{
    NoOpFactory factory;

    OnlyMetricsPath innerPath;
    innerPath.moveTo(.0f, -20.5f);
    innerPath.cubicTo(11.3218384f, -20.5f, 20.5f, -11.3218384f, 20.5f, .0f);
    innerPath.cubicTo(20.5f, 11.3218384f, 11.3218384f, 20.5f, .0f, 20.5f);
    innerPath.cubicTo(-11.3218384f, 20.5f, -20.5f, 11.3218384f, -20.5f, .0f);
    innerPath.cubicTo(-20.5f, -11.3218384f, -11.3218384f, -20.5f, .0f, -20.5f);

    OnlyMetricsPath outerPath;
    outerPath.addPath(&innerPath, Mat2D(1.f, .0f, .0f, 1.f, -134217728.f, -134217728.f));

    RawPath result;
    outerPath.paths()[0]->trim(.0f, 168.389008f, true, &result);
    CHECK(math::nearly_equal(outerPath.paths()[0]->length(), 168.389008f));
}
