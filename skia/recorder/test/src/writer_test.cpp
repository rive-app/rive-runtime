#define private public // Expose private fields/methods.
#include "catch.hpp"
#include "writer.hpp"

TEST_CASE("No format for file")
{
	REQUIRE_THROWS_WITH(MovieWriter("no_format", 100, 100, 60, 0),
	                    "Failed to determine output format for no_format.");
}

TEST_CASE("Bitrate has been set to the 10,000kbps", "[writer]")
{
	auto writer = MovieWriter("./output.mp4", 100, 100, 60, 10000);
	REQUIRE(writer.m_Cctx->bit_rate == 10000 * 1000);
}

TEST_CASE("Bitrate is empty: set to 0 (auto)", "[no_bitrate]")
{
	auto writer = MovieWriter("./output.mp4", 100, 100, 60);
	REQUIRE(writer.m_Cctx->bit_rate == 0);
}