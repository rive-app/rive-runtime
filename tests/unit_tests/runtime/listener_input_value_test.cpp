#include "rive/animation/state_machine_instance.hpp"
#include "rive/file.hpp"
#include "rive/input/focus_manager.hpp"
#include "rive/input/gamepad_batch.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive/viewmodel/viewmodel_instance_boolean.hpp"
#include "rive/viewmodel/viewmodel_instance_number.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <cstring>
#include <vector>

using namespace rive;

// listener_input_values.riv (source: assets/rml/listener_input_values.rml):
// one full-artboard shape, focused on entry, with listeners whose
// ListenerViewModelChange actions write a value of the triggering input
// instead of a constant:
//   Pointer     (down)             px <- pointerX, py <- pointerY,
//                                  skipped <- keyPressed (never fed; its
//                                  constant true must not land),
//                                  pxText <- pointerX through To String
//   Stick       (axis 0, axis 1)   lx <- axis 0, ly <- axis 1,
//                                  mapped <- axis 0 through a -1..1 -> 0..100
//                                  range mapper
//   South       (button 0 down/up) southPressed, southValue,
//                                  southNumber <- pressed through To Number,
//                                  southRaw <- pressed with no converter
//                                  (starts at 5; nothing may be written)
//   Any gamepad (no filters)       changed <- the changed value
//   Key         (a, down/up)       keyDown <- keyPressed
//   Text        (text input)       typed <- text
//   Focus       (focus, blur)      focused <- focused
namespace
{
struct GamepadWire
{
    std::vector<uint8_t> buf;

    GamepadWire() { u32(kGamepadBatchWireVersion); }

    void u8(uint8_t v) { buf.push_back(v); }
    void u32(uint32_t v)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            buf.push_back(static_cast<uint8_t>(v >> shift));
        }
    }
    void f32(float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }

    void connected(int32_t deviceId, uint8_t nButtons, uint8_t nAxes)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::connected));
        u32(static_cast<uint32_t>(deviceId));
        u8(0); // standard mapping
        u8(nButtons);
        u8(nAxes);
        u8(0); // padding
        for (uint8_t i = 0; i < nButtons + nAxes; i++)
        {
            f32(0.f);
        }
    }

    void update(int32_t deviceId,
                GamepadInputChangeKind kind,
                uint8_t index,
                float value)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::update));
        u32(static_cast<uint32_t>(deviceId));
        u8(1); // nChanges
        u8(static_cast<uint8_t>(kind));
        u8(index);
        f32(value);
    }
};

struct InputScene
{
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    std::unique_ptr<StateMachineInstance> stateMachine;
    rcp<ViewModelInstance> vmi;

    InputScene()
    {
        file = ReadRiveFile("assets/listener_input_values.riv");
        artboard = file->artboardDefault();
        REQUIRE(artboard != nullptr);
        stateMachine = artboard->stateMachineAt(0);
        REQUIRE(stateMachine != nullptr);
        vmi = file->createDefaultViewModelInstance(artboard.get());
        REQUIRE(vmi != nullptr);
        stateMachine->bindViewModelInstance(vmi);
        advance();
        // The entry state focuses the pad; key, text and gamepad events only
        // reach listeners on the focused node.
        REQUIRE(artboard->focusManager() != nullptr);
        REQUIRE(artboard->focusManager()->primaryFocus() != nullptr);
    }

    void advance() { stateMachine->advanceAndApply(0.016f); }

    float number(const char* name)
    {
        auto value = vmi->propertyValue(name);
        REQUIRE(value != nullptr);
        return value->as<ViewModelInstanceNumber>()->propertyValue();
    }

    bool boolean(const char* name)
    {
        auto value = vmi->propertyValue(name);
        REQUIRE(value != nullptr);
        return value->as<ViewModelInstanceBoolean>()->propertyValue();
    }

    std::string string(const char* name)
    {
        auto value = vmi->propertyValue(name);
        REQUIRE(value != nullptr);
        return value->as<ViewModelInstanceString>()->propertyValue();
    }

    void submit(const GamepadWire& wire)
    {
        REQUIRE(stateMachine->submitGamepadsFromBuffer(wire.buf.data(),
                                                       wire.buf.size()));
        advance();
    }
};
} // namespace

TEST_CASE("pointer listeners write the pointer position", "[listener_input]")
{
    InputScene scene;
    scene.stateMachine->pointerDown(Vec2D(120.0f, 310.0f));
    scene.advance();

    CHECK(scene.number("px") == 120.0f);
    CHECK(scene.number("py") == 310.0f);
    // keyPressed can't be read from a pointer event, so that action is skipped
    // rather than writing its constant.
    CHECK(scene.boolean("skipped") == false);
}

TEST_CASE("gamepad axes read the full snapshot", "[listener_input]")
{
    InputScene scene;

    GamepadWire wire;
    wire.connected(0, 17, 4);
    wire.update(0, GamepadInputChangeKind::axis, 0, 0.5f);
    scene.submit(wire);
    CHECK(scene.number("lx") == 0.5f);
    CHECK(scene.number("ly") == 0.0f);
    CHECK(scene.number("mapped") == 75.0f);
    CHECK(scene.number("changed") == 0.5f);

    // Only axis 1 changes, but the axis 0 action still reads its value from
    // the snapshot instead of taking axis 1's.
    GamepadWire next;
    next.update(0, GamepadInputChangeKind::axis, 1, -0.25f);
    scene.submit(next);
    CHECK(scene.number("lx") == 0.5f);
    CHECK(scene.number("ly") == -0.25f);
    CHECK(scene.number("mapped") == 75.0f);
    CHECK(scene.number("changed") == -0.25f);
}

TEST_CASE("gamepad buttons write pressed state and value", "[listener_input]")
{
    InputScene scene;

    GamepadWire press;
    press.connected(0, 17, 4);
    press.update(0, GamepadInputChangeKind::button, 0, 1.0f);
    scene.submit(press);
    CHECK(scene.boolean("southPressed") == true);
    CHECK(scene.number("southValue") == 1.0f);

    GamepadWire release;
    release.update(0, GamepadInputChangeKind::button, 0, 0.0f);
    scene.submit(release);
    CHECK(scene.boolean("southPressed") == false);
    CHECK(scene.number("southValue") == 0.0f);
}

TEST_CASE("input values of another type go through the converter",
          "[listener_input]")
{
    InputScene scene;

    // The bindable property holds the input's type; its data bind's converter
    // maps it to the view model property's type.
    scene.stateMachine->pointerDown(Vec2D(120.4f, 310.0f));
    scene.advance();
    CHECK(scene.string("pxText") == "120");

    GamepadWire press;
    press.connected(0, 17, 4);
    press.update(0, GamepadInputChangeKind::button, 0, 1.0f);
    scene.submit(press);
    CHECK(scene.number("southNumber") == 1.0f);
    // Without a converter a boolean can't reach a number, so nothing lands.
    CHECK(scene.number("southRaw") == 5.0f);

    GamepadWire release;
    release.update(0, GamepadInputChangeKind::button, 0, 0.0f);
    scene.submit(release);
    CHECK(scene.number("southNumber") == 0.0f);
    CHECK(scene.number("southRaw") == 5.0f);
}

TEST_CASE("keyboard listeners write whether the key is down",
          "[listener_input]")
{
    InputScene scene;
    auto focusManager = scene.artboard->focusManager();

    focusManager->keyInput(Key::a, KeyModifiers::none, true, false);
    scene.advance();
    CHECK(scene.boolean("keyDown") == true);

    focusManager->keyInput(Key::a, KeyModifiers::none, false, false);
    scene.advance();
    CHECK(scene.boolean("keyDown") == false);
}

TEST_CASE("text input listeners write the committed text", "[listener_input]")
{
    InputScene scene;

    scene.artboard->focusManager()->textInput("hi");
    scene.advance();
    CHECK(scene.string("typed") == "hi");
}

TEST_CASE("focus listeners write the focus state", "[listener_input]")
{
    InputScene scene;
    // Focus events are queued and dispatched on the following advance, so the
    // entry state's focus reaches the listener one frame later.
    scene.advance();
    CHECK(scene.boolean("focused") == true);

    scene.artboard->focusManager()->clearFocus();
    scene.advance();
    CHECK(scene.boolean("focused") == false);

    scene.artboard->focusManager()->focusNext();
    scene.advance();
    CHECK(scene.boolean("focused") == true);
}
