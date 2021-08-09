#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include "no_op_renderer.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("draw rules load and sort correctly", "[draw rules]")
{
	FILE* fp = fopen("../../test/assets/draw_rule_cycle.riv", "r");
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

	// auto node = file->artboard()->node("TopEllipse");
	// REQUIRE(node != nullptr);
	// REQUIRE(node->is<rive::Shape>());

	// auto shape = node->as<rive::Shape>();
	// REQUIRE(shape->clippingShapes().size() == 2);
	// REQUIRE(shape->clippingShapes()[0]->shape()->name() == "ClipRect2");
	// REQUIRE(shape->clippingShapes()[1]->shape()->name() == "BabyEllipse");

	// file->artboard()->updateComponents();

	// rive::NoOpRenderer renderer;
	// file->artboard()->draw(&renderer);
	delete file;
	delete[] bytes;
}