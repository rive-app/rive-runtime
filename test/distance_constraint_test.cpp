#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/constraints/distance_constraint.hpp>
#include <rive/node.hpp>
#include <rive/math/vec2d.hpp>
#include <rive/shapes/shape.hpp>
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("distance constraints moves items as expected", "[file]")
{
	FILE* fp = fopen("../../test/assets/distance_constraint.riv", "r");
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

	REQUIRE(artboard->find<rive::Shape>("A") != nullptr);
	auto a = artboard->find<rive::Shape>("A");

	REQUIRE(artboard->find<rive::Shape>("B") != nullptr);
	auto b = artboard->find<rive::Shape>("B");

	REQUIRE(a->constraints().size() == 1);
	REQUIRE(a->constraints()[0]->is<rive::DistanceConstraint>());

	auto distanceConstraint =
	    a->constraints()[0]->as<rive::DistanceConstraint>();
	REQUIRE(distanceConstraint->modeValue() == 1);

	b->x(259.31f);
	b->y(137.87f);
	artboard->advance(0.0f);

	rive::Vec2D at;
	a->worldTranslation(at);
	rive::Vec2D expectedTranslation(259.2808837890625, 62.87000274658203);
	REQUIRE(rive::Vec2D::distance(at, expectedTranslation) < 0.001f);

	delete file;
	delete[] bytes;
}