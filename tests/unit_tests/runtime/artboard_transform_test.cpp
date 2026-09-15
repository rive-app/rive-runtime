#include <cmath>
#include <memory>
#include <vector>
#include <rive/artboard.hpp>
#include <rive/math/mat2d.hpp>
#include <rive/animation/animation_state.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/node.hpp>
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

// A zero (or single-axis zero) scale makes the artboard's self transform
// singular. Draw collapses the contents to nothing, so nothing inside can be
// clicked. The listener path inverts that transform to map the pointer into
// content space; when the inverse doesn't exist it used to leave the pointer
// untouched, hit-testing the contents as if the artboard were unscaled.
//
// opaque_hit_test's "main" artboard has an opaque "red" target at [0, 0, 200,
// 200] that sets toGreen=false and an opaque "green" target at [0, 100, 200,
// 300] that sets toGreen=true, plus a non-opaque "gray" target above both that
// flips grayToggle on every click that reaches it.
TEST_CASE("Zero-scale artboard takes no listener hits", "[file]")
{
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");
    auto artboard = file->artboard("main");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance = std::make_unique<StateMachineInstance>(
        artboard->stateMachine("main-state-machine"),
        artboardInstance.get());
    stateMachineInstance->advanceAndApply(0.0f);

    auto toGreen = stateMachineInstance->getBool("toGreen");
    REQUIRE(toGreen != nullptr);
    auto grayToggle = stateMachineInstance->getBool("grayToggle");
    REQUIRE(grayToggle != nullptr);

    // Baseline at unit scale: the green target is reachable.
    stateMachineInstance->pointerDown(Vec2D(100.0f, 250.0f));
    REQUIRE(toGreen->value() == true);
    bool grayBefore = grayToggle->value();

    // Collapsed on both axes: the red target under (100, 50) would clear
    // toGreen if it were hit, and gray would flip.
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerDown(Vec2D(100.0f, 50.0f));
    REQUIRE(toGreen->value() == true);
    REQUIRE(grayToggle->value() == grayBefore);

    // Collapsed on one axis is degenerate too: the determinant is still zero,
    // so there is still no inverse.
    artboardInstance->scaleX(1.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerDown(Vec2D(100.0f, 50.0f));
    REQUIRE(toGreen->value() == true);
    REQUIRE(grayToggle->value() == grayBefore);

    // Restoring an invertible scale restores hit-testing, so the misses above
    // aren't the state machine having gone inert.
    artboardInstance->scaleY(1.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerDown(Vec2D(100.0f, 50.0f));
    REQUIRE(toGreen->value() == false);
    REQUIRE(grayToggle->value() != grayBefore);
}

// StateMachineInstance::hitTest is the other side of the same mapping (it
// answers "is the pointer over anything interactive", and is what a parent
// state machine calls through NestedStateMachine). It must reject a collapsed
// artboard rather than testing the raw position.
TEST_CASE("Zero-scale artboard reports no hit from hitTest", "[file]")
{
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");
    auto artboard = file->artboard("main");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance = std::make_unique<StateMachineInstance>(
        artboard->stateMachine("main-state-machine"),
        artboardInstance.get());
    stateMachineInstance->advanceAndApply(0.0f);

    // World position of a content point, mirroring what draw does: the frame
    // origin translation first, then the artboard's own transform.
    Vec2D frameOffset = artboardInstance->frameOrigin()
                            ? Vec2D(artboardInstance->originX() *
                                        artboardInstance->layoutWidth(),
                                    artboardInstance->originY() *
                                        artboardInstance->layoutHeight())
                            : Vec2D(0.0f, 0.0f);
    Vec2D greenContent(100.0f, 250.0f);
    auto toWorld = [&](Vec2D content) {
        return frameOffset +
               artboardInstance->selfTransform() * (content - frameOffset);
    };

    REQUIRE(stateMachineInstance->hitTest(greenContent) == true);

    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->hitTest(greenContent) == false);
    REQUIRE(stateMachineInstance->hitTest(Vec2D(0.0f, 0.0f)) == false);

    // Half scale is invertible, so the same content is hit at its scaled-down
    // world position.
    artboardInstance->scaleX(0.5f);
    artboardInstance->scaleY(0.5f);
    artboardInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->hitTest(toWorld(greenContent)) == true);
}

// The reported case: the collapsed artboard is the one *mounted* by a
// NestedArtboard. The NestedArtboard component's own world transform stays
// invertible, so the worldToLocal guards in HitNestedArtboard don't fire; the
// zero scale only shows up in the mounted artboard's self transform, which the
// nested state machine inverts.
//
// opaque_hit_test's "second" artboard is a 300x300 rect (flips
// second-gray-toggle) with the opaque "second-nested" artboard on top of it at
// [0, 0, 150, 150] (flips its own bool-target).
TEST_CASE("Zero-scale mounted artboard takes no pointer events", "[file]")
{
    auto file = ReadRiveFile("assets/opaque_hit_test.riv");
    auto artboard = file->artboard("second");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance = std::make_unique<StateMachineInstance>(
        artboard->stateMachine("second-state-machine"),
        artboardInstance.get());

    auto nestedArtboard =
        artboardInstance->find<NestedArtboard>("second-nested");
    REQUIRE(nestedArtboard != nullptr);
    auto nestedTarget = nestedArtboard->nestedAnimations()[0]
                            ->as<NestedStateMachine>()
                            ->stateMachineInstance()
                            ->getBool("bool-target");
    REQUIRE(nestedTarget != nullptr);
    auto grayToggle = stateMachineInstance->getBool("second-gray-toggle");
    REQUIRE(grayToggle != nullptr);

    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);

    auto mounted = nestedArtboard->artboardInstance();
    REQUIRE(mounted != nullptr);
    mounted->scaleX(0.0f);
    mounted->scaleY(0.0f);
    mounted->advance(0.0f);

    // The host's transform is still invertible — the collapse is entirely in
    // the mounted artboard's own scale.
    Mat2D hostInverse;
    REQUIRE(nestedArtboard->worldTransform().invert(&hostInverse) == true);

    bool grayBefore = grayToggle->value();
    stateMachineInstance->pointerDown(Vec2D(100.0f, 50.0f));
    // Nothing inside the collapsed artboard is hit...
    REQUIRE(nestedTarget->value() == false);
    // ...so the click falls through to the rect it was covering.
    REQUIRE(grayToggle->value() != grayBefore);

    // With an invertible scale the mounted artboard takes the click again, and
    // being opaque it stops the rect underneath from seeing it.
    mounted->scaleX(1.0f);
    mounted->scaleY(1.0f);
    mounted->advance(0.0f);
    grayBefore = grayToggle->value();
    stateMachineInstance->pointerDown(Vec2D(100.0f, 50.0f));
    REQUIRE(nestedTarget->value() == true);
    REQUIRE(grayToggle->value() == grayBefore);
}

// updateListeners does more than dispatch hits: it resets every listener
// group, re-establishes hover, advances the click phase, and releases pointer
// state on exit. Collapsing the artboard has to keep that bookkeeping running
// with everything forced to miss -- the same way an occluding target does --
// rather than skipping the pass. Otherwise pointer state freezes at whatever
// it was when the scale hit zero.
//
// click_event's "art-1" has two 200x200 rects (centered at [100, 100] and
// [200, 200]) under one group with a click listener that reports an event.
TEST_CASE("Collapsing an artboard mid-click drops the held press", "[file]")
{
    auto file = ReadRiveFile("assets/click_event.riv");
    auto artboard = file->artboard("art-1");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance =
        std::make_unique<StateMachineInstance>(artboard->stateMachine("sm-1"),
                                               artboardInstance.get());
    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->advance(0.0f);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // Press on the target, then collapse before the release.
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f));
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);

    // The release lands on nothing, so no click completes...
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // ...and the press it was paired with is gone: once the scale comes back, a
    // release with no press of its own must not complete a click.
    artboardInstance->scaleX(1.0f);
    artboardInstance->scaleY(1.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // A fresh press and release still clicks.
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f));
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);

    // Same again, but the collapse is only ever seen by a move. Nothing marks
    // the press as finished in that window, so it has to be cancelled outright
    // rather than left to resume once the scale is back.
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f));
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerMove(Vec2D(75.0f, 75.0f));
    artboardInstance->scaleX(1.0f);
    artboardInstance->scaleY(1.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f));
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
}

// click_event's "art-2" has enter/exit listeners on its shapes that switch the
// layer between a "red" and a "green" animation.
TEST_CASE("Collapsing an artboard exits what the pointer was over", "[file]")
{
    auto file = ReadRiveFile("assets/click_event.riv");
    auto artboard = file->artboard("art-2");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance =
        std::make_unique<StateMachineInstance>(artboard->stateMachine("sm-1"),
                                               artboardInstance.get());
    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->advance(0.0f);

    auto currentAnimation = [&]() {
        auto state = stateMachineInstance->layerState(0);
        REQUIRE(state->is<AnimationState>());
        return std::string(state->as<AnimationState>()->animation()->name());
    };

    stateMachineInstance->pointerMove(Vec2D(75.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(currentAnimation() == "green");

    // Collapsing pulls the content out from under a pointer that hasn't moved,
    // so the exit listener still has to fire.
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerMove(Vec2D(75.0f, 75.0f));
    artboardInstance->advance(0.0f);
    stateMachineInstance->advanceAndApply(0.0f);
    REQUIRE(currentAnimation() == "red");
}

// drag_event's "main" artboard has a drag listener on its shape whose action
// aligns the node the nested artboard hangs off of, so the node's position
// tracks a live drag.
TEST_CASE("Collapsing an artboard mid-drag ends the drag", "[file]")
{
    auto file = ReadRiveFile("assets/drag_event.riv");
    auto artboardInstance = file->artboardDefault();
    REQUIRE(artboardInstance != nullptr);
    auto stateMachineInstance = artboardInstance->stateMachineAt(0);
    stateMachineInstance->bindViewModelInstance(
        file->createDefaultViewModelInstance(artboardInstance.get()));
    stateMachineInstance->advanceAndApply(0.1f);

    auto nested = artboardInstance->find<NestedArtboard>();
    REQUIRE(nested.size() == 1);
    auto dragTarget = nested[0]->parent()->as<Node>();
    float restingX = dragTarget->x();

    // Press, then move: the first move starts the drag gesture (which rebases
    // the gesture's previous position), the second one actually drags.
    stateMachineInstance->pointerDown(Vec2D(250.0f, 250.0f));
    stateMachineInstance->advanceAndApply(0.1f);
    stateMachineInstance->pointerMove(Vec2D(250.0f, 250.0f));
    stateMachineInstance->advanceAndApply(0.1f);
    stateMachineInstance->pointerMove(Vec2D(200.0f, 200.0f));
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(dragTarget->x() != restingX);

    // Collapse mid-drag. The drag listener's branch doesn't consult hover or
    // occlusion, so without an explicit cancel an invisible artboard would go
    // on dragging the target for as long as the pointer moves.
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    float collapsedX = dragTarget->x();
    float collapsedY = dragTarget->y();
    stateMachineInstance->pointerMove(Vec2D(150.0f, 150.0f));
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(dragTarget->x() == collapsedX);
    REQUIRE(dragTarget->y() == collapsedY);

    // Release onto nothing.
    stateMachineInstance->pointerUp(Vec2D(200.0f, 200.0f));
    stateMachineInstance->advanceAndApply(0.1f);

    // The drag ended with the release, so once the scale comes back a move
    // with no button held must not keep dragging the target.
    artboardInstance->scaleX(1.0f);
    artboardInstance->scaleY(1.0f);
    float releasedX = dragTarget->x();
    float releasedY = dragTarget->y();
    stateMachineInstance->pointerMove(Vec2D(100.0f, 100.0f));
    stateMachineInstance->advanceAndApply(0.1f);
    REQUIRE(dragTarget->x() == releasedX);
    REQUIRE(dragTarget->y() == releasedY);
}

// Cancellation covers every pointer the groups are tracking, not just the one
// that happened to deliver the event that observed the collapse -- the contents
// went away for all of them.
TEST_CASE("Collapsing an artboard cancels every pointer", "[file]")
{
    auto file = ReadRiveFile("assets/click_event.riv");
    auto artboard = file->artboard("art-1");
    auto artboardInstance = artboard->instance();
    auto stateMachineInstance =
        std::make_unique<StateMachineInstance>(artboard->stateMachine("sm-1"),
                                               artboardInstance.get());
    stateMachineInstance->advance(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->advance(0.0f);

    // Two pointers pressed on the target.
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f), 0);
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f), 1);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // Only pointer 0 delivers an event while collapsed; pointer 1 sits still.
    artboardInstance->scaleX(0.0f);
    artboardInstance->scaleY(0.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f), 0);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // Pointer 1 never saw the collapse itself, but its press must not have
    // survived it either.
    artboardInstance->scaleX(1.0f);
    artboardInstance->scaleY(1.0f);
    artboardInstance->advance(0.0f);
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f), 1);
    REQUIRE(stateMachineInstance->reportedEventCount() == 0);

    // Both pointers still work from a fresh press.
    stateMachineInstance->pointerDown(Vec2D(75.0f, 75.0f), 1);
    stateMachineInstance->pointerUp(Vec2D(75.0f, 75.0f), 1);
    REQUIRE(stateMachineInstance->reportedEventCount() == 1);
}
