/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/canvas_schedule.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/gpu_census.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/renderer/ore/cmd/ore_replay.hpp"
#include <algorithm>
#include <deque>
#include <mutex>
#include <unordered_map>
#include <vector>

// DeferredReplayer consumes a recorded deferred frame, replaying the 2D and
// Ore streams against real resources through a DeferredFrameSink. Reusing one
// replayer keeps resources resident frame to frame. It makes no thread
// assumptions; the sink is the threading seam.
namespace rive::cmd
{

// Host supplied GPU frame operations, kept behind an interface so the replayer
// stays backend and window agnostic.
class DeferredFrameSink
{
public:
    virtual ~DeferredFrameSink() = default;

    // The real Factory resources replay against.
    virtual Factory* factory() = 0;

    // Defaults to the factory's ore context; hosts whose factory does not own
    // one override it.
    virtual ore::Context* oreContext() { return factory()->ore(); }

    // Open the screen frame for one render target and return its renderer.
    // A session drives every target its render context owns, so replay asks
    // once per target it recorded for. Left open for the caller to present.
    virtual Renderer* beginScreenFrame(uint64_t target) = 0;

    // Which target claims work no screen segment attributed, such as an Ore
    // only or canvas only frame. A sink serving one texture answers with that
    // texture's target, since target 0 may belong to a different one.
    virtual uint64_t defaultScreenTarget() { return 0; }

    // Bracket the recorded Ore passes with the backend's Ore frame.
    virtual void beginOreFrame() {}
    virtual void endOreFrame() {}
    // Backend state fix up after an Ore frame, such as a GL state invalidate.
    virtual void afterOreFrame() {}

    // Open the canvas's own frame and return the renderer its draws route
    // into, or null to drop them. endCanvasContent flushes it.
    virtual Renderer* beginCanvasContent(gpu::RenderCanvas* /*canvas*/,
                                         uint32_t /*clearColor*/)
    {
        return nullptr;
    }
    virtual void endCanvasContent() {}
};

// Immutable snapshot of one recorded frame so the producer can record the next
// frame while this one replays on a render thread.
struct DeferredFrame
{
    std::vector<uint8_t> commands, blobs;       // 2D ordered stream
    std::vector<uint8_t> oreCommands, oreBlobs; // Ore ordered stream
    std::vector<rcp<RenderImage>> canvasImages; // unflagged canvas id -> image
    std::unordered_map<RenderHandle, rcp<gpu::RenderCanvas>> contentCanvases;
    std::vector<rcp<rive::gpu::GPUResource>> oreReals; // unflagged real id
    // Assembled scheduler segments; byte ranges index the streams above.
    std::vector<DeferredSegment> segments;
    // The caps the recording answered with, so a replay far from the session
    // can still verify it runs on the device the stream was declared for.
    ore::ReplayCaps oreCaps;
};

inline DeferredFrame snapshotFrame(DeferredSession& session)
{
    // An errored script can leave a canvas range open; close it before the
    // bytes are copied.
    session.closeOpenRange();
    auto copy = [](Span<const uint8_t> s) {
        return std::vector<uint8_t>(s.data(), s.data() + s.size());
    };
    DeferredFrame f;
    f.commands = copy(session.commandBuffer().commandBytes());
    f.blobs = copy(session.commandBuffer().blobBytes());
    f.oreCommands = copy(session.oreContext().stream().commandBytes());
    f.oreBlobs = copy(session.oreContext().stream().blobBytes());
    f.canvasImages = session.canvases().images();
    f.contentCanvases = session.contentCanvases();
    f.oreReals = session.oreContext().realResources();
    f.segments = session.schedulerSegments();
    f.oreCaps = session.oreContext().caps();
    return f;
}

// Snapshot and clear as one step. A session hands over exactly one frame per
// rendered frame however many targets it drove, so the reset that ends the
// recording window belongs with the capture that reads it, not with any one
// target's flush.
inline DeferredFrame takeFrame(DeferredSession& session)
{
    DeferredFrame frame = snapshotFrame(session);
    session.resetFrame();
    return frame;
}

class DeferredReplayer
{
public:
    // Inline zero copy form: replay straight out of the session. Leaves the
    // screen frame open for the caller to present.
    void replayFrame(DeferredSession& session, DeferredFrameSink& sink)
    {
        session.closeOpenRange();
        replay(
            session.commandBuffer().commandBytes(),
            session.commandBuffer().blobBytes(),
            session.oreContext().stream().commandBytes(),
            session.oreContext().stream().blobBytes(),
            [&](RenderHandle id) { return session.canvasImageAt(id); },
            [&](RenderHandle id) { return session.contentCanvasAt(id); },
            session.oreContext().realResources(),
            sink,
            session.schedulerSegments(),
            session.oreContext().caps());
    }

    // Snapshot form: replay an owned frame, independent of the session.
    void replayFrame(const DeferredFrame& frame, DeferredFrameSink& sink)
    {
        replay(
            toSpan(frame.commands),
            toSpan(frame.blobs),
            toSpan(frame.oreCommands),
            toSpan(frame.oreBlobs),
            [&](RenderHandle id) -> RenderImage* {
                return id < frame.canvasImages.size()
                           ? frame.canvasImages[id].get()
                           : nullptr;
            },
            [&](RenderHandle id) -> gpu::RenderCanvas* {
                auto it = frame.contentCanvases.find(id);
                return it == frame.contentCanvases.end() ? nullptr
                                                         : it->second.get();
            },
            frame.oreReals,
            sink,
            frame.segments,
            frame.oreCaps);
    }

    // Drop the resident tables so the next replay recreates everything.
    void reset()
    {
        m_2d = ResourceTable{};
        m_ore = ore::cmd::OreResident{};
    }

    ResourceTable& table() { return m_2d; }

    // What the resident tables are holding on the GPU. Walks on demand and
    // costs the record and replay paths nothing, but reads the tables, so the
    // caller owes it a quiescent replayer.
    GpuCensus gpuCensus() const { return takeGpuCensus(m_2d, m_ore); }

private:
    static Span<const uint8_t> toSpan(const std::vector<uint8_t>& v)
    {
        return Span<const uint8_t>(v.data(), v.size());
    }

    template <typename CanvasImageFn, typename ContentCanvasFn>
    void replay(Span<const uint8_t> commands,
                Span<const uint8_t> blobs,
                Span<const uint8_t> oreCommands,
                Span<const uint8_t> oreBlobs,
                CanvasImageFn canvasImage,
                ContentCanvasFn contentCanvas,
                const std::vector<rcp<rive::gpu::GPUResource>>& oreReals,
                DeferredFrameSink& sink,
                const std::vector<DeferredSegment>& segments,
                const ore::ReplayCaps& oreCaps)
    {
        m_stats = ReplayStats{};
        m_2d.clearVersionAliases();
        ReplayHooks hooks;
        hooks.stats = &m_stats;
        hooks.canvasImage = canvasImage;
        // A canvas's content may span several ranges; its real frame opens at
        // the first range and flushes once its group ends below.
        Renderer* openContentRenderer = nullptr;
        RenderHandle openContentId = kInvalidRenderHandle;
        hooks.beginCanvasContent = [&](RenderHandle id,
                                       uint32_t clearColor) -> Renderer* {
            if (id == openContentId)
            {
                return openContentRenderer; // later range, frame already open
            }
            gpu::RenderCanvas* canvas = contentCanvas(id);
            openContentRenderer =
                canvas ? sink.beginCanvasContent(canvas, clearColor) : nullptr;
            openContentId = id;
            return openContentRenderer;
        };
        // A stale ore replay marker in the 2D stream replays as a no-op.

        auto subSpan = [](Span<const uint8_t> s, uint32_t begin, uint32_t end) {
            return Span<const uint8_t>(s.data() + begin, end - begin);
        };

        // The whole Ore stream replays as one frame. Splitting it per pass
        // would risk breaking a create use destroy resource lifecycle.
        auto replayOre = [&]() {
            if (oreCommands.empty())
            {
                return;
            }
            ore::Context* realOre = sink.oreContext();
            if (realOre == nullptr)
            {
                // One shot content like a canvas wrap never re-records, so a
                // skipped stream is permanent loss, never silent.
                RIVE_WARN_THROTTLED(
                    "rive deferred: no ore context, dropping %zu ore command "
                    "bytes (canvas content will be lost)\n",
                    oreCommands.size());
                return;
            }
            // A host that declared caps recorded against them; a replay
            // device that disagrees executes a stream recorded for other
            // hardware.
            if (oreCaps.featuresKnown && !oreCaps.matchesReplayDevice(*realOre))
            {
                fprintf(stderr,
                        "rive deferred: TRIPWIRE replay device disagrees with "
                        "the declared ReplayCaps\n");
                assert(false && "replay device does not match declared caps");
            }
            sink.beginOreFrame();
            ore::cmd::replayOreStream(
                *realOre,
                oreCommands,
                oreBlobs,
                m_ore,
                [&](ore::cmd::ResourceHandle h) -> rive::gpu::GPUResource* {
                    ore::cmd::ResourceHandle i =
                        h & ore::cmd::kRealResourceMask;
                    return i < oreReals.size() ? oreReals[i].get() : nullptr;
                },
                [&](uint32_t canvasId) -> gpu::RenderCanvas* {
                    return contentCanvas(canvasId);
                },
                [&](uint32_t imageId) -> RenderImage* {
                    // Resident 2D image, created by the hoisted create pass
                    // or an earlier frame.
                    return m_2d.images.get(imageId);
                });
            sink.endOreFrame();
            sink.afterOreFrame();
        };

        // Creates and mutations replay first over the whole stream in record
        // order; draws pin the version they recorded against, so the segment
        // partition below cannot misorder state or break mint order.
        hooks.filter = ReplayFilter::resources;
        replayRenderCommands(sink.factory(),
                             nullptr,
                             commands,
                             blobs,
                             m_2d,
                             hooks);
        hooks.filter = ReplayFilter::draws;

        // Partition the 2D segments canvas before screen; the backend allows
        // one open frame at a time. Ore passes must run inside the screen
        // frame right after it opens or the ramp upload finds no texture
        // bound.
        std::vector<const DeferredSegment*> screenSegments;
        std::unordered_map<uint64_t, std::vector<const DeferredSegment*>>
            canvasRanges;
        for (const DeferredSegment& s : segments)
        {
            if (s.target == DeferredSegment::Target::screen)
            {
                screenSegments.push_back(&s);
                continue;
            }
            canvasRanges[s.targetId].push_back(&s);
        }
        // Canvas groups replay in dependency order, so a sampler sees this
        // frame's content regardless of record order. Cycles keep record
        // order on the back edge: previous frame sampling, by contract.
        CanvasSchedule schedule = scheduleCanvases(commands, segments);
        if (schedule.hadCycle)
        {
            RIVE_WARN_THROTTLED("rive deferred: canvas sample cycle, the "
                                "back edge samples the previous frame\n");
        }
        const std::vector<uint64_t>& canvasOrder = schedule.order;
        // Ore replays once for the whole session frame, inside the first
        // screen frame opened. The scripting GPU surface is canvas scoped -
        // render passes exist only as canvas:beginRenderPass, with no screen
        // or frame target type - so a script writes canvases and never
        // screens, and its output reaches a target indirectly through 2D
        // draws that sample canvas textures. Running it in whichever screen
        // frame opens first therefore orders it ahead of every target's draws
        // without attributing it to any one of them. The decision is owned
        // here, at session frame scope, so per target sinks cannot disagree
        // about whether Ore ran at all.
        bool oreReplayed = false;
        std::unordered_map<uint64_t, Renderer*> openScreens;
        auto openScreenAndOre = [&](uint64_t target) -> Renderer* {
            auto entry = openScreens.try_emplace(target, nullptr);
            if (entry.second)
            {
                entry.first->second = sink.beginScreenFrame(target);
            }
            if (!oreReplayed)
            {
                replayOre();
                oreReplayed = true;
            }
            return entry.first->second;
        };
        for (uint64_t canvasId : canvasOrder)
        {
            for (const DeferredSegment* seg : canvasRanges[canvasId])
            {
                replayRenderCommands(sink.factory(),
                                     nullptr,
                                     subSpan(commands, seg->begin, seg->end),
                                     blobs,
                                     m_2d,
                                     hooks);
            }
            if (openContentRenderer != nullptr)
            {
                sink.endCanvasContent(); // flush once per canvas
            }
            openContentRenderer = nullptr;
            openContentId = kInvalidRenderHandle;
        }
        for (const DeferredSegment* seg : screenSegments)
        {
            // Target's frame open, Ore inside the first of them, then draw.
            Renderer* screen = openScreenAndOre(seg->targetId);
            replayRenderCommands(sink.factory(),
                                 screen,
                                 subSpan(commands, seg->begin, seg->end),
                                 blobs,
                                 m_2d,
                                 hooks);
        }
        // Work no screen segment claimed still falls to the default target:
        // ore has nowhere else to run, and a canvas only frame still owes the
        // host the screen frame its clear and present live in.
        if (openScreens.empty() &&
            (!oreCommands.empty() || !canvasRanges.empty()))
        {
            openScreenAndOre(sink.defaultScreenTarget());
        }
        // Destroys replay last so a destroy recorded in a screen gap cannot
        // free a resource a reordered canvas segment still draws.
        hooks.filter = ReplayFilter::destroys;
        replayRenderCommands(sink.factory(),
                             nullptr,
                             commands,
                             blobs,
                             m_2d,
                             hooks);
    }

    ResourceTable m_2d;          // resident 2D resources
    ore::cmd::OreResident m_ore; // resident Ore resources
    ReplayStats m_stats;

public:
    // Draws dropped in the last replayFrame. Nonzero means mixed factory
    // recording or a replay bug the host should surface.
    uint32_t droppedDraws() const { return m_stats.droppedDraws; }
};

} // namespace rive::cmd
