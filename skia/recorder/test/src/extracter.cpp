#include "catch.hpp"
#include "extractor.hpp"

TEST_CASE("Text extractor source not found")
{
	REQUIRE_THROWS_WITH(new RiveFrameExtractor("missing.riv", "", "", ""),
	                    "Failed to open file missing.riv");
}

TEST_CASE("Text extractor no animation")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_animation.riv", "", "", ""),
	    "Artboard doesn't contain a default animation.");
}

TEST_CASE("Text extractor no artboard")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_artboard.riv", "", "", ""),
	    "File doesn't contain a default artboard.");
}

TEST_CASE("Text extractor odd width")
{
	auto rive = new RiveFrameExtractor("./static/51x50.riv", "", "", "");
	REQUIRE(rive->width() == 52);
	REQUIRE(rive->height() == 50);
}

TEST_CASE("Text extractor odd height")
{
	auto rive = new RiveFrameExtractor("./static/50x51.riv", "", "", "");
	REQUIRE(rive->width() == 50);
	REQUIRE(rive->height() == 52);
}
