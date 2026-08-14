/*
 * Copyright 2026 Rive
 */

// ForeignImageRegistry is the cross session image route. A RenderImage the
// session did not decode is not in its id space, so the registry retains the
// object and the frame snapshot carries the rcp: whoever replays resolves the
// image itself rather than an id some other table has to agree about.
//
// These cases drive it the way a host does, through DeferredRenderer::drawImage
// on a session's own recorder, and read the resolved object back off replay.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "deferred_test_sink.hpp"

#include <catch.hpp>

using namespace rive;

namespace
{
// A RenderImage nothing decoded through a session, which is what makes it
// foreign: lite_rtti_cast to DeferredRenderImage fails and the recorder falls
// through to the registry.
class ForeignImage : public RenderImage
{
public:
    ForeignImage(int tag, bool* destroyed) : m_tag(tag), m_destroyed(destroyed)
    {
        m_Width = 4;
        m_Height = 4;
    }

    ~ForeignImage() override
    {
        if (m_destroyed != nullptr)
        {
            *m_destroyed = true;
        }
    }

    int tag() const { return m_tag; }

private:
    int m_tag;
    bool* m_destroyed;
};

// Records the image object each replayed draw resolved to, which is the only
// way to tell a right resolution from a wrong one that also draws.
class ImageRecorder : public Renderer
{
public:
    std::vector<const RenderImage*> drawn;

    void drawImage(const RenderImage* image,
                   ImageSampler,
                   BlendMode,
                   float) override
    {
        drawn.push_back(image);
    }

    void save() override {}
    void restore() override {}
    void transform(const Mat2D&) override {}
    void drawPath(RenderPath*, RenderPaint*) override {}
    void clipPath(RenderPath*) override {}
    void drawImageMesh(const RenderImage* image,
                       ImageSampler,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       rcp<RenderBuffer>,
                       uint32_t,
                       uint32_t,
                       BlendMode,
                       float) override
    {
        drawn.push_back(image);
    }
    void modulateOpacity(float) override {}
};

class ImageSink : public deferred_test::TestSink
{
public:
    ImageRecorder recorder;

    Renderer* beginScreenFrame(uint64_t) override { return &recorder; }
};

void drawForeign(cmd::DeferredSession& session, RenderImage* image)
{
    session.screenRenderer()->drawImage(image,
                                        ImageSampler::LinearClamp(),
                                        BlendMode::srcOver,
                                        1.0f);
}

// Replays one recorded frame and reports what its draws resolved to.
std::vector<const RenderImage*> replayed(const cmd::DeferredFrame& frame)
{
    ImageSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    CHECK(replayer.droppedDraws() == 0);
    return sink.recorder.drawn;
}

// Same, for the inline form, which resolves against the live registry instead
// of the snapshot's copy. Both routes are shipped, so both are covered.
std::vector<const RenderImage*> replayedInline(cmd::DeferredSession& session)
{
    ImageSink sink;
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(session, sink);
    CHECK(replayer.droppedDraws() == 0);
    return sink.recorder.drawn;
}
} // namespace

TEST_CASE("a foreign image resolves in a session that never decoded it",
          "[deferred][foreign_image]")
{
    bool destroyed = false;
    rcp<ForeignImage> image(new ForeignImage(1, &destroyed));

    // Two sessions with nothing shared between them: separate id spaces,
    // separate registries, separate streams.
    cmd::DeferredSession first(rive::ore::ReplayCaps{});
    cmd::DeferredSession second(rive::ore::ReplayCaps{});

    drawForeign(first, image.get());
    auto firstDrawn = replayed(cmd::takeFrame(first));

    drawForeign(second, image.get());
    auto secondLive = replayedInline(second);
    auto secondDrawn = replayed(cmd::takeFrame(second));

    REQUIRE(firstDrawn.size() == 1);
    REQUIRE(secondLive.size() == 1);
    REQUIRE(secondDrawn.size() == 1);
    CHECK(firstDrawn[0] == image.get());
    CHECK(secondLive[0] == image.get());
    CHECK(secondDrawn[0] == image.get());
    CHECK_FALSE(destroyed);
}

TEST_CASE("two sessions numbering the same images oppositely each resolve "
          "their own",
          "[deferred][foreign_image]")
{
    rcp<ForeignImage> a(new ForeignImage(1, nullptr));
    rcp<ForeignImage> b(new ForeignImage(2, nullptr));

    // Registration order sets the unflagged id, so the two sessions give the
    // same pair of images opposite ids. Resolving through anything id keyed
    // and shared crosses them, and both draws still land.
    cmd::DeferredSession forward(rive::ore::ReplayCaps{});
    drawForeign(forward, a.get());
    drawForeign(forward, b.get());
    auto forwardLive = replayedInline(forward);
    auto forwardDrawn = replayed(cmd::takeFrame(forward));

    cmd::DeferredSession reverse(rive::ore::ReplayCaps{});
    drawForeign(reverse, b.get());
    drawForeign(reverse, a.get());
    auto reverseLive = replayedInline(reverse);
    auto reverseDrawn = replayed(cmd::takeFrame(reverse));

    auto tags = [](const std::vector<const RenderImage*>& drawn) {
        std::vector<int> out;
        for (auto* image : drawn)
        {
            out.push_back(static_cast<const ForeignImage*>(image)->tag());
        }
        return out;
    };

    CHECK(tags(forwardLive) == std::vector<int>{1, 2});
    CHECK(tags(forwardDrawn) == std::vector<int>{1, 2});
    CHECK(tags(reverseLive) == std::vector<int>{2, 1});
    CHECK(tags(reverseDrawn) == std::vector<int>{2, 1});
}

TEST_CASE("a snapshot holds a foreign image past the frame and past its "
          "caller",
          "[deferred][foreign_image]")
{
    bool destroyed = false;
    auto* raw = new ForeignImage(3, &destroyed);
    rcp<ForeignImage> image(raw);

    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    drawForeign(session, raw);

    // takeFrame copies the retained images out and clears the registry, so
    // after the caller lets go the snapshot is the only owner left. A registry
    // that recorded the pointer without retaining it leaves replay a dangling
    // one, and replay would still draw.
    cmd::DeferredFrame frame = cmd::takeFrame(session);
    // Checked before the caller's reference goes away: a registry that only
    // recorded the pointer would leave the object already dead here, and the
    // release below would be a use after free rather than an assertion.
    REQUIRE(raw->debugging_refcnt() > 1);
    image = nullptr;
    REQUIRE_FALSE(destroyed);

    auto drawn = replayed(frame);
    REQUIRE(drawn.size() == 1);
    CHECK(drawn[0] == raw);
    CHECK(static_cast<const ForeignImage*>(drawn[0])->tag() == 3);
    CHECK_FALSE(destroyed);

    frame = cmd::DeferredFrame{};
    CHECK(destroyed);
}
