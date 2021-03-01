#include "catch.hpp"
#include "main.hpp"

TEST_CASE("String format check hello world")
{
	REQUIRE(string_format("hello %s", "world") == "hello world");
}

TEST_CASE("String format check hello world")
{
	REQUIRE(string_format("hello %s %d", "cruel", "world") == "hello world 1");
}

// Was kinda expecting to be able to use this in multiple ways. but i think i
// need some different compiler thign for hat..
//
// Undefined symbols for architecture x86_64:
//   "std::__1::basic_string<char, std::__1::char_traits<char>,
//   std::__1::allocator<char> > string_format<char const*,
//   int>(std::__1::basic_string<char, std::__1::char_traits<char>,
//   std::__1::allocator<char> > const&, char const*, int)", referenced from:
//       ____C_A_T_C_H____T_E_S_T____2() in stringFormat.o
// ld: symbol(s) not found for architecture x86_64
// REQUIRE(string_format("hello %s %d", "cruel", "world") == "hello world 1");
// REQUIRE(string_format("hello {} {}", "world", 2) == "hello world 2");
