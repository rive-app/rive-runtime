#include "catch.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "no_op_renderer.hpp"
#include "node.hpp"
#include "shapes/rectangle.hpp"
#include "shapes/shape.hpp"
#include <cstdio>

TEST_CASE("file with state machine be read", "[file]")
{
	FILE* fp = fopen("../../test/assets/rocket.riv", "r");
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
	auto artboard = file->artboard();
	REQUIRE(artboard != nullptr);
	REQUIRE(artboard->animationCount() == 3);
	REQUIRE(artboard->stateMachineCount() == 1);

	delete file;
	delete[] bytes;
}