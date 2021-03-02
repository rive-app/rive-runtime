#include "catch.hpp"
#include "extractor.hpp"

TEST_CASE("Test extractor source not found")
{
	REQUIRE_THROWS_WITH(new RiveFrameExtractor("missing.riv", "", "", "", 0, 0),
	                    "Failed to open file missing.riv");
}

TEST_CASE("Test extractor no animation")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_animation.riv", "", "", "", 0, 0),
	    "Artboard doesn't contain a default animation.");
}

TEST_CASE("Test extractor no artboard")
{
	REQUIRE_THROWS_WITH(
	    new RiveFrameExtractor("./static/no_artboard.riv", "", "", "", 0, 0),
	    "File doesn't contain a default artboard.");
}

TEST_CASE("Test extractor odd width")
{
	auto rive = new RiveFrameExtractor("./static/51x50.riv", "", "", "", 0, 0);
	REQUIRE(rive->width() == 52);
	REQUIRE(rive->height() == 50);
}

TEST_CASE("Test extractor odd height")
{
	auto rive = new RiveFrameExtractor("./static/50x51.riv", "", "", "", 0, 0);
	REQUIRE(rive->width() == 50);
	REQUIRE(rive->height() == 52);
}

TEST_CASE("Test extractor width override")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 100, 0);
	REQUIRE(rive->width() == 100);
	REQUIRE(rive->height() == 52);
}

TEST_CASE("Test extractor height override")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 0, 100);
	REQUIRE(rive->width() == 50);
	REQUIRE(rive->height() == 100);
}

TEST_CASE("Test small extent target (width)")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 50, 100, 720);
	REQUIRE(rive->width() == 720);
	REQUIRE(rive->height() == 1440);
}

TEST_CASE("Test small extent target maxed (width)")
{
	auto rive = new RiveFrameExtractor(
	    "./static/50x51.riv", "", "", "", 50, 100, 720, 1080, 1080);
	REQUIRE(rive->width() == 540);
	REQUIRE(rive->height() == 1080);
}

TEST_CASE("Test small extent target (height)")
{
	auto rive =
	    new RiveFrameExtractor("./static/50x51.riv", "", "", "", 100, 50, 720);
	REQUIRE(rive->height() == 720);
	REQUIRE(rive->width() == 1440);
}

TEST_CASE("Test small extent target maxed (height)")
{
	auto rive = new RiveFrameExtractor(
	    "./static/50x51.riv", "", "", "", 100, 50, 720, 1080, 1080);
	REQUIRE(rive->height() == 540);
	REQUIRE(rive->width() == 1080);
}

TEST_CASE("Test 1s_oneShot min 10s")
{
	auto rive = new RiveFrameExtractor(
	    "./static/animations.riv", "", "1s_oneShot", "", 0, 0, 0, 0, 0, 10);
	REQUIRE(rive->totalFrames() == 600);
}

TEST_CASE("Test 2s_loop min 5s")
{
	auto rive = new RiveFrameExtractor(
	    "./static/animations.riv", "", "2s_loop", "", 0, 0, 0, 0, 0, 5);
	REQUIRE(rive->totalFrames() == 360);
}

TEST_CASE("Test 2s_loop min 5s max 5s")
{
	// give it something dumb, it'll do something dumb.
	auto rive = new RiveFrameExtractor(
	    "./static/animations.riv", "", "2s_loop", "", 0, 0, 0, 0, 0, 5, 5);
	REQUIRE(rive->totalFrames() == 300);
}

TEST_CASE("Test 2s_pingpong min 5s")
{
	auto rive = new RiveFrameExtractor(
	    "./static/animations.riv", "", "2s_pingpong", "", 0, 0, 0, 0, 0, 5);
	REQUIRE(rive->totalFrames() == 480);
}

TEST_CASE("Test 100s_oneshot animation min duration 10s")
{
	auto rive = new RiveFrameExtractor("./static/animations.riv",
	                                   "",
	                                   "100s_oneShot",
	                                   "",
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   10);
	REQUIRE(rive->totalFrames() == 600);
}

TEST_CASE("Test 100s_loop animation min duration 10s")
{
	auto rive = new RiveFrameExtractor(
	    "./static/animations.riv", "", "100s_loop", "", 0, 0, 0, 0, 0, 0, 10);
	REQUIRE(rive->totalFrames() == 600);
}

TEST_CASE("Test 100s_pingpong animation min duration 10s")
{
	auto rive = new RiveFrameExtractor("./static/animations.riv",
	                                   "",
	                                   "100s_pingpong",
	                                   "",
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   0,
	                                   10);
	REQUIRE(rive->totalFrames() == 600);
}
