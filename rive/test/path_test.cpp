#include "artboard.hpp"
#include "catch.hpp"
#include "core/binary_reader.hpp"
#include "file.hpp"
#include "no_op_renderer.hpp"
#include "node.hpp"
#include "shapes/rectangle.hpp"
#include "shapes/shape.hpp"
#include <cstdio>

TEST_CASE("rectangle path builds expected commands", "[path]")
{
	rive::Artboard* artboard = new rive::Artboard();
	rive::Rectangle* rectangle = new rive::Rectangle();

	rectangle->x(0.0f);
	rectangle->y(0.0f);
	rectangle->width(100.0f);
	rectangle->height(200.0f);
	rectangle->cornerRadius(0.0f);

	artboard->addObject(artboard);
	artboard->addObject(rectangle);

	artboard->initialize();

	artboard->advance(0.0f);

	REQUIRE(rectangle->renderPath() != nullptr);

	auto path =
	    reinterpret_cast<rive::NoOpRenderPath*>(rectangle->renderPath());

	REQUIRE(path->commands.size() == 6);
	REQUIRE(path->commands[0].command == rive::NoOpPathCommandType::Reset);
	REQUIRE(path->commands[1].command == rive::NoOpPathCommandType::MoveTo);
	REQUIRE(path->commands[2].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[3].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[4].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[5].command == rive::NoOpPathCommandType::Close);

	delete artboard;
}

TEST_CASE("rounded rectangle path builds expected commands", "[path]")
{
	rive::Artboard* artboard = new rive::Artboard();
	rive::Rectangle* rectangle = new rive::Rectangle();

	rectangle->x(0.0f);
	rectangle->y(0.0f);
	rectangle->width(100.0f);
	rectangle->height(200.0f);
	rectangle->cornerRadius(20.0f);

	artboard->addObject(artboard);
	artboard->addObject(rectangle);

	artboard->initialize();

	artboard->advance(0.0f);

	REQUIRE(rectangle->renderPath() != nullptr);

	auto path =
	    reinterpret_cast<rive::NoOpRenderPath*>(rectangle->renderPath());

	// reset
	// moveTo
	// cubic - for 1st corner

	// lineTo, cubicTo for 2nd corner
	// lineTo, cubicTo for 3rd corner
	// lineTo, cubicTo for 4th corner

	// close

	REQUIRE(path->commands.size() == 10);

	// Init
	REQUIRE(path->commands[0].command == rive::NoOpPathCommandType::Reset);
	REQUIRE(path->commands[1].command == rive::NoOpPathCommandType::MoveTo);

	// 1st
	REQUIRE(path->commands[2].command == rive::NoOpPathCommandType::CubicTo);

	// 2nd
	REQUIRE(path->commands[3].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[4].command == rive::NoOpPathCommandType::CubicTo);

	// 3rd
	REQUIRE(path->commands[5].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[6].command == rive::NoOpPathCommandType::CubicTo);

	// 4th
	REQUIRE(path->commands[7].command == rive::NoOpPathCommandType::LineTo);
	REQUIRE(path->commands[8].command == rive::NoOpPathCommandType::CubicTo);

	REQUIRE(path->commands[9].command == rive::NoOpPathCommandType::Close);

	delete artboard;
}