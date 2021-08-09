#include <rive/core/binary_reader.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/bones/bone.hpp>
#include <rive/shapes/shape.hpp>
#include "no_op_renderer.hpp"
#include "rive_testing.hpp"
#include <cstdio>

TEST_CASE("two bone ik places bones correctly", "[file]")
{
	FILE* fp = fopen("../../test/assets/two_bone_ik.riv", "r");
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

	REQUIRE(artboard->find<rive::Shape>("circle a") != nullptr);
	auto circleA = artboard->find<rive::Shape>("circle a");

	REQUIRE(artboard->find<rive::Shape>("circle b") != nullptr);
	auto circleB = artboard->find<rive::Shape>("circle b");

	REQUIRE(artboard->find<rive::Bone>("a") != nullptr);
	auto boneA = artboard->find<rive::Bone>("a");

	REQUIRE(artboard->find<rive::Bone>("b") != nullptr);
	auto boneB = artboard->find<rive::Bone>("b");

	REQUIRE(artboard->find<rive::Node>("target") != nullptr);
	auto target = artboard->find<rive::Node>("target");

	REQUIRE(artboard->animation("Animation 1") != nullptr);
	auto animation = artboard->animation("Animation 1");

	// Make sure dependency structure is correct. Important thing here is to
	// ensure that circle a is dependent upon the tip of the ik chain (bone b).
	// circle b is a child of bone b so it'll be there anyway, but may as well
	// validate.
	REQUIRE(std::find(boneB->dependents().begin(),
	                  boneB->dependents().end(),
	                  circleA) != boneB->dependents().end());
	REQUIRE(std::find(boneB->dependents().begin(),
	                  boneB->dependents().end(),
	                  circleB) != boneB->dependents().end());

	animation->apply(artboard, 0.0f, 1.0f);
	artboard->advance(0.0f);
	REQUIRE(target->x() == 296.0f);
	REQUIRE(target->y() == 202.0f);
	REQUIRE(aboutEqual(boneA->worldTransform(),
	                   rive::Mat2D(0.11632211506366729736328125f,
	                               -0.993211567401885986328125f,
	                               0.993211567401885986328125f,
	                               0.11632211506366729736328125f,
	                               26.015254974365234375f,
	                               475.2149658203125f)));

	REQUIRE(aboutEqual(boneB->worldTransform(),
	                   rive::Mat2D(0.974071562290191650390625f,
	                               0.2262403070926666259765625f,
	                               -0.2262403070926666259765625f,
	                               0.974071562290191650390625f,
	                               64.31568145751953125f,
	                               148.1883544921875f)));

	animation->apply(artboard, 1.0f, 1.0f);
	artboard->advance(0.0f);
	REQUIRE(target->x() == 450.0f);
	REQUIRE(target->y() == 337.0f);
	REQUIRE(aboutEqual(boneA->worldTransform(),
	                   rive::Mat2D(0.650279819965362548828125f,
	                               -0.7596948146820068359375f,
	                               0.7596948146820068359375f,
	                               0.650279819965362548828125f,
	                               26.015254974365234375f,
	                               475.2149658203125f)));

	REQUIRE(aboutEqual(boneB->worldTransform(),
	                   rive::Mat2D(0.8823678493499755859375f,
	                               0.470560371875762939453125f,
	                               -0.47056043148040771484375f,
	                               0.882367908954620361328125f,
	                               240.1275634765625f,
	                               225.07647705078125f)));

	delete file;
	delete[] bytes;
}