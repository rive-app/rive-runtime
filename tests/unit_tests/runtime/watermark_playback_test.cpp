#include <rive/animation/state_machine_instance.hpp>
#include <rive/artboard.hpp>
#include <rive/file.hpp>
#include <rive/node.hpp>
#include <rive/watermark.hpp>
#include <utils/no_op_renderer.hpp>

#include "catch.hpp"
#include "rive_file_reader.hpp"

#include <algorithm>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>

using namespace rive;

namespace
{
// The watermark refuses to advance faster than the wall clock unless the host
// owns the timeline. Tests that want to drive time themselves — every one below
// that counts frames — say so by taking this.
struct DeterministicTime
{
    DeterministicTime() { File::deterministicMode = true; }
    ~DeterministicTime() { File::deterministicMode = false; }
};

// Records which RenderPaths reached the renderer. Every ArtboardInstance builds
// its own paths, so path identity is what tells the watermark's artboard apart
// from the host's.
class PathRecordingRenderer : public NoOpRenderer
{
public:
    std::vector<RenderPath*> paths;
    void drawPath(RenderPath* path, RenderPaint*) override
    {
        paths.push_back(path);
    }

    bool sawAnyOf(const std::vector<RenderPath*>& others) const
    {
        for (auto path : paths)
        {
            if (std::find(others.begin(), others.end(), path) != others.end())
            {
                return true;
            }
        }
        return false;
    }
};

std::vector<RenderPath*> recordDraw(Artboard* artboard)
{
    PathRecordingRenderer renderer;
    artboard->draw(&renderer);
    return renderer.paths;
}

// drawInternal is never diverted, so this is how to see an artboard's own
// content even while a watermark is attached to it.
std::vector<RenderPath*> recordDrawInternal(Artboard* artboard)
{
    PathRecordingRenderer renderer;
    artboard->drawInternal(&renderer);
    return renderer.paths;
}

// The fixture's state machine tweens a node's x, so the node positions are a
// direct read on whether the artboard advanced.
std::vector<float> nodePositions(Artboard* artboard)
{
    std::vector<float> positions;
    for (auto node : artboard->find<Node>())
    {
        positions.push_back(node->x());
        positions.push_back(node->y());
    }
    return positions;
}

// Builds a watermark out of a second instance of the same artboard. The two
// instances render distinct paths, which is all the tests below need to tell
// which one drew.
std::unique_ptr<Watermark> makeWatermark(const File* file,
                                         std::vector<RenderPath*>* paths)
{
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    if (paths != nullptr)
    {
        *paths = recordDraw(artboard.get());
    }
    return std::make_unique<Watermark>(std::move(artboard),
                                       std::move(stateMachine));
}
} // namespace

TEST_CASE("a watermark holds back the artboard it is attached to",
          "[watermark]")
{
    DeterministicTime deterministic;
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);

    // Captured before the watermark is attached, while the host still draws
    // itself.
    auto hostPaths = recordDraw(artboard.get());
    REQUIRE(!hostPaths.empty());
    auto frozenPositions = nodePositions(artboard.get());
    REQUIRE(!frozenPositions.empty());

    std::vector<RenderPath*> watermarkPaths;
    artboard->watermark(makeWatermark(file.get(), &watermarkPaths));
    REQUIRE(artboard->watermark() != nullptr);
    REQUIRE(!watermarkPaths.empty());

    for (int i = 0; i < 10; i++)
    {
        // Never reports "settled" while the pre-roll runs: a false here would
        // stop the host's ticker mid watermark.
        REQUIRE(stateMachine->advanceAndApply(0.016f));
        REQUIRE(artboard->watermark() != nullptr);
        CHECK(artboard->watermark()->isPlaying());

        PathRecordingRenderer renderer;
        artboard->draw(&renderer);
        CHECK(!renderer.paths.empty());
        CHECK(!renderer.sawAnyOf(hostPaths));
        CHECK(nodePositions(artboard.get()) == frozenPositions);
    }
}

TEST_CASE("the artboard takes over once the watermark is done", "[watermark]")
{
    DeterministicTime deterministic;
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");

    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);

    auto hostPaths = recordDraw(artboard.get());
    auto frozenPositions = nodePositions(artboard.get());
    artboard->watermark(makeWatermark(file.get(), nullptr));

    // The same artboard played with no watermark at all, to compare against.
    auto control = file->artboardDefault();
    auto controlStateMachine = control->stateMachineAt(0);
    REQUIRE(controlStateMachine != nullptr);
    controlStateMachine->advanceAndApply(0.0f);

    // Long enough to outrun the pre-roll's hard time cap even if the
    // watermark's state machine never settles on its own.
    for (int i = 0; i < 800 && artboard->watermark() != nullptr; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    // Released the moment it finishes, so the pre-roll can't play twice.
    REQUIRE(artboard->watermark() == nullptr);

    // Once the pre-roll is out of the way the artboard plays exactly what it
    // would have played without one: same frames, just delayed. The frame that
    // released the watermark is already the artboard's first real one.
    int hostFrames = 1;
    for (int i = 0; i < 60; i++)
    {
        stateMachine->advanceAndApply(0.016f);
        hostFrames++;
    }
    for (int i = 0; i < hostFrames; i++)
    {
        controlStateMachine->advanceAndApply(0.016f);
    }
    auto controlPositions = nodePositions(control.get());
    REQUIRE(controlPositions != frozenPositions); // the fixture does animate
    CHECK(nodePositions(artboard.get()) == controlPositions);

    PathRecordingRenderer renderer;
    artboard->draw(&renderer);
    CHECK(renderer.sawAnyOf(hostPaths));
}

TEST_CASE("a watermark that is never advanced does not hide the artboard",
          "[watermark]")
{
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");

    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);

    auto hostPaths = recordDraw(artboard.get());
    artboard->watermark(makeWatermark(file.get(), nullptr));

    // Playback that never runs through a state machine never starts the
    // pre-roll, and must keep drawing the artboard rather than freeze on a
    // watermark that would never end.
    PathRecordingRenderer renderer;
    artboard->draw(&renderer);
    CHECK(renderer.sawAnyOf(hostPaths));
}

TEST_CASE("drawInternal never consults the watermark", "[watermark]")
{
    DeterministicTime deterministic;
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");

    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    auto hostPaths = recordDraw(artboard.get());
    REQUIRE(!hostPaths.empty());

    std::vector<RenderPath*> watermarkPaths;
    artboard->watermark(makeWatermark(file.get(), &watermarkPaths));
    REQUIRE(stateMachine->advanceAndApply(0.016f));
    REQUIRE(artboard->watermark()->isPlaying());

    // Nested artboards and component lists render through drawInternal, which
    // is why the gate lives in draw(): drawing this way must still render the
    // artboard's own content even mid pre-roll.
    PathRecordingRenderer renderer;
    artboard->drawInternal(&renderer);
    CHECK(renderer.sawAnyOf(hostPaths));
    CHECK(!renderer.sawAnyOf(watermarkPaths));
}

TEST_CASE("a watermarked file pre-rolls the artboard it hands out",
          "[watermark]")
{
    DeterministicTime deterministic;
    // created by watermark_export_test.dart in rive_core.
    auto file = ReadRiveFile("assets/watermark_playback_test.riv");
    auto* manifest = file->manifest();
    REQUIRE(manifest != nullptr);
    REQUIRE(manifest->hasWatermark());
    // The exporter inlines the watermark after the host's artboards, and drags
    // in whatever that watermark itself nests, so the total count follows the
    // bundled artwork rather than being fixed. What this test needs is that the
    // host kept index 0 and the watermark landed right after it.
    REQUIRE(file->artboardCount() > 1);
    REQUIRE(manifest->watermarkArtboardIndex() == 1);

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    REQUIRE(artboard->watermark() != nullptr);
    // The watermark artboard itself is still vended plain, so tooling can look
    // at it without triggering a pre-roll of itself.
    CHECK(file->artboardAt(1)->watermark() == nullptr);

    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    auto hostPaths = recordDrawInternal(artboard.get());
    REQUIRE(!hostPaths.empty());

    REQUIRE(stateMachine->advanceAndApply(0.016f));
    REQUIRE(artboard->watermark()->isPlaying());
    PathRecordingRenderer duringPreRoll;
    artboard->draw(&duringPreRoll);
    CHECK(!duringPreRoll.paths.empty());
    CHECK(!duringPreRoll.sawAnyOf(hostPaths));

    // The bundled watermark rests in a one-shot animation, so its state machine
    // settles well inside the pre-roll's hard time cap.
    for (int i = 0; i < 800 && artboard->watermark() != nullptr; i++)
    {
        stateMachine->advanceAndApply(0.016f);
    }
    REQUIRE(artboard->watermark() == nullptr);

    PathRecordingRenderer afterPreRoll;
    artboard->draw(&afterPreRoll);
    CHECK(afterPreRoll.sawAnyOf(hostPaths));
}

TEST_CASE("the watermark cannot be advanced faster than real time",
          "[watermark]")
{
    // Deliberately not DeterministicTime: this is the real-clock behaviour.
    REQUIRE(!File::deterministicMode);

    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    artboard->watermark(makeWatermark(file.get(), nullptr));

    // One enormous delta is the obvious way to seek past a pre-roll. It buys
    // only the wall time that has actually passed, which here is ~nothing.
    REQUIRE(stateMachine->advanceAndApply(1000.0f));
    CHECK(artboard->watermark() != nullptr);

    // Neither does spinning: this loop is worth minutes of animation at face
    // value but runs in well under a frame of real time.
    for (int i = 0; i < 500; i++)
    {
        stateMachine->advanceAndApply(0.5f);
    }
    CHECK(artboard->watermark() != nullptr);
    CHECK(artboard->watermark()->isPlaying());

    // Nor does spinning *underneath* the clock tolerance, which is the subtle
    // way through: while each delta was compared against the wall clock on its
    // own, a delta at or just below the tolerance was never clamped, and the
    // tolerance could be re-spent every call. This loop is worth far more than
    // the pre-roll's hard cap at face value and still costs no real time.
    for (int i = 0; i < 8000; i++)
    {
        stateMachine->advanceAndApply(0.004f);
    }
    CHECK(artboard->watermark() != nullptr);
    CHECK(artboard->watermark()->isPlaying());

    // And the artboard is still being held back, not quietly playing behind it.
    PathRecordingRenderer renderer;
    artboard->draw(&renderer);
    CHECK(!renderer.paths.empty());
    CHECK(!renderer.sawAnyOf(recordDrawInternal(artboard.get())));
}

TEST_CASE("a file without a watermark section vends plain artboards",
          "[watermark]")
{
    // This file carries a manifest (it has a string table) but no watermark
    // section, which is the shape of every file exported today.
    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto* manifest = file->manifest();
    REQUIRE((manifest == nullptr || !manifest->hasWatermark()));

    auto artboard = file->artboardDefault();
    REQUIRE(artboard != nullptr);
    CHECK(artboard->watermark() == nullptr);

    for (size_t i = 0; i < file->artboardCount(); i++)
    {
        CHECK(file->artboardAt(i)->watermark() == nullptr);
    }
}

// Idle time used to be bankable. The whole wall-clock gap went into the
// budget, so an instance left alone -- a backgrounded tab, a file loaded and
// not yet advanced -- funded a burst of advances that could drain the pre-roll
// a frame at a time without ever showing it. The credit for one call is now
// capped at the same ceiling a single advance may spend.
TEST_CASE("idle time cannot be banked to drain the watermark", "[watermark]")
{
    // Deliberately not DeterministicTime: this is the real-clock behaviour.
    REQUIRE(!File::deterministicMode);

    auto file = ReadRiveFile("assets/data_bind_keyframes_test.riv");
    auto artboard = file->artboardDefault();
    auto stateMachine = artboard->stateMachineAt(0);
    REQUIRE(stateMachine != nullptr);
    stateMachine->advanceAndApply(0.0f);
    artboard->watermark(makeWatermark(file.get(), nullptr));

    // Sit idle for well over one frame's worth of real time...
    std::this_thread::sleep_for(std::chrono::milliseconds(400));

    // ...then spend it as fast as possible. Every one of these is capped to a
    // frame, so banking the full gap would have let the burst consume all of
    // it rather than the one frame that has been earned.
    for (int i = 0; i < 20; i++)
    {
        stateMachine->advanceAndApply(1000.0f);
    }

    REQUIRE(artboard->watermark() != nullptr);
    CHECK(artboard->watermark()->isPlaying());
    // One frame's credit plus the tolerance, not the whole idle gap. The bound
    // is deliberately loose: what matters is that it is nowhere near 0.4.
    CHECK(artboard->watermark()->elapsedSeconds() < 0.3f);
}
