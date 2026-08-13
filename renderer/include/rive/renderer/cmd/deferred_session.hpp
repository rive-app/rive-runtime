/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/renderer/cmd/foreign_image_registry.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/render_canvas.hpp"
#include <algorithm>
#include <unordered_map>
#include <vector>

// DeferredSession owns a deferred frame: the 2D stream (it is a
// DeferredFactory), the Ore context, and the shared canvas registry.
// Everything records into one ordered 2D stream, so a single replay pass is
// byte identical by construction with no command reordering.
namespace rive::cmd
{

// One scheduler segment: a canvas or screen run plus the byte range
// [begin, end) in the 2D stream. Replay runs every canvas segment before any
// screen segment while preserving record order within each phase.
struct DeferredSegment
{
    enum class Target : uint8_t
    {
        canvas, // offscreen, runs first
        screen, // main target, runs last
    };
    Target target;
    // Canvas id for a canvas run, screen target id for a screen one. A session
    // serves several screen targets, so a screen run names the one it feeds.
    uint64_t targetId;
    uint32_t begin;
    uint32_t end;
};

class DeferredSession : public DeferredFactory,
                        public DeferredCanvasHost,
                        public DeferredRouteHost
{
public:
    // realOre may be null on web; it late binds via bindRealOre.
    explicit DeferredSession(ore::Context* realOre) : m_ore(realOre)
    {
        wireOreCanvases();
    }

    // Capability only construction: recording holds no device, so a host can
    // record on a thread or process the real context never visits. The
    // consumer's caps late bind via bindReplayCaps if unknown here.
    explicit DeferredSession(const ore::ReplayCaps& caps) : m_ore(caps)
    {
        wireOreCanvases();
    }

    ore::cmd::DeferredOreContext& oreContext() { return m_ore; }
    void bindRealOre(ore::Context* real) { m_ore.bindReal(real); }
    void bindReplayCaps(const ore::ReplayCaps& caps) { m_ore.bindCaps(caps); }

    // Cross session image sharing lives here, not in an id space: the
    // registry carries a real rcp<RenderImage> through the frame snapshot, so
    // any number of sessions can name the same image without a shared table
    // to resolve it against, and re-registering per frame keeps the retained
    // set bounded. Everything else a render context decodes is already shared
    // because one session records it all.
    ForeignImageRegistry& canvases() { return m_canvases; }

    // Hosts that import through this session get the recording ore context
    // automatically.
    rive::ore::Context* ore() override { return &m_ore; }

    // The context this session records for. Scripts imported through the
    // session talk to it directly for GPU state while their canvas work
    // records, so it has to be the real thing, not the session. Web has none
    // while the file imports, so the host binds the attaching texture's one
    // here alongside bindRealOre and a script that deferred its canvas backing
    // reads it again when the real size arrives.
    void bindRenderContext(Factory* renderContext)
    {
        m_renderContext = renderContext;
    }
    Factory* renderContext() override { return m_renderContext; }
    cmd::DeferredCanvasHost* deferredCanvasHost() override { return this; }

    // Routed so a screen draw issued while a canvas range is open lands in a
    // screen range, not the canvas's.
    std::unique_ptr<Renderer> makeScreenRenderer(uint64_t target = 0)
    {
        return std::make_unique<DeferredRenderer>(&commandBuffer(),
                                                  &m_canvases,
                                                  this,
                                                  screenTarget(target));
    }

    // Stable screen recorder for FFI hosts that hold a raw pointer across
    // frames, one per render target this session drives.
    Renderer* screenRenderer(uint64_t target = 0)
    {
        auto& recorder = m_screenRenderers[target];
        if (recorder == nullptr)
        {
            recorder = makeScreenRenderer(target);
        }
        return recorder.get();
    }

    // ---- Render targets ----
    // A host claims an id for its lifetime; ids are reused so a long lived
    // context churning textures does not grow the recorder map forever. The
    // first claim is 0, which is what a single host session records today.
    uint64_t acquireScreenTarget()
    {
        if (!m_freeScreenTargets.empty())
        {
            uint64_t id = m_freeScreenTargets.back();
            m_freeScreenTargets.pop_back();
            return id;
        }
        return m_nextScreenTarget++;
    }
    // The host is gone, so its recorder is too. Callers drain first, and a
    // queued frame holds bytes rather than the recorder.
    void releaseScreenTarget(uint64_t target)
    {
        m_screenRenderers.erase(target);
        m_freeScreenTargets.push_back(target);
    }
    // Hosts holding an id right now. Sizes anything that has to scale with the
    // targets sharing this session, such as the consumer's queue bound.
    size_t attachedTargetCount() const
    {
        return static_cast<size_t>(m_nextScreenTarget) -
               m_freeScreenTargets.size();
    }

    // ---- Frame boundary ----
    // A session serves every target its render context drives, so the frame
    // is a session-wide window: it opens when the first target starts
    // recording and closes when the last one finishes. Ending the window per
    // host would reset the stream underneath a target still recording.
    void beginTargetFrame(uint64_t target)
    {
        if (std::find(m_openTargets.begin(), m_openTargets.end(), target) ==
            m_openTargets.end())
        {
            m_openTargets.push_back(target);
        }
    }
    // True once this closes the last open target, meaning the caller may take
    // the frame. A target that never opened one closes nothing.
    bool endTargetFrame(uint64_t target)
    {
        auto it = std::find(m_openTargets.begin(), m_openTargets.end(), target);
        if (it == m_openTargets.end())
        {
            return m_openTargets.empty();
        }
        m_openTargets.erase(it);
        return m_openTargets.empty();
    }
    // A host that stops recording without finishing, such as one paused
    // mid frame, must not pin the window shut for everyone else.
    void abandonTargetFrame(uint64_t target)
    {
        auto it = std::find(m_openTargets.begin(), m_openTargets.end(), target);
        if (it != m_openTargets.end())
        {
            m_openTargets.erase(it);
        }
    }

    // ---- DeferredRouteHost ----
    // Splits the stream into per target scheduler ranges as the issuing
    // renderer changes.
    void routeTo(uint64_t target) override
    {
        if (m_activeRouted && target == m_activeTarget)
        {
            return;
        }
        closeActiveRange();
        m_activeTarget = target;
        m_activeRouted = true;
        m_activeBegin = streamSize();
        if (isScreenTarget(target))
        {
            m_openScreen = screenTargetId(target);
            m_hasOpenScreen = true;
            return;
        }
        RenderHandle id = static_cast<RenderHandle>(target);
        commandBuffer().append(
            static_cast<uint8_t>(RenderCmd::canvasContentBegin),
            CanvasContentPOD{id | kCanvasHandleFlag, m_canvasClear[target]});
    }

    // Snapshots call this too since an errored script can leave a range open.
    void closeOpenRange()
    {
        closeActiveRange();
        reopenUnroutedRange();
    }

    // ---- DeferredCanvasHost ----
    // A canvas may record as several interleaved ranges; replay groups them
    // back into one real canvas frame.
    Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                 uint32_t clearColor) override
    {
        RenderHandle id =
            m_canvases.imageDrawId(canvas->renderImage()) & kCanvasHandleMask;
        m_contentCanvases[id] = ref_rcp(canvas);
        m_canvasClear[id] = clearColor;
        auto& recorder = m_canvasRenderers[id];
        if (recorder == nullptr)
        {
            recorder = std::make_unique<DeferredRenderer>(&commandBuffer(),
                                                          &m_canvases,
                                                          this,
                                                          id);
        }
        // Open the range now so a clear-only frame still clears at replay.
        routeTo(id);
        return recorder.get();
    }
    void endCanvasContent(gpu::RenderCanvas*) override
    {
        // Back to the screen whose recording the canvas interrupted, so the
        // bytes that follow are not credited to a target that drew nothing.
        if (m_hasOpenScreen)
        {
            routeTo(screenTarget(m_openScreen));
            return;
        }
        closeActiveRange();
        reopenUnroutedRange();
    }

    // Replay bindings are per frame so the retained set stays bounded; each
    // frame's draws re-register what they reference.
    void resetFrame()
    {
        DeferredFactory::resetFrame();
        m_ore.resetFrame();
        m_canvases.reset();
        m_contentCanvases.clear();
        m_canvasRenderers.clear();
        m_canvasClear.clear();
        m_activeTarget = kScreenTarget;
        m_activeRouted = false;
        m_activeBegin = 0;
        m_openScreen = 0;
        m_hasOpenScreen = false;
        m_hasOreMarker = false;
        m_segments.clear();
    }

    // Physical bytes this session's producer streams hold. Computed on
    // demand so recording pays nothing; the frame boundary drains it, so it
    // only means anything read before a snapshot.
    uint64_t streamBytes() const
    {
        return commandBuffer().commandBytes().size() +
               commandBuffer().blobBytes().size() +
               m_ore.stream().commandBytes().size() +
               m_ore.stream().blobBytes().size();
    }

    // Nothing recorded: the host keeps its last presented frame up. Pending
    // ore content counts, since one shot content like a canvas wrap never
    // re-records and must not park behind the gate.
    bool recordedThisFrame() const
    {
        return m_hasOreMarker || !commandBuffer().empty() ||
               !m_ore.stream().empty();
    }

    // Closed segments in record order, canvas and screen alike.
    const std::vector<DeferredSegment>& recordedSegments() const
    {
        return m_segments;
    }
    // Full scheduler input: the closed segments plus the range still open.
    std::vector<DeferredSegment> schedulerSegments() const
    {
        std::vector<DeferredSegment> all = m_segments;
        if (m_activeRouted && isScreenTarget(m_activeTarget) &&
            streamSize() > m_activeBegin)
        {
            all.push_back({DeferredSegment::Target::screen,
                           screenTargetId(m_activeTarget),
                           m_activeBegin,
                           streamSize()});
        }
        return all;
    }

    // Ore replays via segment scheduling; the frame only needs to know Ore
    // content exists so an otherwise empty frame still replays.
    void recordOreReplayMarker() { m_hasOreMarker = true; }

    // Render thread lookups for the replay hooks.
    gpu::RenderCanvas* contentCanvasAt(RenderHandle id) const
    {
        auto it = m_contentCanvases.find(id);
        return it == m_contentCanvases.end() ? nullptr : it->second.get();
    }
    RenderImage* canvasImageAt(RenderHandle id) const
    {
        return m_canvases.imageAt(id);
    }
    // Retained content canvas bindings for the consumer snapshot.
    const std::unordered_map<RenderHandle, rcp<gpu::RenderCanvas>>&
    contentCanvases() const
    {
        return m_contentCanvases;
    }

private:
    void wireOreCanvases()
    {
        // Register wrapped canvases under the shared 2D id space so the
        // consumer can perform the real wrap at replay.
        m_ore.canvasIdProvider = [this](gpu::RenderCanvas* canvas) -> uint32_t {
            RenderHandle id = m_canvases.imageDrawId(canvas->renderImage()) &
                              kCanvasHandleMask;
            m_contentCanvases[id] = ref_rcp(canvas);
            return id;
        };
        // The 2D stream shares the ore stream's single writer contract.
        commandBuffer().bindRecordingThread();
        // Lets view() on a canvas backed image resolve its canvas id off the
        // registry.
        m_ore.canvasRegistry = &m_canvases;
    }

    uint32_t streamSize() const
    {
        return static_cast<uint32_t>(commandBuffer().commandBytes().size());
    }

    // Reopen the range no target has claimed. Bytes appended outside any
    // renderer - resource creates, drained destroys - belong to no target:
    // they replay from the whole stream in the create and destroy passes, so
    // crediting them to a screen would open that target's frame in a frame
    // where only other targets drew.
    void reopenUnroutedRange()
    {
        m_activeTarget = screenTarget(m_openScreen);
        m_activeRouted = m_hasOpenScreen;
        m_activeBegin = streamSize();
    }

    // Push the open range as a segment, closing a canvas one's bracket first.
    // An empty screen range is dropped: replaying it would open its target's
    // frame to draw nothing.
    void closeActiveRange()
    {
        if (!m_activeRouted)
        {
            return;
        }
        if (isScreenTarget(m_activeTarget))
        {
            if (streamSize() > m_activeBegin)
            {
                m_segments.push_back({DeferredSegment::Target::screen,
                                      screenTargetId(m_activeTarget),
                                      m_activeBegin,
                                      streamSize()});
            }
            return;
        }
        RenderHandle id = static_cast<RenderHandle>(m_activeTarget);
        commandBuffer().append(
            static_cast<uint8_t>(RenderCmd::canvasContentEnd),
            ResIdPOD{id | kCanvasHandleFlag});
        // Push order of m_segments defines record order across both streams.
        m_segments.push_back({DeferredSegment::Target::canvas,
                              m_activeTarget,
                              m_activeBegin,
                              streamSize()});
    }

    ore::cmd::DeferredOreContext m_ore;
    // Set on texture attach, read by the producer's scripts. Written and read
    // on different threads in the worker build, exactly as m_ore's real
    // binding already is.
    Factory* m_renderContext = nullptr;
    ForeignImageRegistry m_canvases;
    // Canvas id to real canvas, retained so it lives to replay. Per frame.
    std::unordered_map<RenderHandle, rcp<gpu::RenderCanvas>> m_contentCanvases;
    // Alive until resetFrame so scripted renderers stay valid across
    // interleaved canvas frames.
    std::unordered_map<uint64_t, std::unique_ptr<DeferredRenderer>>
        m_canvasRenderers;
    // First beginFrame's clear color; replay clears only when it opens the
    // real frame.
    std::unordered_map<uint64_t, uint32_t> m_canvasClear;
    // Deliberately outlives resetFrame, unlike m_canvasRenderers: FFI hosts
    // take these raw and keep drawing through them frame after frame.
    std::unordered_map<uint64_t, std::unique_ptr<Renderer>> m_screenRenderers;
    bool m_hasOreMarker = false;
    // Scheduler segments recorded this frame, in script issue order.
    std::vector<DeferredSegment> m_segments;
    uint64_t m_activeTarget = kScreenTarget; // target of the open range
    // False while the open range belongs to no target, which is how a frame
    // starts and where creates outside any renderer land.
    bool m_activeRouted = false;
    uint32_t m_activeBegin = 0; // open range's start offset
    // Screen target a closing canvas range hands the stream back to, unset
    // until a screen actually records.
    uint64_t m_openScreen = 0;
    bool m_hasOpenScreen = false;
    // Render targets attached to this session, and the ones still recording
    // this frame.
    uint64_t m_nextScreenTarget = 0;
    std::vector<uint64_t> m_freeScreenTargets;
    std::vector<uint64_t> m_openTargets;
};

} // namespace rive::cmd
