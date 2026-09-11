#include "rive/bitmap_cache.hpp"
#ifdef RIVE_CANVAS
#include "rive/artboard.hpp"
#include "rive/renderer/render_canvas.hpp"
#endif

using namespace rive;

// Defined out of line so the rcp<gpu::RenderCanvas> member is constructed and
// destroyed in a TU where RenderCanvas is complete.
BitmapCache::BitmapCache() = default;
BitmapCache::~BitmapCache() = default;

#ifdef RIVE_CANVAS
void BitmapCache::invalidate()
{
    // The cached raster is no longer trustworthy; rebuild it on the next draw.
    m_dirty = true;
    // ...but only if there *is* a next draw. m_dirty is private to this object,
    // so nothing outside it can tell the artboard changed -- and a host that
    // skips rendering while Artboard::didChange() is false would then never
    // submit the frame that rebuilds the cache, leaving the stale raster on
    // screen for good. Null while the property is being deserialized, before
    // the object is parented.
    if (Artboard* owner = artboard())
    {
        owner->changed();
    }
}
#endif

void BitmapCache::resolutionChanged()
{
#ifdef RIVE_CANVAS
    // A new resolution means the cached raster is the wrong size.
    invalidate();
#endif
}

void BitmapCache::cacheFlagsChanged()
{
#ifdef RIVE_CANVAS
    // A flag change can alter how the raster is produced, so the existing one
    // is no longer trustworthy. (`dither` is reserved and not read anywhere
    // yet, so today only cacheEnabled actually changes the output -- see the
    // property's description in dev/defs/bitmap_cache.json.)
    invalidate();
    if (!cacheEnabled())
    {
        // Freeing the texture is the point of a disable switch. Safe to drop
        // ours mid-frame: the canvas is refcounted and a DeferredFrame being
        // replayed holds its own reference.
        m_canvas.reset();
        m_widthPx = 0;
        m_heightPx = 0;
    }
#endif
}
