#include "catch.hpp"
#include "core/binary_reader.hpp"
#include "node.hpp"
#include "file.hpp"
#include <cstdio>

TEST_CASE("file can be read", "[file]")
{
	FILE* fp = fopen("../../assets/two_artboards.riv", "r");
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


TEST_CASE("file with animation be read", "[file]")
{
	FILE* fp = fopen("../../assets/juice.riv", "r");
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

	auto artboard = file->artboard();
	REQUIRE(artboard->name() == "New Artboard");

	auto shin = artboard->find("shin_right");
	REQUIRE(shin != nullptr);
	REQUIRE(shin->is<rive::Node>());

	auto shinNode = shin->as<rive::Node>();
	REQUIRE(shinNode->parent() != nullptr);
	REQUIRE(shinNode->parent()->name() == "leg_right");
	REQUIRE(shinNode->parent()->parent() != nullptr);
	REQUIRE(shinNode->parent()->parent()->name() == "root");
	REQUIRE(shinNode->parent()->parent() != nullptr);
	REQUIRE(shinNode->parent()->parent()->parent() != nullptr);
	REQUIRE(shinNode->parent()->parent()->parent() == artboard);

	delete[] bytes;
}