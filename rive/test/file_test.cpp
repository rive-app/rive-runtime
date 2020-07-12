#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include <cstdio>

TEST_CASE("file can be read", "[file]")
{
	FILE* fp = fopen("../../assets/copy_paste_test.riv", "r");
	REQUIRE(fp != nullptr);

	fseek(fp, 0, SEEK_END);
	auto length = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	uint8_t* bytes = new uint8_t[length];
	REQUIRE(fread(bytes, 1, length, fp) == length);
	auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = nullptr;
	auto result = rive::File::import(reader, &file);

	REQUIRE(result == rive::ImportResult::success);
	REQUIRE(file != nullptr);
	REQUIRE(file->artboard() != nullptr);
	
	// Default artboard should be named One.
	REQUIRE(file->artboard()->name() == "One");

	// There should be a second artboard named Two.
	REQUIRE(file->artboard("Two") != nullptr);

	delete[] bytes;
}