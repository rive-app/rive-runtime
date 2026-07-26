#include <cmath>
#include <vector>
#include <rive/artboard.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/nested_artboard.hpp>
#include <utils/no_op_renderer.hpp>
#include <utils/serializing_factory.hpp>
#include <rive_file_reader.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>

using namespace rive;

// Records every transform pushed onto the renderer so we can assert the
// artboard bakes its own rotation/scale into the draw path.
class TransformRecordingRenderer : public NoOpRenderer
{
public:
    std::vector<Mat2D> transforms;
    void transform(const Mat2D& m) override { transforms.push_back(m); }

    bool contains(const Mat2D& expected) const
    {
        for (const auto& m : transforms)
        {
            bool match = true;
            for (int i = 0; i < 6; i++)
            {
                if (std::abs(m[i] - expected[i]) > 0.0001f)
                {
                    match = false;
                    break;
                }
            }
            if (match)
            {
                return true;
            }
        }
        return false;
    }
};

// An artboard's own rotation/scale must be applied to its contents when it
// renders (pivoted at the origin, i.e. content-local 0,0), so it works both
// top-level and nested. Before the fix these properties were inert.
TEST_CASE("Artboard bakes its own rotation/scale into draw", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    auto artboard = file->artboard()->instance();
    artboard->advance(0.0f);

    // Pure scale (identity rotation) is an unambiguous matrix to look for.
    artboard->scaleX(2.0f);
    artboard->scaleY(3.0f);
    artboard->advance(0.0f);

    TransformRecordingRenderer renderer;
    artboard->draw(&renderer);

    Mat2D expectedScale = Mat2D::fromRotation(0.0f);
    expectedScale.scaleByValues(2.0f, 3.0f);
    REQUIRE(renderer.contains(expectedScale));

    // Now rotation combined with scale.
    artboard->rotation(1.5707963f); // 90 degrees
    artboard->advance(0.0f);

    TransformRecordingRenderer rotated;
    artboard->draw(&rotated);

    Mat2D expectedRotScale = Mat2D::fromRotation(1.5707963f);
    expectedRotScale.scaleByValues(2.0f, 3.0f);
    REQUIRE(rotated.contains(expectedRotScale));
}

// A default (no rotation, unit scale) artboard must push exactly one fewer
// transform than a scaled one — i.e. the rotation/scale transform is only added
// when it is non-default, preserving existing behavior for the common case.
TEST_CASE("Artboard transform is only pushed when non-default", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");

    auto plain = file->artboard()->instance();
    plain->advance(0.0f);
    TransformRecordingRenderer plainRenderer;
    plain->draw(&plainRenderer);

    auto scaled = file->artboard()->instance();
    scaled->scaleX(2.0f);
    scaled->advance(0.0f);
    TransformRecordingRenderer scaledRenderer;
    scaled->draw(&scaledRenderer);

    REQUIRE(scaledRenderer.transforms.size() ==
            plainRenderer.transforms.size() + 1);
}

// Hit-testing must apply the same transform as draw. With the artboard rotated,
// a listener target is reached from the inverse-mapped world position, not its
// original one. opaque_hit_test's "main" artboard has a "red" target up top and
// a "green" target lower down; clicking green sets toGreen=true.
TEST_CASE("Artboard rotation is honored in state-machine hit-testing", "[file]")
{
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");
    auto artboard = file->artboard("main");
    auto artboardInstance = artboard->instance();

    // Rotate 180 degrees about the origin.
    artboardInstance->rotation(3.1415927f);
    artboardInstance->advance(0.0f);

    auto stateMachineInstance =
        new StateMachineInstance(artboard->stateMachine("main-state-machine"),
                                 artboardInstance.get());
    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->advance(0.0f);
    auto toGreen = stateMachineInstance->getBool("toGreen");
    REQUIRE(toGreen != nullptr);

    // The world point that maps to the same content the un-rotated green target
    // sat under (originally clicked at (100, 250)):
    // world = frameOffset + R * (contentWorld - frameOffset).
    Vec2D frameOffset(
        artboardInstance->originX() * artboardInstance->layoutWidth(),
        artboardInstance->originY() * artboardInstance->layoutHeight());
    Mat2D r = artboardInstance->selfTransform();
    Vec2D greenContent(100.0f, 250.0f);
    Vec2D greenWorld = frameOffset + r * (greenContent - frameOffset);

    // Rotation actually moved the target away from its original location.
    REQUIRE((greenWorld - greenContent).length() > 1.0f);

    // Clicking the rotated location hits green; clicking the original does not.
    stateMachineInstance->pointerDown(greenContent);
    REQUIRE(toGreen->value() == false);
    stateMachineInstance->pointerDown(greenWorld);
    REQUIRE(toGreen->value() == true);

    delete stateMachineInstance;
}

// rootTransform (which drives Node::computedRootX/Y) must fold in a nested
// artboard's own rotation/scale, since that transform is part of how its
// contents land in the parent. Before the fix it was ignored.
TEST_CASE("Nested artboard's own rotation affects rootTransform", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    auto mainArtboard = file->artboard()->instance();
    auto artboard = mainArtboard->find<Artboard>("Parent Artboard");
    artboard->updateComponents();
    auto container =
        artboard->find<NestedArtboard>("Nested artboard container");
    auto nested = container->artboardInstance();

    Vec2D point(10.0f, 0.0f);
    auto before = nested->rootTransform(point);

    nested->rotation(1.5707964f); // 90 degrees
    nested->updateComponents();
    auto rotated = nested->rootTransform(point);

    REQUIRE((rotated - before).length() > 1.0f);
}

TEST_CASE("Artboard transform and opacity", "[silver]")
{
    SerializingFactory silver;
    auto file =
        ReadRiveFile("assets/artboard_opacity_and_transform_test.riv", &silver);
    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    auto stateMachine = artboard->stateMachineAt(0);

    auto vmi = file->createDefaultViewModelInstance(artboard.get());

    auto xPos = vmi->propertyValue("xPos")->as<ViewModelInstanceNumber>();
    auto yPos = vmi->propertyValue("yPos")->as<ViewModelInstanceNumber>();

    stateMachine->bindViewModelInstance(vmi);
    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());

    int frames = 11;
    for (int i = 0; i < frames; i++)
    {
        silver.addFrame();
        stateMachine->advanceAndApply(0.1f);

        stateMachine->pointerDown(
            Vec2D(xPos->propertyValue(), yPos->propertyValue()));
        stateMachine->pointerUp(
            Vec2D(xPos->propertyValue(), yPos->propertyValue()));
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("artboard_opacity_and_transform_test"));
}
// Records the composed renderer transform in effect when clipPath is called, so
// we can assert the artboard clip is transformed by its own rotation/scale.
class ClipRecordingRenderer : public rive::NoOpRenderer
{
public:
    std::vector<rive::Mat2D> stack{rive::Mat2D()};
    bool clipped = false;
    rive::Mat2D clipTransform;

    void save() override { stack.push_back(stack.back()); }
    void restore() override { stack.pop_back(); }
    void transform(const rive::Mat2D& m) override
    {
        stack.back() = stack.back() * m;
    }
    void clipPath(rive::RenderPath* path) override
    {
        clipped = true;
        clipTransform = stack.back();
    }
};

// A clipping artboard must clip in the same (rotated/scaled) space as its
// content. Before the fix the clip was applied before the transforms, so a
// rotated artboard clipped against an axis-aligned rect.
TEST_CASE("Artboard clip is transformed by its own rotation", "[file]")
{
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    auto artboard = file->artboard()->instance();
    artboard->clip(true);
    artboard->rotation(1.5707963f); // 90 degrees
    artboard->advance(0.0f);

    ClipRecordingRenderer renderer;
    artboard->draw(&renderer);

    REQUIRE(renderer.clipped);
    // The clip is applied under a transform carrying the rotation (non-zero
    // off-diagonal terms), not an axis-aligned matrix.
    REQUIRE((std::abs(renderer.clipTransform[1]) > 0.0001f ||
             std::abs(renderer.clipTransform[2]) > 0.0001f));
}
