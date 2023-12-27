#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/math/circle_constant.hpp>
#include <rive/node.hpp>
#include <rive/shapes/ellipse.hpp>
#include <rive/shapes/path_composer.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/solo.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include "rive_file_reader.hpp"
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
    Close,
    AddPath,
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
    void addPath(rive::CommandPath* path, const rive::Mat2D& transform) override
    {
        commands.emplace_back(
            TestPathCommand{TestPathCommandType::AddPath, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f});
    }
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
    rive::rcp<rive::RenderPath> makeEmptyRenderPath() override
    {
        return rive::make_rcp<TestRenderPath>();
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

TEST_CASE("nested solo with shape expanded and path collapsed", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("../../test/assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard = file->artboard("test-1-shape-with-shape-and-path")->instance();
    // Root-Shape
    artboard->advance(0.0f);
    auto rootShape = artboard->children()[0]->as<rive::Shape>();
    REQUIRE(rootShape != nullptr);
    REQUIRE(rootShape->name() == "Root-Shape");
    auto s1 = artboard->find<rive::Solo>("Solo-1");
    REQUIRE(s1 != nullptr);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 1);
    auto solo = solos[0];
    REQUIRE(solo->children().size() == 2);
    REQUIRE(solo->children()[0]->is<rive::Shape>());
    REQUIRE(solo->children()[0]->name() == "Rectangle-shape");
    REQUIRE(solo->children()[1]->is<rive::Path>());
    REQUIRE(solo->children()[1]->name() == "Path-2");
    auto rectangleShape = solo->children()[0]->as<rive::Shape>();
    auto path = solo->children()[1]->as<rive::Path>();
    REQUIRE(rectangleShape->isCollapsed() == false);
    REQUIRE(path->isCollapsed() == true);
    REQUIRE(path->commandPath() != nullptr);
    auto pathComposer = rootShape->pathComposer();
    auto pathComposerPath = static_cast<TestRenderPath*>(pathComposer->localPath());
    // Path is skipped and the nested shape forms its own drawable, so size is 0
    REQUIRE(pathComposerPath->commands.size() == 0);
}

TEST_CASE("nested solo clipping with shape collapsed and path expanded", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("../../test/assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard = file->artboard("test-2-clip-with-shape-and-path")->instance();
    // Root-Shape
    artboard->advance(0.0f);
    auto rectangleClip = artboard->find<rive::Shape>("Rectangle-clipped");
    REQUIRE(rectangleClip != nullptr);
    REQUIRE(rectangleClip->name() == "Rectangle-clipped");
    auto s1 = artboard->find<rive::Solo>("Solo-name");
    REQUIRE(s1 != nullptr);
    auto solos = artboard->find<rive::Solo>();
    REQUIRE(solos.size() == 1);
    auto solo = solos[0];
    REQUIRE(solo->children().size() == 2);
    REQUIRE(solo->children()[0]->is<rive::Shape>());
    REQUIRE(solo->children()[0]->name() == "Rectangle-shape");
    REQUIRE(solo->children()[1]->is<rive::Path>());
    REQUIRE(solo->children()[1]->name() == "Path-2");
    auto rectangleShape = solo->children()[0]->as<rive::Shape>();
    auto path = solo->children()[1]->as<rive::Path>();
    REQUIRE(rectangleShape->isCollapsed() == true);
    REQUIRE(path->isCollapsed() == false);
    REQUIRE(path->commandPath() != nullptr);
    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    auto clippingPath = static_cast<TestRenderPath*>(clippingShape->renderPath());
    // One path is skipped, otherwise size would be 3
    REQUIRE(clippingPath->commands.size() == 2);
}

TEST_CASE("nested solo clipping with animation", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("../../test/assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard = file->artboard("test-5-clip-with-group-and-path-and-shape")->instance();
    artboard->advance(0.0f);
    auto rectangleClip = artboard->find<rive::Shape>("Rectangle-clipped");
    REQUIRE(rectangleClip != nullptr);
    REQUIRE(rectangleClip->name() == "Rectangle-clipped");
    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    auto clippingPath = static_cast<TestRenderPath*>(clippingShape->renderPath());
    REQUIRE(clippingPath != nullptr);
    std::unique_ptr<rive::LinearAnimationInstance> animation = artboard->animationAt(0);
    // First a single shape is drawn as part of the solo
    REQUIRE(clippingPath->commands[0].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[1].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 2);
    animation->advanceAndApply(2.5f);
    // Then a different shape is drawn as part of the solo
    REQUIRE(clippingPath->commands[2].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[3].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 4);
    animation->advanceAndApply(2.5f);
    // Then an empty group is focused, so there is no path drawn
    REQUIRE(clippingPath->commands[4].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands.size() == 5);
    animation->advanceAndApply(2.5f);
    // Then an empty group is focused, so there is no path drawn
    REQUIRE(clippingPath->commands[5].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[6].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 7);
    animation->advanceAndApply(1.0f);
    // Then back to the group so no path is added
    REQUIRE(clippingPath->commands[7].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands.size() == 8);
    animation->advanceAndApply(2.5f);
    // Finally a new shape is added
    REQUIRE(clippingPath->commands[8].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[9].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 10);
}

TEST_CASE("double nested solos clipping with animation", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("../../test/assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard = file->artboard("test-6-clip-with-nested-solos")->instance();
    artboard->advance(0.0f);
    auto rectangleClip = artboard->find<rive::Shape>("Rectangle-clipped");
    REQUIRE(rectangleClip != nullptr);
    REQUIRE(rectangleClip->name() == "Rectangle-clipped");
    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    auto clippingPath = static_cast<TestRenderPath*>(clippingShape->renderPath());
    REQUIRE(clippingPath != nullptr);
    std::unique_ptr<rive::LinearAnimationInstance> animation = artboard->animationAt(0);
    // First a single shape is drawn as part of the solo
    REQUIRE(clippingPath->commands[0].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[1].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 2);
    animation->advanceAndApply(1.5f); // 1.5s in timeline
    // Changed nested group but path does not change.
    // Reset and AddPath are called anyway because it is marked as dirty
    REQUIRE(clippingPath->commands[2].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[3].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 4);
    animation->advanceAndApply(1.0f); // 2.5s in timeline
    // Both solos are pointing to empty groups
    REQUIRE(clippingPath->commands[4].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands.size() == 5);
    animation->advanceAndApply(1.0f); // 3.5s in timeline
    // Nothing changes, it hasn't reached any new keyframe
    REQUIRE(clippingPath->commands.size() == 5);
    animation->advanceAndApply(1.0f); // 4.5s in timeline
    // Outer solo is pointing to inner solo, but inner solo is pointing to empty group
    REQUIRE(clippingPath->commands.size() == 5);
    animation->advanceAndApply(1.0f); // 5.5s in timeline
    // Outer solo is pointing to inner solo, inner solo is pointing to shape
    REQUIRE(clippingPath->commands[5].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[6].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 7);
    animation->advanceAndApply(1.0f); // 6.5s in timeline
    // Outer solo pointing to empty group
    REQUIRE(clippingPath->commands[7].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands.size() == 8);
    animation->advanceAndApply(2.0f); // 8.5s in timeline
    // Outer solo pointing to inner solo. Inner solo pointing to rect shape
    REQUIRE(clippingPath->commands[8].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[9].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 10);
    animation->advanceAndApply(2.0f); // 10.5s in timeline
    // Outer solo pointing to inner solo. Inner solo pointing to path
    REQUIRE(clippingPath->commands[10].command == TestPathCommandType::Reset);
    REQUIRE(clippingPath->commands[11].command == TestPathCommandType::AddPath);
    REQUIRE(clippingPath->commands.size() == 12);
}
