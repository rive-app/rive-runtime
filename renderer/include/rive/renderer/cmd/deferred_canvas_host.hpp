/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"

#include <cstdint>

// Hook the scripting layer uses when a deferred host is recording. Stored on
// the ScriptingContext as a forward declared pointer so the script headers
// stay decoupled from the cmd layer; DeferredSession implements it.
namespace rive
{
class Renderer;
class RenderImage;
namespace gpu
{
class RenderCanvas;
}

namespace cmd
{

class DeferredCanvasHost
{
public:
    virtual ~DeferredCanvasHost() = default;

    // Allocates the canvas whose content this host is about to take. Who
    // allocates matters: a recording host can hand back an unbacked canvas and
    // leave the pixels to whoever replays, while a host that draws immediately
    // has no replay to defer to and must return one with a real texture. It
    // also keeps callers from having to find a device themselves. Null means
    // this host cannot provide one, and the caller draws normally instead.
    virtual rcp<gpu::RenderCanvas> makeContentCanvas(uint32_t width,
                                                     uint32_t height) = 0;

    // Whether a layer mask can actually be applied to a canvas this host hands
    // out. Asked here rather than of the Factory or the Renderer because this
    // is the one party that is always resolved: the renderer that records is
    // not the renderer that draws, and a factory may delegate its host away to
    // a session bound to a different device.
    //
    // A "no" is load bearing. Renderer::applyLayerMask no-ops rather than
    // drawing something wrong, but by then the mask has already sized its
    // rasters to the content-and-coverage intersection, so the layer would come
    // back unmasked *and* cropped to a box that only made sense if the mask had
    // been applied. The caller has to know before it rasterizes, so it can draw
    // the range inline instead -- both truly unmasked and free of two canvases
    // and a composite.
    //
    // Defaults to NO, and deliberately: Renderer::applyLayerMask is itself a
    // no-op by default, so a host that has not thought about the question is
    // exactly the host whose renderer will silently drop the op -- and a
    // yes-by-default would have it crop the layer to the mask box on the way.
    // A capability is opted into by the hosts that can honour it; there are
    // three in tree, and each says so.
    virtual bool supportsLayerMask() const { return false; }

    // The image a composite should sample to get this canvas's pixels.
    // Usually the canvas's own render image, but not always: GL renders into a
    // canvas bottom-up and keeps a Y-flipped companion for anything that wants
    // to sample it, so a GL host hands that back instead. The host answers
    // because it is the only party here that knows which backend it drives.
    virtual rcp<RenderImage> contentCanvasImage(gpu::RenderCanvas* canvas) = 0;

    // A renderer to issue the composite through, or null to use the one that
    // was drawing. An immediate host that interrupted a live frame to give the
    // canvas its own has to answer this: a renderer carried across that
    // interrupt cannot draw the result afterwards, while one created fresh
    // against the resumed frame can. The caller re-applies the current
    // transform, since a fresh renderer shares no state with the original.
    virtual Renderer* compositeRenderer() { return nullptr; }

    // Emits the content begin bracket and returns a recording renderer owned
    // by the host and valid until endCanvasContent. clearColor is ARGB.
    virtual Renderer* beginCanvasContent(gpu::RenderCanvas* canvas,
                                         uint32_t clearColor) = 0;

    // Emits the content end bracket and releases the renderer.
    virtual void endCanvasContent(gpu::RenderCanvas* canvas) = 0;
};

} // namespace cmd
} // namespace rive
