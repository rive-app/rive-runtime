#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/image.hpp>
#include "no_op_renderer.hpp"
#include <catch.hpp>
#include <cstdio>

TEST_CASE("image assets loads correctly", "[assets]")
{
	FILE* fp = fopen("../../test/assets/walle.riv", "r");
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

	auto node = file->artboard()->find("walle");
	REQUIRE(node != nullptr);
	REQUIRE(node->is<rive::Image>());
	auto walle = node->as<rive::Image>();
	REQUIRE(walle->imageAsset() != nullptr);

	auto eve_left = file->artboard()->find("eve_left");
	REQUIRE(eve_left != nullptr);
	REQUIRE(eve_left->is<rive::Image>());
	REQUIRE(eve_left->as<rive::Image>()->imageAsset() != nullptr);

	auto eve_right = file->artboard()->find("eve_right");
	REQUIRE(eve_right != nullptr);
	REQUIRE(eve_right->is<rive::Image>());
	REQUIRE(eve_right->as<rive::Image>()->imageAsset() != nullptr);
	REQUIRE(eve_right->as<rive::Image>()->imageAsset() != walle->imageAsset());
	REQUIRE(eve_right->as<rive::Image>()->imageAsset() ==
	        eve_left->as<rive::Image>()->imageAsset());

	file->artboard()->updateComponents();

	rive::NoOpRenderer renderer;
	file->artboard()->draw(&renderer);

	delete file;
	delete[] bytes;
}