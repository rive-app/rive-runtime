#include "rive/animation/state_machine_instance.hpp"
#include "rive/artboard.hpp"
#include "rive/file.hpp"
#include "rive/input/gamepad_batch.hpp"
#include "rive/input/gamepad_snapshot.hpp"
#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "utils/serializing_factory.hpp"
#include "rive/viewmodel/viewmodel_instance_string.hpp"
#include <cstdint>
#include <cstring>
#include <vector>

using namespace rive;

namespace
{
// Builds a little-endian gamepad batch matching the wire format documented in
// `gamepad_batch.cpp`. Multi-byte fields are fixed-width LE; counts must stay
// within `kGamepadBatchMax{Buttons,Axes}`.
struct WireBuilder
{
    std::vector<uint8_t> buf;

    void u8(uint8_t v) { buf.push_back(v); }
    void u32(uint32_t v)
    {
        buf.push_back(static_cast<uint8_t>(v));
        buf.push_back(static_cast<uint8_t>(v >> 8));
        buf.push_back(static_cast<uint8_t>(v >> 16));
        buf.push_back(static_cast<uint8_t>(v >> 24));
    }
    void i32(int32_t v) { u32(static_cast<uint32_t>(v)); }
    void f32(float v)
    {
        uint32_t bits;
        std::memcpy(&bits, &v, sizeof(bits));
        u32(bits);
    }

    void header() { u32(kGamepadBatchWireVersion); }

    void connected(int32_t deviceId,
                   uint8_t nButtons = 17,
                   uint8_t nAxes = 4,
                   uint8_t mapping = 0)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::connected));
        i32(deviceId);
        u8(mapping);
        u8(nButtons);
        u8(nAxes);
        u8(0); // padding to align the float arrays
        for (uint8_t b = 0; b < nButtons; b++)
        {
            f32(0.f);
        }
        for (uint8_t a = 0; a < nAxes; a++)
        {
            f32(0.f);
        }
    }

    void updateOne(int32_t deviceId,
                   GamepadInputChangeKind kind,
                   uint8_t index,
                   float value)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::update));
        i32(deviceId);
        u8(1); // nChanges
        u8(static_cast<uint8_t>(kind));
        u8(index);
        f32(value);
    }

    void disconnected(int32_t deviceId)
    {
        u8(static_cast<uint8_t>(GamepadRecordType::disconnected));
        i32(deviceId);
    }
};

// Open the shared gamepad fixture and advance once so the focus manager and
// scripted drawable list are initialized before any events are dispatched.
std::unique_ptr<StateMachineInstance> openReadyStateMachine(
    rcp<File>& fileOut,
    std::unique_ptr<ArtboardInstance>& artboardOut,
    SerializingFactory& silver)
{
    fileOut = ReadRiveFile("assets/gamepad_test.riv", &silver);
    artboardOut = fileOut->artboardDefault();
    REQUIRE(artboardOut != nullptr);

    auto sm = artboardOut->stateMachineAt(0);
    REQUIRE(sm != nullptr);

    auto vmi = fileOut->createDefaultViewModelInstance(artboardOut.get());
    sm->bindViewModelInstance(vmi);
    sm->advanceAndApply(0);
    return sm;
}
} // namespace

TEST_CASE("gamepad batch accepts a single connected record", "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    constexpr int32_t kDeviceId = 0;
    constexpr uint8_t kNButtons = 17;
    constexpr uint8_t kNAxes = 4;
    static_assert(kNButtons <= kGamepadBatchMaxButtons, "button cap");
    static_assert(kNAxes <= kGamepadBatchMaxAxes, "axis cap");

    WireBuilder wb;
    wb.header();
    wb.connected(kDeviceId, kNButtons, kNAxes);

    // Version (4) + record type (1) + deviceId (4) + 4 header bytes
    // + 17 button floats + 4 axis floats.
    REQUIRE(wb.buf.size() ==
            4u + 1u + 4u + 4u + size_t(kNButtons) * 4u + size_t(kNAxes) * 4u);

    CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(), wb.buf.size()));
}

TEST_CASE("gamepad batch tracks multiple device ids independently", "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    // Connect three devices with distinct ids (including a high id to make
    // sure the int32 path round-trips).
    WireBuilder connect;
    connect.header();
    connect.connected(/*deviceId*/ 1);
    connect.connected(/*deviceId*/ 7);
    connect.connected(/*deviceId*/ 42);
    CHECK(stateMachine->submitGamepadsFromBuffer(connect.buf.data(),
                                                 connect.buf.size()));

    // Send a separate batch per device, mixing button + axis updates.
    WireBuilder updates;
    updates.header();
    updates.updateOne(1, GamepadInputChangeKind::button, /*index*/ 0, 1.f);
    updates.updateOne(7, GamepadInputChangeKind::axis, /*index*/ 2, -0.5f);
    updates.updateOne(42, GamepadInputChangeKind::button, /*index*/ 4, 1.f);
    updates.updateOne(42, GamepadInputChangeKind::axis, /*index*/ 0, 0.75f);
    CHECK(stateMachine->submitGamepadsFromBuffer(updates.buf.data(),
                                                 updates.buf.size()));
}

TEST_CASE("gamepad batch rejects an update for an unknown device id",
          "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    WireBuilder wb;
    wb.header();
    wb.connected(/*deviceId*/ 3);
    // Update targets a deviceId we never connected — must bail out.
    wb.updateOne(/*deviceId*/ 99,
                 GamepadInputChangeKind::button,
                 /*index*/ 0,
                 1.f);

    CHECK_FALSE(
        stateMachine->submitGamepadsFromBuffer(wb.buf.data(), wb.buf.size()));
}

TEST_CASE("gamepad batch handles disconnect of one of several devices",
          "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    WireBuilder wb;
    wb.header();
    wb.connected(/*deviceId*/ 10);
    wb.connected(/*deviceId*/ 20);
    wb.connected(/*deviceId*/ 30);
    // Drive an update on each, disconnect the middle one, then drive the
    // surviving devices again to confirm they are still tracked.
    wb.updateOne(10, GamepadInputChangeKind::button, 0, 1.f);
    wb.updateOne(20, GamepadInputChangeKind::axis, 1, 0.25f);
    wb.updateOne(30, GamepadInputChangeKind::button, 2, 1.f);
    wb.disconnected(/*deviceId*/ 20);
    wb.updateOne(10, GamepadInputChangeKind::axis, 0, -1.f);
    wb.updateOne(30, GamepadInputChangeKind::button, 3, 0.f);

    CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(), wb.buf.size()));

    // Anything addressed to the now-disconnected device must be rejected.
    WireBuilder afterDisconnect;
    afterDisconnect.header();
    afterDisconnect.updateOne(/*deviceId*/ 20,
                              GamepadInputChangeKind::button,
                              0,
                              1.f);
    CHECK_FALSE(
        stateMachine->submitGamepadsFromBuffer(afterDisconnect.buf.data(),
                                               afterDisconnect.buf.size()));
}

TEST_CASE("gamepad batch allows reconnecting the same device id", "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    constexpr int32_t kDeviceId = 5;

    WireBuilder first;
    first.header();
    first.connected(kDeviceId);
    first.updateOne(kDeviceId, GamepadInputChangeKind::button, 0, 1.f);
    first.disconnected(kDeviceId);
    CHECK(stateMachine->submitGamepadsFromBuffer(first.buf.data(),
                                                 first.buf.size()));

    // After disconnect the device must be unknown again — any update before a
    // fresh connect is rejected.
    WireBuilder strayUpdate;
    strayUpdate.header();
    strayUpdate.updateOne(kDeviceId, GamepadInputChangeKind::button, 0, 1.f);
    CHECK_FALSE(stateMachine->submitGamepadsFromBuffer(strayUpdate.buf.data(),
                                                       strayUpdate.buf.size()));

    // Reconnect with the same id and confirm updates flow again.
    WireBuilder reconnect;
    reconnect.header();
    reconnect.connected(kDeviceId);
    reconnect.updateOne(kDeviceId, GamepadInputChangeKind::axis, 0, 0.5f);
    CHECK(stateMachine->submitGamepadsFromBuffer(reconnect.buf.data(),
                                                 reconnect.buf.size()));
}

TEST_CASE("gamepad batch tolerates disconnect of an unknown device id",
          "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    // Disconnect for a device we never connected is a no-op `erase` per the
    // wire-format contract — the batch must still parse successfully.
    WireBuilder wb;
    wb.header();
    wb.disconnected(/*deviceId*/ 1234);

    CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(), wb.buf.size()));
}

TEST_CASE("File loads and processes multiple types of gamepad inputs",
          "[gamepad]")
{
    SerializingFactory silver;
    rcp<File> file;
    std::unique_ptr<ArtboardInstance> artboard;
    auto stateMachine = openReadyStateMachine(file, artboard, silver);

    REQUIRE(artboard != nullptr);

    silver.frameSize(artboard->width(), artboard->height());

    stateMachine->advanceAndApply(0.1f);
    auto renderer = silver.makeRenderer();
    artboard->draw(renderer.get());
    silver.addFrame();

    constexpr int32_t kDeviceId1 = 3;
    constexpr int32_t kDeviceId2 = 5;
    constexpr int32_t kDeviceId3 = 1;

    // connect device 1
    {
        WireBuilder wb;
        wb.header();
        wb.connected(kDeviceId1);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    // press button at index 0 on device 1
    {

        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::button, 0, 1.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // connect device 2 and 3
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.connected(kDeviceId2);
        wb.connected(kDeviceId3,
                     17,
                     4,
                     1); // Device 3 is not a standard mapping
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // submit button press on device 2 and 3
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId2, GamepadInputChangeKind::button, 2, 1.f);
        wb.updateOne(kDeviceId3, GamepadInputChangeKind::button, 2, 1.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // submit axis change on device 1 and 2
    {

        for (auto i = 1; i < 10; i++)
        {
            silver.addFrame();
            WireBuilder wb;
            wb.header();
            wb.updateOne(kDeviceId1, GamepadInputChangeKind::axis, 0, i * 0.1f);
            wb.updateOne(kDeviceId2,
                         GamepadInputChangeKind::axis,
                         1,
                         i * -0.1f);
            CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                         wb.buf.size()));
            stateMachine->advanceAndApply(0.016f);
            artboard->draw(renderer.get());
        }
    }
    // disconnect device 3
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.disconnected(kDeviceId3);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // release button 0 of device 1
    {
        stateMachine->focusManager()->focusNext();
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::button, 0, 0.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // press button 0 of device 1
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::button, 0, 1.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // Release button 0
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::button, 0, 0.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::button, 1, 1.f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }
    // Move left joystick x and y axis
    {
        silver.addFrame();
        WireBuilder wb;
        wb.header();
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::axis, 0, 0.5f);
        wb.updateOne(kDeviceId1, GamepadInputChangeKind::axis, 1, 0.5f);
        CHECK(stateMachine->submitGamepadsFromBuffer(wb.buf.data(),
                                                     wb.buf.size()));
        stateMachine->advanceAndApply(0.016f);
        artboard->draw(renderer.get());
    }

    CHECK(silver.matches("gamepad_test"));
}