/*
 * Copyright 2026 Rive
 */

#pragma once

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)

#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <functional>
#include <memory>

// Shared deferred rendering host layer for platform runtimes. Each platform
// keeps only how it reaches its render context, opens its screen frame, and
// schedules the replay.
namespace rive::cmd
{

// DeferredFrameSink over a platform render context. Derived sinks supply the
// context and the screen frame; policy flags cover the backend quirks.
class HostFrameSink : public DeferredFrameSink
{
public:
    // target is the render target this host owns in its session. One session
    // serves several, so a sink presents its own and refuses the rest rather
    // than drawing another texture's content into this one.
    HostFrameSink(bool clear,
                  uint32_t color,
                  uint64_t target = 0,
                  bool replayOre = true) :
        m_clear(clear), m_color(color), m_target(target), m_replayOre(replayOre)
    {}

    // The real render context replay runs against.
    virtual rive::gpu::RenderContext* renderContext() = 0;
    // Open the platform screen frame for one render target and return its
    // renderer, called once per target per replayed frame.
    virtual rive::Renderer* beginScreen(uint64_t target,
                                        bool clear,
                                        uint32_t color) = 0;

    rive::Factory* factory() override;
    // Null skips the Ore frame brackets and the shared Ore tail. Bracketing a
    // frame with no Ore content leaves GL state the screen flush renders
    // black through.
    rive::ore::Context* oreContext() override;
    rive::Renderer* beginScreenFrame(uint64_t target) override;
    uint64_t defaultScreenTarget() override { return m_target; }
    void beginOreFrame() override;
    void endOreFrame() override;
    rive::Renderer* beginCanvasContent(rive::gpu::RenderCanvas* canvas,
                                       uint32_t clearColor) override;
    void endCanvasContent() override;

    // This host's target was reached, so there is something to present.
    bool began() const { return m_screen != nullptr; }

protected:
    // A host whose session only ever displays on this one surface presents
    // every screen target: after a repoint, a frame recorded against the
    // prior texture's target is still this session's content, and refusing
    // it leaves static content blank with nothing left to re-record.
    bool m_acceptAnyScreenTarget = false;
    // D3D11 disables raster ordering and flushes without an external command
    // buffer.
    bool m_disableRasterOrdering = false;
    bool m_useExternalCommandBuffer = true;
    rive::gpu::RenderCanvas* m_activeCanvas = nullptr;

private:
    bool m_clear;
    uint32_t m_color;
    uint64_t m_target;
    // One sink serves every target of one replayed frame, so whether Ore runs
    // is settled once here rather than differing target to target.
    bool m_replayOre;
    // A sink presents exactly one target, so the screen it opens is a single
    // pointer and the per frame stack object never reaches the heap.
    rive::Renderer* m_screen = nullptr;
    std::unique_ptr<rive::RiveRenderer> m_canvasRenderer;
};

// Synchronous host: records into a bound session and replays inline on the
// recording thread. Hosts that thread the replay keep their own consumer
// machinery and only share the sink.
class DeferredInlineHost
{
public:
    // The caller owns the session and keeps it alive past every resource
    // created through it. The host claims a target identity within it.
    void bindSession(DeferredSession* session, uint64_t target = 0)
    {
        m_session = session;
        m_target = target;
    }
    DeferredSession* session() const { return m_session; }
    uint64_t target() const { return m_target; }

    // The recorder this host draws through, null without a session.
    rive::Renderer* screenRenderer() const
    {
        return m_session == nullptr ? nullptr
                                    : m_session->screenRenderer(m_target);
    }

    // Frame open: stash the clear policy and mark the Ore replay point.
    void beginRecord(bool clear, uint32_t color);

    bool doClear() const { return m_doClear; }
    uint32_t clearColor() const { return m_color; }
    // The recorded frame has Ore content, so replay brackets an Ore frame.
    bool replayOre() const;

    // False when nothing was recorded; the last presented frame stays up.
    // present runs only when replay reached the screen.
    bool replayInline(HostFrameSink& sink,
                      const std::function<void()>& present);

    DeferredReplayer& replayer() { return m_replayer; }

private:
    DeferredSession* m_session = nullptr;
    DeferredReplayer m_replayer;
    uint64_t m_target = 0;
    bool m_doClear = true;
    uint32_t m_color = 0;
};

} // namespace rive::cmd

#endif // RIVE_CANVAS && RIVE_ORE
