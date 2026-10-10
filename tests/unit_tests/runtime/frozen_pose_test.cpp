#include <rive/file.hpp>
#include <rive/artboard_component_list.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/color.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/viewmodel/viewmodel_instance_boolean.hpp>
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <memory>

// A layer resting on an animation that writes keyframe values as they are -- a
// pose, or a one-shot that has finished and is held at its end -- stops
// re-applying it while nothing reaches its artboard. Every one skipped under
// TESTING is also checked against the artboard and aborts the run if a value
// it would have written isn't there, so the whole suite checks it; these cases
// pin down that it engages, and that a write to the artboard brings the values
// back.
//
// frozen_pose.riv: layer "base" rests on a one-frame pose painting the
// rectangle red and holding the artboard's own rotation at 0. Layer "flash",
// later in order, paints it blue for 10 frames when the view model's flash
// turns true, then exits. Layer "fade" fades the square in over 4 frames and
// holds the finished animation; it also keys the square's y with a single
// keyframe far past its end, which writes the same value at any time and so
// doesn't stop it freezing.

using namespace rive;

static const ColorInt red = colorARGB(255, 255, 0, 0);
static const ColorInt blue = colorARGB(255, 0, 0, 255);

static SolidColor* rectangleColor(ArtboardInstance* artboard)
{
    auto shape = artboard->find<Shape>("rectangle");
    REQUIRE(shape != nullptr);
    REQUIRE(shape->children()[1]->is<Fill>());
    return shape->children()[1]->as<Fill>()->paint()->as<SolidColor>();
}

static Shape* square(ArtboardInstance* artboard)
{
    auto shape = artboard->find<Shape>("square");
    REQUIRE(shape != nullptr);
    return shape;
}

struct FrozenPoseScene
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    rcp<ViewModelInstance> viewModelInstance;
    std::unique_ptr<StateMachineInstance> machine;

    FrozenPoseScene()
    {
        file = ReadRiveFile("assets/frozen_pose.riv");
        artboard = file->artboard()->instance();
        viewModelInstance =
            file->createDefaultViewModelInstance(artboard.get());
        machine = artboard->defaultStateMachine();
        machine->bindViewModelInstance(viewModelInstance);
        machine->advanceAndApply(0.0f);
    }

    void advance(int frames)
    {
        for (int i = 0; i < frames; i++)
        {
            machine->advanceAndApply(1.0f / 60.0f);
        }
    }
};

TEST_CASE("A layer resting on a pose stops re-applying it", "[state machine]")
{
    FrozenPoseScene scene;
    REQUIRE(rectangleColor(scene.artboard.get())->colorValue() == red);

    // The fade animates over the first frames; once it has finished, nothing
    // changes and every layer freezes.
    scene.advance(30);
    CHECK(scene.machine->isLayerFrozen(0));
    auto poses = StateMachineInstance::sm_frozenPoseAdvances;
    scene.advance(10);
    CHECK(StateMachineInstance::sm_frozenPoseAdvances > poses);
    CHECK(rectangleColor(scene.artboard.get())->colorValue() == red);
}

TEST_CASE("A layer holding a finished animation stops re-applying it",
          "[state machine]")
{
    FrozenPoseScene scene;
    scene.advance(30);
    REQUIRE(square(scene.artboard.get())->opacity() == 1.0f);
    CHECK(scene.machine->isLayerFrozen(2));

    // The host fades it out: that wakes the layer, and the next advance puts
    // the held value back, as it would have without freezing.
    square(scene.artboard.get())->opacity(0.25f);
    CHECK_FALSE(scene.machine->isLayerFrozen(2));
    scene.advance(1);
    CHECK(square(scene.artboard.get())->opacity() == 1.0f);

    scene.advance(2);
    CHECK(scene.machine->isLayerFrozen(2));
}

TEST_CASE("A write to the artboard makes a frozen pose re-apply",
          "[state machine]")
{
    FrozenPoseScene scene;
    scene.advance(30);

    // The host paints over the pose; the next advance puts it back, as it
    // would have without freezing.
    auto color = rectangleColor(scene.artboard.get());
    CHECK(scene.machine->isLayerFrozen(0));
    color->colorValue(colorARGB(255, 0, 255, 0));
    CHECK_FALSE(scene.machine->isLayerFrozen(0));
    scene.advance(1);
    CHECK(color->colorValue() == red);
}

TEST_CASE("Rotating the artboard itself makes a frozen pose re-apply",
          "[state machine]")
{
    FrozenPoseScene scene;
    scene.advance(30);
    CHECK(scene.machine->isLayerFrozen(0));

    // The artboard's own rotation reaches it through selfTransformChanged,
    // which leaves the rasters' content revision alone; the pose still has to
    // hear about it.
    scene.artboard->rotation(0.5f);
    CHECK_FALSE(scene.machine->isLayerFrozen(0));
    scene.advance(1);
    CHECK(scene.artboard->rotation() == 0.0f);
}

TEST_CASE("A frozen pose re-applies once a later layer stops overriding it",
          "[state machine]")
{
    FrozenPoseScene scene;
    scene.advance(30);
    auto flash = scene.viewModelInstance->propertyValue("flash")
                     ->as<ViewModelInstanceBoolean>();

    flash->propertyValue(true);
    scene.advance(1);
    CHECK(rectangleColor(scene.artboard.get())->colorValue() == blue);

    // The flash plays out and its layer exits: the pose it was overriding is
    // back, and freezes again.
    flash->propertyValue(false);
    scene.advance(30);
    CHECK(rectangleColor(scene.artboard.get())->colorValue() == red);
    CHECK(scene.machine->isLayerFrozen(0));
}

// frozen_pose_rows.riv: two list rows, each resting on a one-frame pose that
// paints its rectangle red and holds the row artboard's own rotation at 0.
TEST_CASE("Writes to a quiet list row bring its pose back", "[state machine]")
{
    auto file = ReadRiveFile("assets/frozen_pose_rows.riv");
    auto artboard = file->artboardNamed("rows_host");
    REQUIRE(artboard != nullptr);
    auto machine = artboard->defaultStateMachine();
    machine->bindViewModelInstance(
        file->createDefaultViewModelInstance(artboard.get()));
    machine->advanceAndApply(0.0f);
    auto lists = artboard->find<ArtboardComponentList>();
    REQUIRE(lists.size() == 1);
    auto list = lists[0];
    REQUIRE(list->artboardCount() == 2);

    // Both rows rest on their pose, so they go quiet.
    auto skips = ArtboardComponentList::sm_quietRowSkips;
    for (int i = 0; i < 30; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    CHECK(ArtboardComponentList::sm_quietRowSkips > skips);

    // A color change reaches the row through changed(), which raises no
    // component dirt; it still has to wake the row so the pose comes back.
    auto row = list->artboardInstance(0);
    auto rectangle = row->find<Shape>("rectangle");
    REQUIRE(rectangle != nullptr);
    auto color =
        rectangle->children()[1]->as<Fill>()->paint()->as<SolidColor>();
    REQUIRE(color->colorValue() == red);
    color->colorValue(colorARGB(255, 0, 255, 0));
    machine->advanceAndApply(1.0f / 60.0f);
    CHECK(color->colorValue() == red);

    // So does the row artboard's own rotation, through selfTransformChanged.
    for (int i = 0; i < 10; i++)
    {
        machine->advanceAndApply(1.0f / 60.0f);
    }
    row->rotation(0.5f);
    machine->advanceAndApply(1.0f / 60.0f);
    CHECK(row->rotation() == 0.0f);
}
