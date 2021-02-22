#include "catch.hpp"
#include "main.hpp"

TEST_CASE("Get filename check hello.world")
{
	REQUIRE(getFileName("hello.world") == "hello");
}

TEST_CASE("Get filename check things/hello.world")
{
	REQUIRE(getFileName("things/hello.world") == "hello");
}

TEST_CASE("Get filename check multiple/things/hello.world")
{
	REQUIRE(getFileName("multiple/things/hello.world") == "hello");
}

TEST_CASE("Get filename check multiple/things/hello.world.com")
{
	REQUIRE(getFileName("multiple/things/hello.world.com") == "hello.world");
}
