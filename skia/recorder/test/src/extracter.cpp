#include "catch.hpp"
#include "extractor.hpp"

TEST_CASE("Text extractor source not found")
{
	REQUIRE_THROWS_WITH(new RiveFrameExtractor("missing.riv", "", "", "", 0, 0),
	                    "Failed to open file missing.riv");
}

TEST_CASE("Text extractor no animation")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_animation.riv", "", "", "", 0, 0),
	    "Artboard doesn't contain a default animation.");
}

TEST_CASE("Text extractor no artboard")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_artboard.riv", "", "", "", 0, 0),
	    "File doesn't contain a default artboard.");
}

TEST_CASE("Text extractor odd width")
{
	auto rive = new RiveFrameExtractor("./static/51x50.riv", "", "", "", 0, 0);
	REQUIRE(rive->width() == 52);
	REQUIRE(rive->height() == 50);
}

TEST_CASE("Text extractor odd height")
{
	auto rive = new RiveFrameExtractor("./static/50x51.riv", "", "", "", 0, 0);
	REQUIRE(rive->width() == 50);
	REQUIRE(rive->height() == 52);
}

TEST_CASE("Text extractor width override")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 100, 0);
	REQUIRE(rive->width() == 100);
	REQUIRE(rive->height() == 52);
}

TEST_CASE("Text extractor height override")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 0, 100);
	REQUIRE(rive->width() == 50);
	REQUIRE(rive->height() == 100);
}

TEST_CASE("Text small extent target (width)")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 50, 100, 720);
	REQUIRE(rive->width() == 720);
	REQUIRE(rive->height() == 1440);
}

TEST_CASE("Text small extent target maxed (width)")
{
	auto rive = new RiveFrameExtractor(
	    "./static/50x51.riv", "", "", "", 50, 100, 720, 1080, 1080);
	REQUIRE(rive->width() == 540);
	REQUIRE(rive->height() == 1080);
}

TEST_CASE("Text small extent target (height)")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 100, 50, 720);
	REQUIRE(rive->height() == 720);
	REQUIRE(rive->width() == 1440);
}

TEST_CASE("Text small extent target maxed (height)")
{
	auto rive = new RiveFrameExtractor(
	    "./static/50x51.riv", "", "", "", 100, 50, 720, 1080, 1080);
	REQUIRE(rive->height() == 540);
	REQUIRE(rive->width() == 1080);
}
