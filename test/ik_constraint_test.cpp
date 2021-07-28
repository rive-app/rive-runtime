#include "rive_testing.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "constraints/ik_constraint.hpp"
#include "node.hpp"
#include "math/vec2d.hpp"
#include "shapes/shape.hpp"
#include "bones/skin.hpp"
#include "bones/bone.hpp"
#include <cstdio>

TEST_CASE("ik with skinned bones orders correctly", "[file]")
{
	FILE* fp = fopen("../../test/assets/complex_ik_dependency.riv", "r");
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

	REQUIRE(artboard->find<rive::Bone>("One") != nullptr);
	auto one = artboard->find<rive::Bone>("One");

	REQUIRE(artboard->find<rive::Bone>("Two") != nullptr);
	auto two = artboard->find<rive::Bone>("Two");
	rive::Skin* skin = nullptr;
	for (auto object : artboard->objects())
	{
		if (object->is<rive::Skin>())
		{
			skin = object->as<rive::Skin>();
			break;
		}
	}

	REQUIRE(skin != nullptr);
	REQUIRE(two->constraints()[0]->is<rive::IKConstraint>());

	REQUIRE(skin->graphOrder() > one->graphOrder());
	REQUIRE(skin->graphOrder() > two->graphOrder());

	delete file;
	delete[] bytes;
}