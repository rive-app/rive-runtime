/*
 * Copyright 2026 Rive
 */
#pragma once

#include "rive/refcnt.hpp"
#include "rive/renderer/render_canvas.hpp"
#include <mutex>
#include <vector>

// A recording hands out canvases whose pixels the replay thread allocates; the
// same thread has to free them, or a canvas dropped on the recording thread
// (a resize, a release) tears a texture out from under a replay in flight.
// These canvases park their backing here when they die, on any thread, and
// the next frame carries it to the replay thread to drop.
namespace rive::cmd
{
struct RetiredCanvasBacking
{
    rcp<gpu::Texture> texture;
    rcp<gpu::RenderTarget> renderTarget;
};

// Shared by the session and every canvas it made, so a canvas outliving the
// session finds it closed rather than a stranger at the same address.
class CanvasRetirer : public RefCnt<CanvasRetirer>
{
public:
    void retire(rcp<gpu::Texture> texture, rcp<gpu::RenderTarget> renderTarget)
    {
        if (texture == nullptr && renderTarget == nullptr)
        {
            return;
        }
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_closed)
        {
            return; // nothing replays any more; the caller lets it go
        }
        m_retired.push_back({std::move(texture), std::move(renderTarget)});
    }

    std::vector<RetiredCanvasBacking> take()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        return std::move(m_retired);
    }

    void close()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_closed = true;
        m_retired.clear();
    }

private:
    std::mutex m_mutex;
    std::vector<RetiredCanvasBacking> m_retired;
    bool m_closed = false;
};

class DeferredRenderCanvasImage : public gpu::RenderCanvasImage
{
public:
    DeferredRenderCanvasImage(rcp<CanvasRetirer> retirer,
                              uint32_t width,
                              uint32_t height) :
        gpu::RenderCanvasImage(width, height), m_retirer(std::move(retirer))
    {}

    ~DeferredRenderCanvasImage() override
    {
        m_retirer->retire(ref_rcp(getTexture()), nullptr);
        resetTexture(nullptr);
    }

private:
    rcp<CanvasRetirer> m_retirer;
};

class DeferredRenderCanvas : public gpu::RenderCanvas
{
public:
    DeferredRenderCanvas(rcp<CanvasRetirer> retirer,
                         uint32_t width,
                         uint32_t height) :
        gpu::RenderCanvas(
            make_rcp<DeferredRenderCanvasImage>(retirer, width, height),
            width,
            height),
        m_retirer(std::move(retirer))
    {}

    ~DeferredRenderCanvas() override
    {
        m_retirer->retire(nullptr, releaseRenderTarget());
    }

private:
    rcp<CanvasRetirer> m_retirer;
};
} // namespace rive::cmd
