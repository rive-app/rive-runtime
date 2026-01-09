#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/math/circle_constant.hpp>
#include <rive/node.hpp>
#include <rive/shapes/ellipse.hpp>
#include <rive/shapes/path_composer.hpp>
#include <rive/shapes/paint/feather.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include <rive/solo.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include "rive_file_reader.hpp"
#include "rive/math/path_types.hpp"
#include "rive/animation/state_machine_instance.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "utils/serializing_factory.hpp"
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
    void addRawPath(const rive::RawPath& path) override { path.addTo(this); }

    void rewind() override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::Reset,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f});
    }

    void fillRule(rive::FillRule value) override {}
    void addPath(rive::CommandPath* path, const rive::Mat2D& transform) override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::AddPath,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f});
    }
    void addRenderPath(rive::RenderPath* path,
                       const rive::Mat2D& transform) override
    {}

    void moveTo(float x, float y) override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::MoveTo,
                                              x,
                                              y,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f});
    }
    void lineTo(float x, float y) override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::LineTo,
                                              x,
                                              y,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f});
    }
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::CubicTo,
                                              x,
                                              y,
                                              ix,
                                              iy,
                                              ox,
                                              oy});
    }
    void close() override
    {
        commands.emplace_back(TestPathCommand{TestPathCommandType::Close,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f,
                                              0.0f});
    }
};

class TestNoOpFactory : public rive::NoOpFactory
{
public:
    rive::rcp<rive::RenderPath> makeEmptyRenderPath() override
    {
        return rive::make_rcp<TestRenderPath>();
    }

    rive::rcp<rive::RenderPath> makeRenderPath(rive::RawPath&,
                                               rive::FillRule) override
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

    auto rawPath = rectangle->rawPath();

    auto verbs = rawPath.verbs();
    REQUIRE(verbs.size() == 6);
    REQUIRE(verbs[0] == rive::PathVerb::move);
    REQUIRE(verbs[1] == rive::PathVerb::line);
    REQUIRE(verbs[2] == rive::PathVerb::line);
    REQUIRE(verbs[3] == rive::PathVerb::line);
    REQUIRE(verbs[4] == rive::PathVerb::line);
    REQUIRE(verbs[5] == rive::PathVerb::close);
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

    auto rawPath = rectangle->rawPath();

    // rewind
    // moveTo
    // cubic - for 1st corner

    // lineTo, cubicTo for 2nd corner
    // lineTo, cubicTo for 3rd corner
    // lineTo, cubicTo for 4th corner

    // close
    auto verbs = rawPath.verbs();
    REQUIRE(verbs.size() == 10);

    // Init
    REQUIRE(verbs[0] == rive::PathVerb::move);

    // 1st
    REQUIRE(verbs[1] == rive::PathVerb::cubic);

    // 2nd
    REQUIRE(verbs[2] == rive::PathVerb::line);
    REQUIRE(verbs[3] == rive::PathVerb::cubic);

    // 3rd
    REQUIRE(verbs[4] == rive::PathVerb::line);
    REQUIRE(verbs[5] == rive::PathVerb::cubic);

    // 4th
    REQUIRE(verbs[6] == rive::PathVerb::line);
    REQUIRE(verbs[7] == rive::PathVerb::cubic);

    REQUIRE(verbs[8] == rive::PathVerb::line);

    REQUIRE(verbs[9] == rive::PathVerb::close);
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

    auto path = ellipse->rawPath();

    // rewind
    // moveTo
    // cubic - for 1st corner

    // lineTo, cubicTo for 2nd corner
    // lineTo, cubicTo for 3rd corner
    // lineTo, cubicTo for 4th corner

    // close

    auto verbs = path.verbs();
    auto points = path.points();
    REQUIRE(verbs.size() == 6);

    // Init
    REQUIRE(verbs[0] == rive::PathVerb::move);
    REQUIRE(points[0].x == 0.0f);
    REQUIRE(points[0].y == -100.0f);

    // 1st
    REQUIRE(verbs[1] == rive::PathVerb::cubic);
    REQUIRE(points[1].x == 50.0f * rive::circleConstant);
    REQUIRE(points[1].y == -100.0f);
    REQUIRE(points[2].x == 50.0f);
    REQUIRE(points[2].y == -100.0f * rive::circleConstant);
    REQUIRE(points[3].x == 50.0f);
    REQUIRE(points[3].y == 0.0f);

    // 2nd
    REQUIRE(verbs[2] == rive::PathVerb::cubic);
    REQUIRE(points[4].x == 50.0f);
    REQUIRE(points[4].y == 100.0f * rive::circleConstant);
    REQUIRE(points[5].x == 50.0f * rive::circleConstant);
    REQUIRE(points[5].y == 100.0f);
    REQUIRE(points[6].x == 0.0f);
    REQUIRE(points[6].y == 100.0f);

    // 3rd
    REQUIRE(verbs[3] == rive::PathVerb::cubic);
    REQUIRE(points[7].x == -50.0f * rive::circleConstant);
    REQUIRE(points[7].y == 100.0f);
    REQUIRE(points[8].x == -50.0f);
    REQUIRE(points[8].y == 100.0f * rive::circleConstant);
    REQUIRE(points[9].x == -50.0f);
    REQUIRE(points[9].y == 0.0f);

    // 4th
    REQUIRE(verbs[4] == rive::PathVerb::cubic);
    REQUIRE(points[10].x == -50.0f);
    REQUIRE(points[10].y == -100.0f * rive::circleConstant);
    REQUIRE(points[11].x == -50.0f * rive::circleConstant);
    REQUIRE(points[11].y == -100.0f);
    REQUIRE(points[12].x == 0.0f);
    REQUIRE(points[12].y == -100.0f);

    REQUIRE(verbs[5] == rive::PathVerb::close);
}

TEST_CASE("nested solo with shape expanded and path collapsed", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard =
        file->artboard("test-1-shape-with-shape-and-path")->instance();
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

    auto pathComposer = rootShape->pathComposer();
    auto localPath = pathComposer->localPath();
    REQUIRE(localPath != nullptr);
    REQUIRE(localPath->numContours() == 0);
    auto pathComposerPath =
        static_cast<TestRenderPath*>(localPath->renderPath(&emptyFactory));
    // Path is skipped and the nested shape forms its own drawable, so size is 0
    REQUIRE(pathComposerPath != nullptr);
    REQUIRE(pathComposerPath->commands.size() == 0);
}

TEST_CASE("nested solo clipping with shape collapsed and path expanded",
          "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard =
        file->artboard("test-2-clip-with-shape-and-path")->instance();
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

    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    auto clippingPath = clippingShape->path();
    REQUIRE(clippingPath != nullptr);
    // One path is skipped, otherwise size would be 2
    REQUIRE(clippingPath->numContours() == 1);
}

TEST_CASE("nested solo clipping with animation", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard =
        file->artboard("test-5-clip-with-group-and-path-and-shape")->instance();
    artboard->advance(0.0f);
    auto rectangleClip = artboard->find<rive::Shape>("Rectangle-clipped");
    REQUIRE(rectangleClip != nullptr);
    REQUIRE(rectangleClip->name() == "Rectangle-clipped");
    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    auto clippingPath = clippingShape->path();
    REQUIRE(clippingPath != nullptr);
    std::unique_ptr<rive::LinearAnimationInstance> animation =
        artboard->animationAt(0);

    // First a single shape is drawn as part of the solo
    REQUIRE(clippingPath->numContours() == 1);
    {
        auto renderPath = clippingPath->renderPath(&emptyFactory);
        REQUIRE(renderPath != nullptr);
    }

    animation->advanceAndApply(2.5f);
    // Then a different shape is drawn as part of the solo
    REQUIRE(clippingPath->numContours() == 1);
    // no cached render path as it's a new contour.
    REQUIRE(!clippingPath->hasRenderPath());
    {
        auto renderPath = clippingPath->renderPath(&emptyFactory);
        REQUIRE(renderPath != nullptr);
    }

    // // Then an empty group is focused, so there is no path drawn
    animation->advanceAndApply(2.5f);
    REQUIRE(clippingPath->numContours() == 0);

    animation->advanceAndApply(2.5f);
    REQUIRE(clippingPath->numContours() == 1);

    // Then back to the group so no path is added
    animation->advanceAndApply(1.0f);
    REQUIRE(clippingPath->numContours() == 0);

    animation->advanceAndApply(2.5f);
    // Finally a new shape is added
    REQUIRE(clippingPath->numContours() == 1);
}

TEST_CASE("double nested solos clipping with animation", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("assets/solos_collapse_tests.riv", &emptyFactory);

    auto artboard = file->artboard("test-6-clip-with-nested-solos")->instance();
    artboard->advance(0.0f);
    auto rectangleClip = artboard->find<rive::Shape>("Rectangle-clipped");
    REQUIRE(rectangleClip != nullptr);
    REQUIRE(rectangleClip->name() == "Rectangle-clipped");
    auto clippingShape = rectangleClip->clippingShapes()[0];
    REQUIRE(clippingShape != nullptr);
    // auto clippingPath = static_cast<TestRenderPath*>(
    //     clippingShape->path()->renderPath(&emptyFactory));
    auto clippingPath = clippingShape->path();
    REQUIRE(clippingPath != nullptr);
    std::unique_ptr<rive::LinearAnimationInstance> animation =
        artboard->animationAt(0);
    // First a single shape is drawn as part of the solo
    REQUIRE(clippingPath->numContours() == 1);
    animation->advanceAndApply(1.5f); // 1.5s in timeline
    // Changed nested group but path does not change.
    // Reset and AddPath are called anyway because it is marked as dirty
    REQUIRE(clippingPath->numContours() == 1);
    REQUIRE(!clippingPath->hasRenderPath());
    {
        auto renderPath = clippingPath->renderPath(&emptyFactory);
        REQUIRE(renderPath != nullptr);
    }

    animation->advanceAndApply(1.0f); // 2.5s in timeline

    // Both solos are pointing to empty groups
    REQUIRE(clippingPath->numContours() == 0);
    animation->advanceAndApply(1.0f); // 3.5s in timeline

    // Nothing changes, it hasn't reached any new keyframe
    REQUIRE(clippingPath->numContours() == 0);
    animation->advanceAndApply(1.0f); // 4.5s in timeline

    // Outer solo is pointing to inner solo, but inner solo is pointing to empty
    // group
    REQUIRE(clippingPath->numContours() == 0);
    animation->advanceAndApply(1.0f); // 5.5s in timeline

    // Outer solo is pointing to inner solo, inner solo is pointing to shape
    REQUIRE(clippingPath->numContours() == 1);
    animation->advanceAndApply(1.0f); // 6.5s in timeline

    // Outer solo pointing to empty group
    REQUIRE(clippingPath->numContours() == 0);
    animation->advanceAndApply(2.0f); // 8.5s in timeline

    // Outer solo pointing to inner solo. Inner solo pointing to rect shape
    REQUIRE(clippingPath->numContours() == 1);
    animation->advanceAndApply(2.0f); // 10.5s in timeline
    // Outer solo pointing to inner solo. Inner solo pointing to path
    REQUIRE(clippingPath->numContours() == 1);
    REQUIRE(!clippingPath->hasRenderPath());
    {
        auto renderPath = clippingPath->renderPath(&emptyFactory);
        REQUIRE(renderPath != nullptr);
    }
}

TEST_CASE("Path  with feather render", "[path]")
{
    TestNoOpFactory emptyFactory;
    auto file = ReadRiveFile("assets/feather_render_test.riv", &emptyFactory);

    auto artboard = file->artboard("button 2")->instance();
    REQUIRE(artboard != nullptr);
    auto node = artboard->find("button_shape");
    REQUIRE(node != nullptr);
    REQUIRE(node->is<rive::Shape>());
    auto stroke = node->as<rive::Shape>()->children()[2];
    REQUIRE(stroke != nullptr);
    auto feather = stroke->as<rive::Fill>()->feather();
    REQUIRE(feather != nullptr);
    auto machine = artboard->defaultStateMachine();
    REQUIRE(feather->renderCount == 0);
    machine->advanceAndApply(0.0f);
    REQUIRE(feather->renderCount == 1);
    machine->advanceAndApply(0.25f);
    REQUIRE(feather->renderCount == 2);
}

TEST_CASE("Apply stacked path effects to paths", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/stacked_path_effects.riv", &silver);

    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("stacked_path_effects"));
}

TEST_CASE("Apply trim path effect to fill", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/fill_trim_path.riv", &silver);

    auto artboard = file->artboardNamed("main");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    stateMachine->advanceAndApply(0.016f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.016f);
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("fill_trim_path"));
}

TEST_CASE("Apply group effect to multiple paths", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/group_effect.riv", &silver);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);
    auto dashValue =
        vmi->propertyValue("dashValue")->as<rive::ViewModelInstanceNumber>();

    auto shape2 = artboard->find<rive::Shape>("shape2");
    rive::ShapePaint* shapePaint2 = nullptr;
    for (auto& child : shape2->children())
    {
        if (child->is<rive::ShapePaint>())
        {
            shapePaint2 = child->as<rive::ShapePaint>();
            break;
        }
    }
    REQUIRE(shapePaint2 != nullptr);

    auto shape1 = artboard->find<rive::Shape>("shape1");
    rive::ShapePaint* shapePaint1 = nullptr;
    for (auto& child : shape1->children())
    {
        if (child->is<rive::ShapePaint>())
        {
            shapePaint1 = child->as<rive::ShapePaint>();
            break;
        }
    }
    REQUIRE(shapePaint1 != nullptr);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    silver.addFrame();
    dashValue->propertyValue(10);

    // Manually advancing and update data bind to test for dirt
    artboard->advanceInternal(
        0.0f,
        rive::AdvanceFlags::IsRoot | rive::AdvanceFlags::Animate |
            rive::AdvanceFlags::AdvanceNested | rive::AdvanceFlags::NewFrame);
    artboard->updateDataBinds();
    // Ensure that invalidating one shape paint does not cause the other shape
    // paint to invalidate even if they share the same group effect
    REQUIRE(shapePaint1->dirt() == rive::ComponentDirt::Path);
    REQUIRE(shapePaint2->dirt() == rive::ComponentDirt::None);
    artboard->updatePass(true);
    stateMachine->advanceAndApply(0.0f);
    artboard->draw(renderer.get());

    int frames = (int)(1.0f / 0.064f);

    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.064f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("group_effect"));
}

TEST_CASE("Apply group effect with missing items", "[silver]")
{
    rive::SerializingFactory silver;
    auto file = ReadRiveFile("assets/group_effect.riv", &silver);

    auto artboard = file->artboardNamed("main-missing-targets");
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);
    int viewModelId = artboard.get()->viewModelId();

    auto vmi = viewModelId == -1
                   ? file->createViewModelInstance(artboard.get())
                   : file->createViewModelInstance(viewModelId, 0);

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.0f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    CHECK(silver.matches("group_effect-main-missing-targets"));
}
