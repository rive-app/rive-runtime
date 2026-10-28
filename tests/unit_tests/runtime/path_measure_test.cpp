#include "rive/math/path_measure.hpp"
#include "rive/math/raw_path.hpp"
#include <catch.hpp>

TEST_CASE("a distance past the end lands on the end", "[path_measure]")
{
  rive::RawPath path;
  path.move({0.0f, 0.0f});
  path.line({10.0f, 0.0f});
  path.line({10.0f, 10.0f});
  rive::PathMeasure measure(&path);

  auto past = measure.atDistance(measure.length() * 1.001f);
  CHECK(past.pos.x == Approx(10.0f));
  CHECK(past.pos.y == Approx(10.0f));

  auto full = measure.atPercentage(1.0f);
  CHECK(full.pos.x == Approx(10.0f));
  CHECK(full.pos.y == Approx(10.0f));
}

TEST_CASE("an empty path measure positions at the origin", "[path_measure]")
{
  rive::PathMeasure measure;
  auto result = measure.atPercentage(0.5f);
  CHECK(result.pos.x == 0.0f);
  CHECK(result.pos.y == 0.0f);
  CHECK(result.tan.x == 0.0f);
  CHECK(result.tan.y == 0.0f);
}
