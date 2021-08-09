#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include "no_op_renderer.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("transform constraint updates world transform", "[file]")
{
	FILE* fp = fopen("../../test/assets/transform_constraint.riv", "r");
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

	REQUIRE(artboard->find<rive::TransformComponent>("Target") != nullptr);
	auto target = artboard->find<rive::TransformComponent>("Target");

	REQUIRE(artboard->find<rive::TransformComponent>("Rectangle") != nullptr);
	auto rectangle = artboard->find<rive::TransformComponent>("Rectangle");

	artboard->advance(0.0f);

	// Expect the transform constraint to have placed the shape in the same
	// exact world transform as the target.
	REQUIRE(aboutEqual(target->worldTransform(), rectangle->worldTransform()));

	delete file;
	delete[] bytes;
}