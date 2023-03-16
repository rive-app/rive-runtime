#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/math/circle_constant.hpp>
#include <rive/node.hpp>
#include <rive/shapes/ellipse.hpp>
#include <rive/shapes/path_composer.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <catch.hpp>
#include <cstdio>

// Need a specialized "noop" factory that does make an inspectable Path

namespace
{
enum class TestPathCommandType
{
    MoveTo,
    LineTo,
    CubicTo,
    Reset,
    Close
};

struct TestPathCommand
{
    TestPathCommandType command;
    float x;
    float y;
    float inX;
    float inY;
    float outX;
    float outY;
};

class TestRenderPath : public rive::RenderPath
{
public:
    std::vector<TestPathCommand> commands;
    void rewind() override
    {
        commands.emplace_back(
            TestPathCommand{TestPathCommandType::Reset, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    }

    void fillRule(rive::FillRule value) override {}
    void addPath(rive::CommandPath* path, const rive::Mat2D& transform) override {}
    void addRenderPath(rive::RenderPath* path, const rive::Mat2D& transform) override {}

    void moveTo(float x, float y) override
    {
        commands.emplace_back(
            TestPathCommand{TestPathCommandType::MoveTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
    }
    void lineTo(float x, float y) override
    {
        commands.emplace_back(
            TestPathCommand{TestPathCommandType::LineTo, x, y, 0.0f, 0.0f, 0.0f, 0.0f});
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y) override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::CubicTo, x, y, ix, iy, ox, oy});
    }
    void close() override
    {
        commands.emplace_back(
            TestPathCommand{TestPathCommandType::Close, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    }
};

class TestNoOpFactory : public rive::NoOpFactory
{
public:
    std::unique_ptr<rive::RenderPath> makeEmptyRenderPath() override
    {
        return rivestd::make_unique<TestRenderPath>();
    }
};
} // namespace

TEST_CASE("rectangle path builds expected commands", "[path]")
{
    TestNoOpFactory emptyFactory;
    rive::Artboard artboard(&emptyFactory);
    rive::Shape* shape = new rive::Shape();
    rive::Rectangle* rectangle = new rive::Rectangle();

    rectangle->x(0.0f);
    rectangle->y(0.0f);
    rectangle->width(100.0f);
    rectangle->height(200.0f);
    rectangle->cornerRadiusTL(0.0f);

    artboard.addObject(&artboard);
    artboard.addObject(shape);
    artboard.addObject(rectangle);
    rectangle->parentId(1);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);

    artboard.advance(0.0f);

    REQUIRE(rectangle->commandPath() != nullptr);

    auto path = static_cast<TestRenderPath*>(rectangle->commandPath());

    REQUIRE(path->commands.size() == 7);
    REQUIRE(path->commands[0].command == TestPathCommandType::Reset);
    REQUIRE(path->commands[1].command == TestPathCommandType::MoveTo);
    REQUIRE(path->commands[2].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[3].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[4].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[5].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[6].command == TestPathCommandType::Close);
}

TEST_CASE("rounded rectangle path builds expected commands", "[path]")
{
    TestNoOpFactory emptyFactory;
    rive::Artboard artboard(&emptyFactory);
    rive::Shape* shape = new rive::Shape();
    rive::Rectangle* rectangle = new rive::Rectangle();

    rectangle->x(0.0f);
    rectangle->y(0.0f);
    rectangle->width(100.0f);
    rectangle->height(200.0f);
    rectangle->cornerRadiusTL(20.0f);
    rectangle->linkCornerRadius(true);

    artboard.addObject(&artboard);
    artboard.addObject(shape);
    artboard.addObject(rectangle);
    rectangle->parentId(1);

    artboard.initialize();

    artboard.advance(0.0f);

    REQUIRE(rectangle->commandPath() != nullptr);

    auto path = static_cast<TestRenderPath*>(rectangle->commandPath());

    // rewind
    // moveTo
    // cubic - for 1st corner

    // lineTo, cubicTo for 2nd corner
    // lineTo, cubicTo for 3rd corner
    // lineTo, cubicTo for 4th corner

    // close

    REQUIRE(path->commands.size() == 11);

    // Init
    REQUIRE(path->commands[0].command == TestPathCommandType::Reset);
    REQUIRE(path->commands[1].command == TestPathCommandType::MoveTo);

    // 1st
    REQUIRE(path->commands[2].command == TestPathCommandType::CubicTo);

    // 2nd
    REQUIRE(path->commands[3].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[4].command == TestPathCommandType::CubicTo);

    // 3rd
    REQUIRE(path->commands[5].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[6].command == TestPathCommandType::CubicTo);

    // 4th
    REQUIRE(path->commands[7].command == TestPathCommandType::LineTo);
    REQUIRE(path->commands[8].command == TestPathCommandType::CubicTo);

    REQUIRE(path->commands[9].command == TestPathCommandType::LineTo);

    REQUIRE(path->commands[10].command == TestPathCommandType::Close);
}

TEST_CASE("ellipse path builds expected commands", "[path]")
{
    TestNoOpFactory emptyFactory;
    rive::Artboard artboard(&emptyFactory);
    rive::Ellipse* ellipse = new rive::Ellipse();
    rive::Shape* shape = new rive::Shape();

    ellipse->x(0.0f);
    ellipse->y(0.0f);
    ellipse->width(100.0f);
    ellipse->height(200.0f);

    artboard.addObject(&artboard);
    artboard.addObject(shape);
    artboard.addObject(ellipse);
    ellipse->parentId(1);

    artboard.initialize();

    artboard.advance(0.0f);

    REQUIRE(ellipse->commandPath() != nullptr);

    auto path = static_cast<TestRenderPath*>(ellipse->commandPath());

    // rewind
    // moveTo
    // cubic - for 1st corner

    // lineTo, cubicTo for 2nd corner
    // lineTo, cubicTo for 3rd corner
    // lineTo, cubicTo for 4th corner

    // close

    REQUIRE(path->commands.size() == 7);

    // Init
    REQUIRE(path->commands[0].command == TestPathCommandType::Reset);
    REQUIRE(path->commands[1].command == TestPathCommandType::MoveTo);
    REQUIRE(path->commands[1].x == 0.0f);
    REQUIRE(path->commands[1].y == -100.0f);

    // 1st
    REQUIRE(path->commands[2].command == TestPathCommandType::CubicTo);
    REQUIRE(path->commands[2].outX == 50.0f * rive::circleConstant);
    REQUIRE(path->commands[2].outY == -100.0f);
    REQUIRE(path->commands[2].inX == 50.0f);
    REQUIRE(path->commands[2].inY == -100.0f * rive::circleConstant);
    REQUIRE(path->commands[2].x == 50.0f);
    REQUIRE(path->commands[2].y == 0.0f);

    // 2nd
    REQUIRE(path->commands[3].command == TestPathCommandType::CubicTo);
    REQUIRE(path->commands[3].outX == 50.0f);
    REQUIRE(path->commands[3].outY == 100.0f * rive::circleConstant);
    REQUIRE(path->commands[3].inX == 50.0f * rive::circleConstant);
    REQUIRE(path->commands[3].inY == 100.0f);
    REQUIRE(path->commands[3].x == 0.0f);
    REQUIRE(path->commands[3].y == 100.0f);

    // 3rd
    REQUIRE(path->commands[4].command == TestPathCommandType::CubicTo);
    REQUIRE(path->commands[4].outX == -50.0f * rive::circleConstant);
    REQUIRE(path->commands[4].outY == 100.0f);
    REQUIRE(path->commands[4].inX == -50.0f);
    REQUIRE(path->commands[4].inY == 100.0f * rive::circleConstant);
    REQUIRE(path->commands[4].x == -50.0f);
    REQUIRE(path->commands[4].y == 0.0f);

    // 4th
    REQUIRE(path->commands[5].command == TestPathCommandType::CubicTo);
    REQUIRE(path->commands[5].outX == -50.0f);
    REQUIRE(path->commands[5].outY == -100.0f * rive::circleConstant);
    REQUIRE(path->commands[5].inX == -50.0f * rive::circleConstant);
    REQUIRE(path->commands[5].inY == -100.0f);
    REQUIRE(path->commands[5].x == 0.0f);
    REQUIRE(path->commands[5].y == -100.0f);

    REQUIRE(path->commands[6].command == TestPathCommandType::Close);
}
