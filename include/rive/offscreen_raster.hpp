#ifndef _RIVE_OFFSCREEN_RASTER_HPP_
#define _RIVE_OFFSCREEN_RASTER_HPP_

#ifdef RIVE_CANVAS

#include "rive/math/aabb.hpp"
#include "rive/math/mat2d.hpp"

#include <cstdint>

namespace rive
{
class Renderer;

namespace gpu
{
class RenderCanvas;
}
namespace cmd
{
class DeferredCanvasHost;
}

// Shared machinery for "rasterize something into an offscreen RenderCanvas at
// N texels per local unit, then composite it back where the vector draw would
// have gone".
//
// This was lifted verbatim out of Artboard::drawCachedAsBitmap /
// Artboard::renderIntoCanvas so a second call site (layer masks) cannot
// re-introduce the bugs that path already paid for:
//
//   * the device scale is quantized to sixteenths, so scrubbing zoom does not
//     re-rasterize and re-allocate on every frame;
//   * the pixel cap is applied by shrinking the scale uniformly rather than
//     clamping one axis, so the two axes never disagree about the mapping;
//   * the composite inverts the *exact* raster scale rather than
//     width / widthPx, which would fold ceil()'s rounding into the mapping and
//     leave a fractional-size box permanently resampled at ~0.995x;
//   * the composite snaps to the pixel grid when the transform is axis
//     aligned, because a half-pixel origin turns every bilinear tap into a
//     blend of two texels -- a box blur over the whole image.
namespace offscreen
{

// No raster may exceed this on either axis. Public because the bucketing and
// the resize policy both have to respect it.
constexpr uint32_t kMaxDim = 2048;

// Everything decided while recording, before anything is rasterized. Both the
// raster size and the composite placement come from here, so a caller that
// rasterizes several things into the same pixel space (content and mask, say)
// plans once and reuses the result for all of them.
struct RasterPlan
{
    // The renderer's transform when the plan was made (local -> device). The
    // raster size is chosen from it, which is why it has to be read while
    // recording rather than at replay.
    Mat2D ctm;
    bool haveCtm = false;
    // The opacity accumulated by an enclosing modulateOpacity() scope. Only
    // needed on the fresh-renderer composite path, which inherits no state.
    float modulatedOpacity = 1.0f;
    bool haveOpacity = false;
    // The local-space box being rasterized. Not necessarily [0,0,w,h].
    AABB box;
    // Texels per local unit actually used for the raster.
    float rasterScale = 1.0f;
    uint32_t widthPx = 0;
    uint32_t heightPx = 0;

    bool valid() const { return widthPx != 0 && heightPx != 0; }
};

// How fitRasterToBox turns a local box into raster geometry.
enum class RasterFit : uint8_t
{
    // widthPx = ceil(box * scale), box passed through untouched. What every
    // caller did before layer masks needed anything else, and still the right
    // answer whenever the box is stable across frames (an artboard's own
    // bounds, say) so re-allocation is not a concern.
    exact,

    // For a box derived from content, which moves and resizes every frame:
    // snap the origin to the raster's own texel grid, add a transparent guard
    // band, and round the size up to a bucket. Grows `box` to span exactly the
    // chosen pixel size, so the composite's inverse stays pixel exact.
    stableGrid,
};

// Chooses a raster size for `box` at `resolution` quality, reading the current
// transform and modulated opacity off `renderer`. Returns false (leaving *out
// untouched) for a degenerate box or a non-finite/zero scale, in which case the
// caller should draw normally instead.
bool planRaster(Renderer* renderer,
                const AABB& box,
                float resolution,
                RasterPlan* out);

// The box-independent half of planRaster: reads the CTM and the modulated
// opacity off `renderer` and works out how many texels per local unit this
// frame wants. Cheap, and it touches nothing that depends on what is being
// rasterized, so a caller can run it before deciding whether it even needs to
// measure its content. Returns false for a non-finite or zero scale.
//
// Leaves out->box / widthPx / heightPx alone; fitRasterToBox fills those in.
bool planRasterScale(Renderer* renderer, float resolution, RasterPlan* out);

// The box-dependent half. Applies the kMaxDim cap (which can lower
// io->rasterScale), then sets io->box / widthPx / heightPx per `fit`.
// `guardTexels` is only meaningful for stableGrid, and must be 0 for exact.
// Returns false for a degenerate box or a scale the cap drove to zero.
bool fitRasterToBox(const AABB& box,
                    uint32_t guardTexels,
                    RasterFit fit,
                    RasterPlan* io);

// Keeps using an allocation at least (widthPx, heightPx), for a caller whose
// content shrank but which does not want to re-allocate yet. Grows io->box
// right and bottom only -- io->box.left/top is what beginComposite's pixel snap
// reads, so moving it would shift the composite -- keeping
// box.width() * rasterScale == widthPx exact. A no-op when the plan is already
// at least that large.
void holdRasterAllocation(uint32_t widthPx, uint32_t heightPx, RasterPlan* io);

// Rounds a raster dimension up so that content which wobbles by a few texels
// keeps the same allocation. Multiples of 64: at kMaxDim = 2048 a finer-grained
// ladder buys nothing (see the comment on the implementation), and the slack is
// at most 63 texels on an axis -- 3% of a full-size raster, and 32 KB for the
// pair of canvases a mask holds at the 64-texel floor.
uint32_t bucketedRasterDim(uint32_t n);

// What to do with an existing allocation now that the content needs a different
// size. Growth is immediate; shrinking waits, because content that is animating
// through a size will otherwise re-allocate every frame.
struct RasterResize
{
    // True when the caller has to mint new canvases at widthPx x heightPx. The
    // caller must also mark its raster dirty: new textures hold nothing.
    bool reallocate = false;
    uint32_t widthPx = 0;
    uint32_t heightPx = 0;
};

// Consecutive decisions that wanted to shrink before one is acted on. 30 draws
// is half a second at 60Hz -- long enough that a rotation or a scale tween
// passes through without churn, short enough that a mask which has settled
// gives its oversized texture back promptly.
constexpr uint8_t kShrinkDraws = 30;

// `needW`/`needH` are what the content wants right now (pre-bucketing),
// `haveW`/`haveH` what is already allocated (0 for nothing yet). `streak` is
// owned by the caller, one per cached raster, and this updates it.
RasterResize decideRasterResize(uint32_t needW,
                                uint32_t needH,
                                uint32_t haveW,
                                uint32_t haveH,
                                uint8_t* streak);

// Opens a content bracket on `canvas` and hands back a renderer already
// transformed so that drawing in local coordinates lands in the canvas at
// plan.rasterScale texels per unit. Closes the bracket on destruction.
//
// renderer() is null when the host could not open an offscreen frame; nothing
// was drawn and no bracket is ended, so the canvas still holds whatever it held
// before (uninitialized on a first attempt). The caller should drop the canvas
// and retry on a later frame rather than compositing a hole.
class CanvasContentScope
{
public:
    CanvasContentScope(cmd::DeferredCanvasHost* host,
                       gpu::RenderCanvas* canvas,
                       const RasterPlan& plan,
                       uint32_t clearColor = 0);
    ~CanvasContentScope();

    CanvasContentScope(const CanvasContentScope&) = delete;
    CanvasContentScope& operator=(const CanvasContentScope&) = delete;

    Renderer* renderer() const { return m_renderer; }

private:
    cmd::DeferredCanvasHost* m_host;
    gpu::RenderCanvas* m_canvas;
    Renderer* m_renderer;
};

// Which renderer a composite should be issued through, and the opacity it has
// to supply itself.
struct CompositePlacement
{
    // Never null: falls back to the renderer that was passed in.
    Renderer* renderer = nullptr;
    // Stays 1 on the in-place path, where the interrupted renderer still
    // carries its own modulated opacity and folds it into the draw.
    float opacity = 1.0f;
};

// Places a composite of an already-rasterized canvas: picks the renderer,
// saves, applies the transform and pixel snap, and maps the raster's pixel
// span back onto the box the pixels were rasterized for. The caller then issues
// its own draw (a drawImage, or anything else spanning (widthPx, heightPx)) and
// calls endComposite.
//
// `compositedRasterScale` is the scale the pixels were actually rasterized at,
// which is not necessarily plan.rasterScale: a cache may be compositing a
// raster built on an earlier frame. Deriving the inverse from the real scale is
// what keeps the mapping pixel exact.
//
// `compositedBox` is the same idea for placement. Pass the box those pixels
// were rasterized for, which is not plan.box when the plan was re-fitted this
// frame but the pixels were not re-rastered. Defaults to plan.box, which is
// correct for any caller whose box does not move.
CompositePlacement beginComposite(Renderer* renderer,
                                  cmd::DeferredCanvasHost* host,
                                  const RasterPlan& plan,
                                  float compositedRasterScale);
CompositePlacement beginComposite(Renderer* renderer,
                                  cmd::DeferredCanvasHost* host,
                                  const RasterPlan& plan,
                                  float compositedRasterScale,
                                  const AABB& compositedBox);

void endComposite(const CompositePlacement& placement);

} // namespace offscreen
} // namespace rive

#endif // RIVE_CANVAS
#endif // _RIVE_OFFSCREEN_RASTER_HPP_
