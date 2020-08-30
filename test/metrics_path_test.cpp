#include "catch.hpp"
#include "shapes/metrics_path.hpp"

TEST_CASE("path metrics compute correctly", "[bezier]")
{
	// Make a square with sides length 10.
	rive::OnlyMetricsPath path;
	path.moveTo(0, 0);
	path.lineTo(10, 0);
	path.lineTo(10, 10);
	path.lineTo(0, 10);
	path.close();

	// Total length should be 40.
	float length = path.computeLength();
	REQUIRE(length == 40);

	// Make a path with a single cubic.
	rive::OnlyMetricsPath cubicPath;
	cubicPath.moveTo(102, 22);
	cubicPath.cubicTo(10, 80, 120, 100, 150, 222);
	cubicPath.close();

	float cubicLength = cubicPath.computeLength();
	REQUIRE(cubicLength == 238.38698f);
}