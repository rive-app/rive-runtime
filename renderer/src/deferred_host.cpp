/*
 * Copyright 2026 Rive
 */

#if defined(RIVE_CANVAS) && defined(RIVE_ORE)

#include "rive/renderer/cmd/deferred_host.hpp"
#include "rive/renderer/render_canvas.hpp"
#include "rive/renderer/render_context.hpp"
#include "rive/renderer/render_context_impl.hpp"
#include <assert.h>
#include <stdio.h>

#if defined(__ANDROID__)
#include <android/log.h>
// stderr goes nowhere on Android, route diagnostics to logcat.
#define DEFERRED_WARN(...)                                                     \
    __android_log_print(ANDROID_LOG_ERROR, "Rive", __VA_ARGS__)
#else
#define DEFERRED_WARN(...) fprintf(stderr, __VA_ARGS__)
#endif

namespace rive::cmd
{

rive::Factory* HostFrameSink::factory() { return renderContext(); }

rive::ore::Context* HostFrameSink::oreContext()
{
    return m_replayOre ? renderContext()->ore() : nullptr;
}

rive::Renderer* HostFrameSink::beginScreenFrame(uint64_t target)
{
    if (target != m_target && !m_acceptAnyScreenTarget)
    {
        // Another texture's segments reached this sink, so the session frame
        // was taken while that texture was still recording. Drop them loudly
        // rather than draw them here; the replayer counts them as dropped.
        static int strays = 0;
        if (strays < 16)
        {
            strays++;
            DEFERRED_WARN("rive deferred: frame carried target %llu into the "
                          "sink for target %llu, dropping its draws\n",
                          (unsigned long long)target,
                          (unsigned long long)m_target);
        }
        // Native delivery is one host per session frame: hosts must record
        // sequentially, so a foreign target here means overlapping opens and
        // its draws would silently blank. Loud in debug; the warning above
        // is all release gets.
        assert(false && "deferred hosts must open and close their screen "
                        "target sequentially, a neighbor's frame reached "
                        "this sink");
        return nullptr;
    }
    if (m_screen == nullptr)
    {
        m_screen = beginScreen(target, m_clear, m_color);
    }
    return m_screen;
}

void HostFrameSink::beginOreFrame()
{
    if (auto* ore = oreContext())
    {
        ore->beginFrame({});
    }
}

void HostFrameSink::endOreFrame()
{
    if (auto* ore = oreContext())
    {
        ore->endFrame();
    }
}

rive::Renderer* HostFrameSink::beginCanvasContent(
    rive::gpu::RenderCanvas* canvas,
    uint32_t clearColor)
{
    m_activeCanvas = canvas;
    auto* rc = renderContext();
    rive::gpu::RenderContext::FrameDescriptor d{};
    d.renderTargetWidth = canvas->width();
    d.renderTargetHeight = canvas->height();
    d.loadAction = rive::gpu::LoadAction::clear;
    d.clearColor = clearColor;
    d.disableRasterOrdering = m_disableRasterOrdering;
    rc->beginFrame(d);
    m_canvasRenderer = std::make_unique<rive::RiveRenderer>(rc);
    return m_canvasRenderer.get();
}

void HostFrameSink::endCanvasContent()
{
    if (m_activeCanvas == nullptr)
    {
        return;
    }
    auto* rc = renderContext();
    rive::gpu::RenderContext::FlushResources fr{};
    fr.renderTarget = m_activeCanvas->renderTarget();
    if (m_useExternalCommandBuffer)
    {
        void* cb = rc->impl()->makeCommandBuffer();
        fr.externalCommandBuffer = cb;
        rc->flush(fr);
        rc->impl()->commitCommandBuffer(cb);
    }
    else
    {
        rc->flush(fr);
    }
    m_canvasRenderer.reset();
    m_activeCanvas = nullptr;
}

void DeferredInlineHost::beginRecord(bool clear, uint32_t color)
{
    m_doClear = clear;
    m_color = color;
    m_session->recordOreReplayMarker();
}

bool DeferredInlineHost::replayOre() const
{
    return m_session->oreContext().stream().commandBytes().size() != 0;
}

static void warnDroppedDraws(uint32_t droppedDraws)
{
    if (droppedDraws == 0)
    {
        return;
    }
    static int drops = 0;
    if (drops < 16)
    {
        drops++;
        DEFERRED_WARN("rive deferred: %u draws DROPPED this frame (mixed "
                      "factories or replay bug)\n",
                      droppedDraws);
    }
}

bool DeferredInlineHost::replayInline(HostFrameSink& sink,
                                      const std::function<void()>& present)
{
    // Nothing recorded, keep showing the last presented frame.
    if (!m_session->recordedThisFrame())
    {
        return false;
    }

    m_replayer.replayFrame(*m_session, sink);
    m_session->resetFrame();

    if (sink.began())
    {
        present();
    }
    warnDroppedDraws(m_replayer.droppedDraws());
    return true;
}

} // namespace rive::cmd

#endif // RIVE_CANVAS && RIVE_ORE
