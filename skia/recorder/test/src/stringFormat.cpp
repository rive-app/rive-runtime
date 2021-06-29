#include "catch.hpp"
#include "util.hxx"

TEST_CASE("String format check hello world", "[.]")
{
	REQUIRE(string_format("hello %s", "world") == "hello world");
}

TEST_CASE("String format check hello cruel world", "[.]")
{
	REQUIRE(string_format("hello %s %s", "cruel", "world") ==
	        "hello cruel world");
}

TEST_CASE("String format check hello sweet world", "[.]")
{
	REQUIRE(string_format("hello %s %d", "sweet", 1) == "hello sweet 1");
}
