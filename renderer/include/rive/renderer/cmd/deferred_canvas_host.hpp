/*
 * Copyright 2026 Rive
 */

#pragma once

#include "rive/refcnt.hpp"
#include "rive/shapes/paint/layer_mask_mode.hpp"

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
  virtual rcp<gpu::RenderCanvas> makeContentCanvas(
      uint32_t width,
      uint32_t height) = 0;

  // The same, but a recording host mints it before a device binds too.
  // Scripts make their canvases at init, ahead of the frame that binds one.
  virtual rcp<gpu::RenderCanvas> makeUnboundContentCanvas(
      uint32_t width,
      uint32_t height) = 0;

  // Whether composeLayerMask works on canvases this host hands out. Callers
  // ask before rasterizing and draw the layer inline, unmasked, on a no.
  virtual bool supportsLayerMask() const { return false; }
  // Writes each content texel, scaled by a factor from the matching coverage
  // texel, into result. All three are one size. False leaves result untouched.
  virtual bool composeLayerMask(
      gpu::RenderCanvas* /*content*/,
      gpu::RenderCanvas* /*coverage*/,
      gpu::RenderCanvas* /*result*/,
      LayerMaskMode /*mode*/)
  {
    return false;
  }

  // The image a composite should sample to get this canvas's pixels.
  virtual rcp<RenderImage> contentCanvasImage(gpu::RenderCanvas* canvas) = 0;

  // A renderer to issue the composite through, or null to use the one that
  // was drawing. An immediate host that interrupted a live frame to give the
  // canvas its own has to answer this: a renderer carried across that
  // interrupt cannot draw the result afterwards, while one created fresh
  // against the resumed frame can. [current] is the renderer that was
  // drawing; the host gives the fresh one its clip. The caller re-applies the
  // current transform and opacity.
  virtual Renderer* compositeRenderer(Renderer* current) { return nullptr; }
  // True when a composite through compositeRenderer() would not be clipped
  // like the original draw. Asked without allocating one.
  virtual bool compositeDropsClips() const { return false; }

  // Emits the content begin bracket and returns a recording renderer owned
  // by the host and valid until endCanvasContent. clearColor is ARGB.
  virtual Renderer* beginCanvasContent(
      gpu::RenderCanvas* canvas,
      uint32_t clearColor) = 0;

  // Emits the content end bracket and releases the renderer.
  virtual void endCanvasContent(gpu::RenderCanvas* canvas) = 0;

#ifdef WITH_RIVE_TOOLS
  // Replay only has a canvas's finished frame, so tooling warns a script
  // whose reader samples one while its frame is open.
  virtual bool canvasContentOpen(gpu::RenderCanvas*, const Renderer* /*reader*/)
      const
  {
    return false;
  }
#endif
};

#ifdef WITH_RIVE_TOOLS
inline constexpr char kSampledOpenCanvasWarning[] =
    "A canvas image was drawn while that canvas's frame was open, so it shows "
    "the canvas's finished frame. Call endFrame() before drawing its image.";
#endif

} // namespace cmd
} // namespace rive
