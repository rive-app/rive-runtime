#ifndef _RIVE_BITMAP_CACHE_HPP_
#define _RIVE_BITMAP_CACHE_HPP_
#include "rive/generated/bitmap_cache_base.hpp"
#ifdef RIVE_CANVAS
#include "rive/refcnt.hpp"
#endif
#include <cstdint>

namespace rive
{
#ifdef RIVE_CANVAS
namespace gpu
{
class RenderCanvas;
}
#endif

// A BitmapCache child on an Artboard turns on cache-as-bitmap for that
// artboard: the artboard's content is rasterized once into an offscreen texture
// and composited each frame instead of re-drawing its vector content. The
// object both configures the behavior (resolution) and, at runtime, owns the
// offscreen render state -- so removing the object automatically frees the GPU
// texture. The Artboard draw path (a friend) reads and updates the render
// state.
class BitmapCache : public BitmapCacheBase
{
    friend class Artboard;

public:
    // Constructor and destructor are out of line so the rcp<gpu::RenderCanvas>
    // member is only constructed/destroyed where RenderCanvas is a complete
    // type (bitmap_cache.cpp) -- otherwise `new BitmapCache()` in the core
    // registry would try to touch the incomplete type.
    BitmapCache();
    ~BitmapCache() override;

    // The mask's bits are named by the generator itself -- cacheEnabled() /
    // cacheEnabled(bool) and dither() / dither(bool) on the base, each folding
    // into cacheFlags(). Re-declaring the getters here would hide the setters.

#ifdef RIVE_CANVAS
private:
    rcp<gpu::RenderCanvas> m_canvas;
    uint32_t m_widthPx = 0;
    uint32_t m_heightPx = 0;
    // Texels per artboard unit baked into m_canvas. The composite inverts this
    // to place the raster, so it has to be the scale that was actually drawn
    // with, not one re-derived from the rounded pixel dimensions.
    float m_rasterScale = 1.0f;
    bool m_dirty = true;
#endif

protected:
#ifdef RIVE_CANVAS
    // Marks the raster stale and tells the artboard it changed, so a host
    // gated on Artboard::didChange() still draws the frame that rebuilds it.
    void invalidate();
#endif
    void resolutionChanged() override;
    // One hook covers both bits: every passthrough write from CoreRegistry
    // funnels through the cacheFlags() setter.
    void cacheFlagsChanged() override;
};
} // namespace rive

#endif
