#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "node.hpp"

TEST_CASE("Node instances", "[core]") { REQUIRE(rive::Node().x() == 1.0f); }

TEST_CASE("nodeX function return x value", "[node]")
{
	rive::Node* node = new rive::Node();
	REQUIRE(rive::nodeX(node) == 1.0f);
	node->set_x(2.0f);
	REQUIRE(rive::nodeX(node) == 2.0f);
}