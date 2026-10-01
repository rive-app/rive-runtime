#include <rive/animation/listener_types/listener_input_type.hpp>
#include <rive/animation/listener_types/listener_input_type_pointer_button.hpp>
#include <rive/animation/state_machine_instance.hpp>
#include <rive/animation/state_machine_listener.hpp>
#include <rive/animation/linear_animation_instance.hpp>
#include <rive/animation/nested_state_machine.hpp>
#include <rive/animation/state_machine_input_instance.hpp>
#include <rive/constraints/scrolling/scroll_constraint.hpp>
#include <rive/nested_artboard.hpp>
#include <rive/pointer_button.hpp>
#include <rive/text/text_input.hpp>
#include <rive/viewmodel/viewmodel_instance_number.hpp>
#include "rive_file_reader.hpp"

#include <catch.hpp>

using namespace rive;

namespace
{
// click_event's "art-1" has two 200x200 rects under one group with a single
// legacy (StateMachineListenerSingle) click listener that reports an event.
struct ClickScene
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;

    ClickScene()
    {
        file = ReadRiveFile("assets/click_event.riv");
        auto sourceArtboard = file->artboard("art-1");
        auto machine = sourceArtboard->stateMachine("sm-1");
        REQUIRE(machine != nullptr);
        artboard = sourceArtboard->instance();
        stateMachine =
            std::make_unique<StateMachineInstance>(machine, artboard.get());
        stateMachine->advance(0.0f);
        artboard->advance(0.0f);
        stateMachine->advance(0.0f);
        REQUIRE(stateMachine->hitComponentsCount() == 2);
        REQUIRE(stateMachine->reportedEventCount() == 0);
    }

    void down(PointerButton button, Vec2D position = Vec2D(75.0f, 75.0f))
    {
        stateMachine->pointerDown(position, 0, button);
    }

    void up(PointerButton button, Vec2D position = Vec2D(75.0f, 75.0f))
    {
        stateMachine->pointerUp(position, 0, button);
    }

    size_t events() const { return stateMachine->reportedEventCount(); }
};

// listener_input_values (see assets/rml/listener_input_values.rml) has a
// full-artboard shape whose first listener is a ListenerInputType "down" that
// writes the pointer position into the px/py view model numbers. Passing a
// button swaps that input for a ListenerInputTypePointerButton bound to it.
struct DownScene
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
    rcp<ViewModelInstance> vmi;

    using Input = std::pair<ListenerType, PointerButton>;

    DownScene(std::vector<Input> inputs = {})
    {
        file = ReadRiveFile("assets/listener_input_values.riv");
        auto machine = file->artboard()->stateMachine(0);
        REQUIRE(machine != nullptr);
        REQUIRE(machine->listenerCount() > 0);
        auto listener = machine->listener(0);
        REQUIRE(listener->listenerInputTypeCount() == 1);
        auto inputType = listener->listenerInputType(0);
        REQUIRE(inputType->listenerTypeValue() == (uint32_t)ListenerType::down);
        REQUIRE(inputType->pointerButton() == PointerButton::primary);
        for (size_t i = 0; i < inputs.size(); i++)
        {
            auto buttonInput =
                std::make_unique<ListenerInputTypePointerButton>();
            buttonInput->listenerTypeValue((uint32_t)inputs[i].first);
            buttonInput->pointerButtonValue((uint8_t)inputs[i].second);
            auto mutableListener = const_cast<StateMachineListener*>(listener);
            if (i == 0)
            {
                mutableListener->replaceListenerInputTypeForTesting(
                    0,
                    std::move(buttonInput));
            }
            else
            {
                mutableListener->addListenerInputTypeForTesting(
                    std::move(buttonInput));
            }
            REQUIRE(listener->listenerInputType(i)->pointerButton() ==
                    inputs[i].second);
        }

        artboard = file->artboardDefault();
        stateMachine = artboard->stateMachineAt(0);
        vmi = file->createDefaultViewModelInstance(artboard.get());
        REQUIRE(vmi != nullptr);
        stateMachine->bindViewModelInstance(vmi);
        advance();
    }

    void advance() { stateMachine->advanceAndApply(0.016f); }

    void down(PointerButton button, Vec2D position)
    {
        stateMachine->pointerDown(position, 0, button);
        advance();
    }

    void move(Vec2D position)
    {
        stateMachine->pointerMove(position);
        advance();
    }

    void up(PointerButton button, Vec2D position)
    {
        stateMachine->pointerUp(position, 0, button);
        advance();
    }

    float px()
    {
        auto value = vmi->propertyValue("px");
        REQUIRE(value != nullptr);
        return value->as<ViewModelInstanceNumber>()->propertyValue();
    }
};
} // namespace

TEST_CASE("plain listener input types are primary button listeners",
          "[pointer_button]")
{
    auto file = ReadRiveFile("assets/listener_input_values.riv");
    auto machine = file->artboard()->stateMachine(0);
    auto listener = machine->listener(0);
    REQUIRE(listener->listenerInputTypeCount() == 1);
    REQUIRE(listener->listenerInputType(0)->pointerButton() ==
            PointerButton::primary);
    REQUIRE(listener->hasListener(ListenerType::down));
    REQUIRE(listener->hasListener(ListenerType::down, PointerButton::primary));
    REQUIRE_FALSE(
        listener->hasListener(ListenerType::down, PointerButton::secondary));
    REQUIRE_FALSE(
        listener->hasListener(ListenerType::down, PointerButton::middle));
}

TEST_CASE("pointer button input types listen to their button only",
          "[pointer_button]")
{
    ListenerInputTypePointerButton input;
    input.listenerTypeValue((uint32_t)ListenerType::click);
    REQUIRE(input.is<ListenerInputType>());
    REQUIRE(input.pointerButton() == PointerButton::primary);
    input.pointerButtonValue((uint8_t)PointerButton::secondary);
    REQUIRE(input.pointerButton() == PointerButton::secondary);
    input.pointerButtonValue((uint8_t)PointerButton::middle);
    REQUIRE(input.pointerButton() == PointerButton::middle);
}

TEST_CASE("legacy single listeners only respond to the primary button",
          "[pointer_button]")
{
    auto file = ReadRiveFile("assets/click_event.riv");
    auto machine = file->artboard("art-1")->stateMachine("sm-1");
    auto listener = machine->listener(0);
    REQUIRE(listener->listenerInputTypeCount() == 0);
    REQUIRE(listener->hasListener(ListenerType::click));
    REQUIRE(listener->hasListener(ListenerType::click, PointerButton::primary));
    REQUIRE_FALSE(
        listener->hasListener(ListenerType::click, PointerButton::secondary));
}

TEST_CASE("primary down listener ignores other buttons", "[pointer_button]")
{
    DownScene scene;
    REQUIRE(scene.px() == 0.0f);

    scene.down(PointerButton::secondary, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 0.0f);

    scene.down(PointerButton::middle, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 0.0f);

    scene.down(PointerButton::primary, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 120.0f);
}

TEST_CASE("secondary down listener fires only on the secondary button",
          "[pointer_button]")
{
    DownScene scene({{ListenerType::down, PointerButton::secondary}});
    REQUIRE(scene.px() == 0.0f);

    scene.down(PointerButton::primary, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 0.0f);

    scene.down(PointerButton::middle, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 0.0f);

    scene.down(PointerButton::secondary, Vec2D(120.0f, 310.0f));
    CHECK(scene.px() == 120.0f);

    scene.down(PointerButton::secondary, Vec2D(200.0f, 310.0f));
    CHECK(scene.px() == 200.0f);
}

TEST_CASE("listensToButtonAt reports the buttons wanted under a point",
          "[pointer_button]")
{
    Vec2D inside(120.0f, 310.0f);
    Vec2D outside(-50.0f, -50.0f);

    DownScene primary;
    CHECK(primary.stateMachine->listensToButtonAt(inside,
                                                  PointerButton::primary));
    CHECK_FALSE(
        primary.stateMachine->listensToButtonAt(inside,
                                                PointerButton::secondary));
    CHECK_FALSE(
        primary.stateMachine->listensToButtonAt(outside,
                                                PointerButton::primary));

    DownScene scene({{ListenerType::down, PointerButton::secondary}});
    CHECK(scene.stateMachine->listensToButtonAt(inside,
                                                PointerButton::secondary));
    CHECK_FALSE(
        scene.stateMachine->listensToButtonAt(inside, PointerButton::primary));
    CHECK_FALSE(
        scene.stateMachine->listensToButtonAt(inside, PointerButton::middle));
    CHECK_FALSE(
        scene.stateMachine->listensToButtonAt(outside,
                                              PointerButton::secondary));

    ClickScene legacy;
    CHECK(legacy.stateMachine->listensToButtonAt(Vec2D(75.0f, 75.0f),
                                                 PointerButton::primary));
    CHECK_FALSE(
        legacy.stateMachine->listensToButtonAt(Vec2D(75.0f, 75.0f),
                                               PointerButton::secondary));
    CHECK_FALSE(legacy.stateMachine->listensToButtonAt(Vec2D(300.0f, 75.0f),
                                                       PointerButton::primary));
}

TEST_CASE("synthetic drag events carry the dragging button", "[pointer_button]")
{
    Vec2D start(100.0f, 100.0f);
    Vec2D dragged(150.0f, 150.0f);
    Vec2D released(200.0f, 200.0f);

    SECTION("a secondary drag end reaches a secondary drag end listener")
    {
        DownScene scene({{ListenerType::drag, PointerButton::secondary},
                         {ListenerType::dragEnd, PointerButton::secondary}});
        scene.down(PointerButton::secondary, start);
        CHECK(scene.px() == 0.0f);
        scene.move(dragged);
        CHECK(scene.px() == 150.0f);
        scene.up(PointerButton::secondary, released);
        CHECK(scene.px() == 200.0f);
    }

    SECTION("a secondary drag end skips a primary drag end listener")
    {
        DownScene scene({{ListenerType::drag, PointerButton::secondary},
                         {ListenerType::dragEnd, PointerButton::primary}});
        scene.down(PointerButton::secondary, start);
        scene.move(dragged);
        CHECK(scene.px() == 150.0f);
        scene.up(PointerButton::secondary, released);
        CHECK(scene.px() == 150.0f);
    }

    SECTION("a secondary drag start reaches a secondary drag start listener")
    {
        DownScene scene({{ListenerType::drag, PointerButton::secondary},
                         {ListenerType::dragStart, PointerButton::secondary}});
        scene.down(PointerButton::secondary, start);
        scene.move(dragged);
        CHECK(scene.px() == 150.0f);
        scene.up(PointerButton::secondary, released);
        CHECK(scene.px() == 150.0f);
    }

    SECTION("a primary drag still drives primary drag listeners")
    {
        DownScene scene({{ListenerType::drag, PointerButton::primary},
                         {ListenerType::dragEnd, PointerButton::primary}});
        scene.down(PointerButton::primary, start);
        scene.move(dragged);
        CHECK(scene.px() == 150.0f);
        scene.up(PointerButton::primary, released);
        CHECK(scene.px() == 200.0f);
    }

    SECTION("a primary drag ignores secondary drag listeners")
    {
        DownScene scene({{ListenerType::drag, PointerButton::secondary},
                         {ListenerType::dragEnd, PointerButton::secondary}});
        scene.down(PointerButton::primary, start);
        scene.move(dragged);
        scene.up(PointerButton::primary, released);
        CHECK(scene.px() == 0.0f);
    }
}

TEST_CASE("primary click listener ignores other buttons", "[pointer_button]")
{
    ClickScene scene;

    scene.down(PointerButton::primary);
    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);

    scene.down(PointerButton::secondary);
    scene.up(PointerButton::secondary);
    REQUIRE(scene.events() == 1);

    scene.down(PointerButton::middle);
    scene.up(PointerButton::middle);
    REQUIRE(scene.events() == 1);

    scene.down(PointerButton::primary);
    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 2);
}

TEST_CASE("a click needs the same button down and up", "[pointer_button]")
{
    ClickScene scene;

    scene.down(PointerButton::primary);
    scene.up(PointerButton::secondary);
    REQUIRE(scene.events() == 0);

    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);

    scene.down(PointerButton::secondary);
    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);
}

TEST_CASE("a held press survives another button pressed over it",
          "[pointer_button]")
{
    ClickScene scene;

    scene.down(PointerButton::primary);
    scene.down(PointerButton::secondary);
    scene.up(PointerButton::secondary);
    REQUIRE(scene.events() == 0);

    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);
}

TEST_CASE("releasing another button outside keeps the held press",
          "[pointer_button]")
{
    ClickScene scene;

    scene.down(PointerButton::primary);
    scene.up(PointerButton::secondary, Vec2D(300.0f, 75.0f));
    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);

    scene.down(PointerButton::primary);
    scene.up(PointerButton::primary, Vec2D(300.0f, 75.0f));
    scene.up(PointerButton::primary);
    REQUIRE(scene.events() == 1);
}

// pointer_button_secondary.riv is exported by rive_core's
// pointer_button_export_test.dart: a full-artboard shape with a secondary click
// listener that reports "Boom" and sets the "clicked" bool input.
TEST_CASE("an exported secondary click fixture loads as a pointer button input",
          "[pointer_button]")
{
    auto file = ReadRiveFile("assets/pointer_button_secondary.riv");
    auto machine = file->artboard()->stateMachine(0);
    REQUIRE(machine != nullptr);
    REQUIRE(machine->listenerCount() == 1);
    auto listener = machine->listener(0);
    REQUIRE(listener->listenerInputTypeCount() == 1);
    auto input = listener->listenerInputType(0);
    REQUIRE(input->is<ListenerInputTypePointerButton>());
    REQUIRE(input->pointerButton() == PointerButton::secondary);
    REQUIRE(input->listenerTypeValue() == (uint32_t)ListenerType::click);

    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.0f);
    auto clicked = stateMachine->getBool("clicked");
    REQUIRE(clicked != nullptr);
    REQUIRE(clicked->value() == false);

    Vec2D center(250.0f, 250.0f);
    stateMachine->pointerDown(center, 0, PointerButton::primary);
    stateMachine->pointerUp(center, 0, PointerButton::primary);
    CHECK(stateMachine->reportedEventCount() == 0);
    CHECK(clicked->value() == false);

    stateMachine->pointerDown(center, 0, PointerButton::secondary);
    stateMachine->pointerUp(center, 0, PointerButton::secondary);
    CHECK(stateMachine->reportedEventCount() == 1);
    CHECK(clicked->value() == true);
}

// pointer_button_nested.riv is exported by the same Dart test: a host artboard
// holding "Inner" at (100, 100), whose full shape has a secondary click
// listener ("Boom") and a secondary drag + dragEnd listener ("Drag").
TEST_CASE("nested pointer button listeners dispatch through the host",
          "[pointer_button]")
{
    auto file = ReadRiveFile("assets/pointer_button_nested.riv");
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    stateMachine->advanceAndApply(0.0f);

    auto nestedArtboards = artboard->find<NestedArtboard>();
    REQUIRE(nestedArtboards.size() == 1);
    auto nestedArtboard = nestedArtboards[0];
    REQUIRE(nestedArtboard->nestedAnimations().size() == 1);
    auto inner = nestedArtboard->nestedAnimations()[0]
                     ->as<NestedStateMachine>()
                     ->stateMachineInstance();
    REQUIRE(inner != nullptr);

    Vec2D overInner(200.0f, 200.0f);
    Vec2D besideInner(20.0f, 20.0f);
    CHECK(stateMachine->listensToButtonAt(overInner, PointerButton::secondary));
    CHECK_FALSE(
        stateMachine->listensToButtonAt(overInner, PointerButton::primary));
    CHECK_FALSE(
        stateMachine->listensToButtonAt(besideInner, PointerButton::secondary));

    stateMachine->pointerDown(overInner, 0, PointerButton::primary);
    stateMachine->pointerUp(overInner, 0, PointerButton::primary);
    CHECK(inner->reportedEventCount() == 0);

    stateMachine->pointerDown(overInner, 0, PointerButton::secondary);
    stateMachine->pointerUp(overInner, 0, PointerButton::secondary);
    CHECK(inner->reportedEventCount() == 1);
    stateMachine->advanceAndApply(0.016f);

    SECTION("a secondary drag reaches the nested drag listeners")
    {
        stateMachine->pointerDown(Vec2D(150.0f, 150.0f),
                                  0,
                                  PointerButton::secondary);
        stateMachine->pointerMove(Vec2D(170.0f, 170.0f));
        auto afterMove = inner->reportedEventCount();
        CHECK(afterMove >= 1);
        stateMachine->pointerUp(Vec2D(180.0f, 180.0f),
                                0,
                                PointerButton::secondary);
        CHECK(inner->reportedEventCount() > afterMove);
    }

    SECTION("a primary drag leaves the nested secondary drag listeners alone")
    {
        stateMachine->pointerDown(Vec2D(150.0f, 150.0f),
                                  0,
                                  PointerButton::primary);
        stateMachine->pointerMove(Vec2D(170.0f, 170.0f));
        stateMachine->pointerUp(Vec2D(180.0f, 180.0f),
                                0,
                                PointerButton::primary);
        CHECK(inner->reportedEventCount() == 0);
    }
}

TEST_CASE("scroll views only drag with the primary button", "[pointer_button]")
{
    auto file = ReadRiveFile("assets/layout/layout_scroll_vertical.riv");
    auto artboard = file->artboard();
    auto artboardInstance = artboard->instance();
    auto machine = artboard->stateMachine("State Machine 1");
    REQUIRE(machine != nullptr);
    StateMachineInstance smi(machine, artboardInstance.get());
    auto scroll = artboardInstance->find<ScrollConstraint>()[0];
    smi.advanceAndApply(0.0f);

    Vec2D over(artboardInstance->width() / 2.0f,
               artboardInstance->height() / 2.0f);
    Vec2D dragged(over.x, over.y - 50.0f);

    smi.pointerDown(over, 0, PointerButton::secondary);
    smi.pointerMove(dragged);
    smi.pointerUp(dragged, 0, PointerButton::secondary);
    smi.advanceAndApply(0.0f);
    CHECK(scroll->offsetY() == 0.0f);

    smi.pointerDown(over, 0, PointerButton::primary);
    smi.pointerMove(dragged);
    smi.advanceAndApply(0.0f);
    CHECK(scroll->offsetY() != 0.0f);
    smi.pointerUp(dragged, 0, PointerButton::primary);
}

TEST_CASE("text inputs only select with the primary button", "[pointer_button]")
{
    auto file = ReadRiveFile("assets/text_input.riv");
    auto artboard = file->artboardNamed("Text Input - Multiline");
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    auto textInput = artboard->objects<TextInput>().first();
    REQUIRE(textInput != nullptr);
    textInput->rawTextInput()->text("hello world");
    stateMachine->advanceAndApply(0.0f);

    Vec2D press(8.0f, 8.0f);
    stateMachine->pointerDown(press, 0, PointerButton::secondary);
    CHECK(textInput->isDragging() == false);
    stateMachine->pointerUp(press, 0, PointerButton::secondary);

    stateMachine->pointerDown(press, 0, PointerButton::primary);
    CHECK(textInput->isDragging() == true);
    stateMachine->pointerUp(press, 0, PointerButton::primary);
}

TEST_CASE("scenes without listeners ignore pointer buttons", "[pointer_button]")
{
    auto file = ReadRiveFile("assets/listener_input_values.riv");
    auto artboard = file->artboardDefault();
    REQUIRE(artboard->animationCount() > 0);
    auto animation = artboard->animationAt(0);
    Scene* scene = animation.get();
    CHECK(scene->pointerDown(Vec2D(1.0f, 1.0f), 0, PointerButton::secondary) ==
          HitResult::none);
    CHECK(scene->pointerUp(Vec2D(1.0f, 1.0f), 0, PointerButton::secondary) ==
          HitResult::none);
}
