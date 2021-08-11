#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/math/transform_components.hpp>
#include "no_op_renderer.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("rotation constraint updates world transform", "[file]")
{
	FILE* fp = fopen("../../test/assets/rotation_constraint.riv", "r");
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

	REQUIRE(artboard->find<rive::TransformComponent>("target") != nullptr);
	auto target = artboard->find<rive::TransformComponent>("target");

	REQUIRE(artboard->find<rive::TransformComponent>("rect") != nullptr);
	auto rectangle = artboard->find<rive::TransformComponent>("rect");

	artboard->advance(0.0f);
	rive::TransformComponents targetComponents;
	rive::Mat2D::decompose(targetComponents, target->worldTransform());
	rive::TransformComponents rectComponents;
	rive::Mat2D::decompose(rectComponents, rectangle->worldTransform());

	REQUIRE(targetComponents.rotation() == rectComponents.rotation());

	delete file;
	delete[] bytes;
}