#include "rive/offscreen_raster.hpp"

#ifdef RIVE_CANVAS

#include "rive/renderer.hpp"
#include "rive/renderer/cmd/deferred_canvas_host.hpp"
#include "rive/renderer/render_canvas.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>

namespace rive
{
namespace offscreen
{

bool planRasterScale(Renderer* renderer, float resolution, RasterPlan* out)
{
    // How many device pixels one local unit covers right now: viewport zoom
    // times window density times whatever scale the mount transform adds. This
    // is the number naive sizing ignores -- rasterizing in local units means
    // that on a 2x display at 100% zoom a resolution-1 raster is already a
    // half-resolution image being magnified, which is what softens text.
    // Reading it here (rather than at replay) is why DeferredRenderer shadows
    // its CTM: the raster size has to be chosen while recording.
    out->haveCtm = renderer->currentTransform(&out->ctm);
    // Read before rasterizing, for the same reason as the CTM: if the host
    // hands back a fresh renderer to composite through, that renderer starts
    // at opacity 1 and the enclosing modulateOpacity() scope has to be carried
    // over by hand.
    out->haveOpacity =
        renderer->currentModulatedOpacity(&out->modulatedOpacity);

    float deviceScale = 1.0f;
    if (out->haveCtm)
    {
        const float s = out->ctm.findMaxScale();
        if (std::isfinite(s) && s > 0.0f)
        {
            // Quantize to sixteenths so scrubbing zoom does not re-raster and
            // re-allocate on every frame. Rounding up keeps the raster at least
            // as fine as the screen, and the scales a user actually rests at
            // (1, 1.5, 2, 3, 4) are already multiples of 1/16, so the cases
            // that matter stay pixel exact rather than merely close.
            constexpr float kScaleQuantum = 16.0f;
            deviceScale = std::ceil(s * kScaleQuantum) / kScaleQuantum;
        }
    }

    constexpr float kMinRes = 0.01f;
    constexpr float kMaxRes = 8.0f;
    const float res =
        std::min<float>(std::max<float>(resolution, kMinRes), kMaxRes);

    // Texels per local unit. resolution 1 means one texel per screen pixel, so
    // the composite is a 1:1 blit; 2 is a genuine 2x supersample of what the
    // screen shows. A renderer that cannot report a transform leaves
    // deviceScale at 1, which is the old local-unit meaning.
    const float rasterScale = res * deviceScale;
    if (!(rasterScale > 0.0f) || !std::isfinite(rasterScale))
    {
        return false;
    }
    out->rasterScale = rasterScale;
    return true;
}

uint32_t bucketedRasterDim(uint32_t n)
{
    // A fixed 64 rather than a ladder that widens with n. The obvious
    // refinement -- a quantum of n/16, so the slack stays a fixed *fraction* --
    // degenerates at kMaxDim = 2048: the next step up, 128, would only be
    // reached at n >= 2048, which is the cap itself. So 64 is the whole ladder
    // until kMaxDim grows.
    constexpr uint32_t kQuantum = 64;
    if (n >= kMaxDim)
    {
        return kMaxDim;
    }
    const uint32_t bucketed = ((n + kQuantum - 1) / kQuantum) * kQuantum;
    return std::min(bucketed, kMaxDim);
}

RasterResize decideRasterResize(uint32_t needW,
                                uint32_t needH,
                                uint32_t haveW,
                                uint32_t haveH,
                                uint8_t* streak)
{
    const uint32_t wantW = bucketedRasterDim(needW);
    const uint32_t wantH = bucketedRasterDim(needH);

    RasterResize out;
    if (haveW == 0 || haveH == 0)
    {
        *streak = 0;
        return {true, wantW, wantH};
    }

    if (needW > haveW || needH > haveH)
    {
        // Grow immediately: a raster smaller than its content crops, and a crop
        // is visible where an oversized texture is only wasteful. Only the axis
        // that is actually short grows, so growing one axis never sneaks an
        // un-hysteresised shrink onto the other.
        *streak = 0;
        out.reallocate = true;
        out.widthPx = needW > haveW ? wantW : haveW;
        out.heightPx = needH > haveH ? wantH : haveH;
        return out;
    }

    if (wantW < haveW || wantH < haveH)
    {
        // Wants to shrink. Wait: content animating through a size would
        // otherwise re-allocate every frame, and re-allocating is far more
        // expensive than carrying a texture that is one bucket too big.
        // Increment before comparing, so the kShrinkDraws'th consecutive
        // shrink request is the one that acts rather than the one after it.
        ++(*streak);
        if (*streak < kShrinkDraws)
        {
            out.widthPx = haveW;
            out.heightPx = haveH;
            return out;
        }
        *streak = 0;
        return {true, wantW, wantH};
    }

    *streak = 0;
    out.widthPx = haveW;
    out.heightPx = haveH;
    return out;
}

bool fitRasterToBox(const AABB& box,
                    uint32_t guardTexels,
                    RasterFit fit,
                    RasterPlan* io)
{
    assert(fit == RasterFit::stableGrid || guardTexels == 0);

    const float w = box.width();
    const float h = box.height();
    if (!(w > 0.0f) || !(h > 0.0f))
    {
        return false;
    }

    // Cap by shrinking the scale, not by clamping one axis: clamping a single
    // dimension would make the two axes disagree about the mapping, and the
    // composite inverts one scale for both. Uniform coarsening at least keeps
    // the image undistorted.
    //
    // stableGrid holds back room for the guard band and for the texel either
    // end of the origin snap can add, so the cap still holds after they are
    // applied rather than being clamped away afterwards.
    const uint32_t reserve =
        fit == RasterFit::stableGrid ? 2u * guardTexels + 2u : 0u;
    const float usableDim =
        static_cast<float>(kMaxDim > reserve ? kMaxDim - reserve : 1u);
    const float maxScale = std::min<float>(usableDim / w, usableDim / h);

    const float rasterScale = std::min<float>(io->rasterScale, maxScale);
    if (!(rasterScale > 0.0f) || !std::isfinite(rasterScale))
    {
        return false;
    }
    io->rasterScale = rasterScale;

    if (fit == RasterFit::exact)
    {
        io->box = box;
        io->widthPx = static_cast<uint32_t>(
            std::min<float>(std::max<float>(std::ceil(w * rasterScale), 1.0f),
                            static_cast<float>(kMaxDim)));
        io->heightPx = static_cast<uint32_t>(
            std::min<float>(std::max<float>(std::ceil(h * rasterScale), 1.0f),
                            static_cast<float>(kMaxDim)));
        return true;
    }

    // Snap the origin down to the raster's own texel grid, anchored at local 0
    // -- NOT to the tight box's own edge. If the origin tracked the content,
    // the content would land at the same sub-texel phase in every raster, and
    // beginComposite's pixel snap would then quantize the composite to whole
    // device pixels: sub-pixel motion would judder. A grid fixed in local space
    // lets the phase vary continuously, exactly as a fixed box does.
    const float guard = static_cast<float>(guardTexels);
    const float leftTx = std::floor(box.left() * rasterScale) - guard;
    const float topTx = std::floor(box.top() * rasterScale) - guard;
    const float rightTx = std::ceil(box.right() * rasterScale) + guard;
    const float bottomTx = std::ceil(box.bottom() * rasterScale) + guard;
    if (!std::isfinite(leftTx) || !std::isfinite(topTx) ||
        !std::isfinite(rightTx) || !std::isfinite(bottomTx))
    {
        return false;
    }

    const uint32_t widthPx = bucketedRasterDim(static_cast<uint32_t>(
        std::min<float>(std::max<float>(rightTx - leftTx, 1.0f),
                        static_cast<float>(kMaxDim))));
    const uint32_t heightPx = bucketedRasterDim(static_cast<uint32_t>(
        std::min<float>(std::max<float>(bottomTx - topTx, 1.0f),
                        static_cast<float>(kMaxDim))));

    io->widthPx = widthPx;
    io->heightPx = heightPx;
    // The box spans exactly the chosen pixel size, so the composite's
    // 1 / rasterScale inverse maps the raster onto it with nothing left over.
    io->box = AABB(leftTx / rasterScale,
                   topTx / rasterScale,
                   (leftTx + static_cast<float>(widthPx)) / rasterScale,
                   (topTx + static_cast<float>(heightPx)) / rasterScale);
    return true;
}

void holdRasterAllocation(uint32_t widthPx, uint32_t heightPx, RasterPlan* io)
{
    if (widthPx <= io->widthPx && heightPx <= io->heightPx)
    {
        return;
    }
    const uint32_t w = std::max(widthPx, io->widthPx);
    const uint32_t h = std::max(heightPx, io->heightPx);
    // Right and bottom only. io->box.left/top is what beginComposite's pixel
    // snap reads; moving it would shift the composite on screen.
    io->box = AABB(io->box.left(),
                   io->box.top(),
                   io->box.left() + static_cast<float>(w) / io->rasterScale,
                   io->box.top() + static_cast<float>(h) / io->rasterScale);
    io->widthPx = w;
    io->heightPx = h;
}

bool planRaster(Renderer* renderer,
                const AABB& box,
                float resolution,
                RasterPlan* out)
{
    RasterPlan plan;
    if (!planRasterScale(renderer, resolution, &plan))
    {
        return false;
    }
    if (!fitRasterToBox(box, 0, RasterFit::exact, &plan))
    {
        return false;
    }
    *out = plan;
    return true;
}

CanvasContentScope::CanvasContentScope(cmd::DeferredCanvasHost* host,
                                       gpu::RenderCanvas* canvas,
                                       const RasterPlan& plan,
                                       uint32_t clearColor) :
    m_host(host), m_canvas(canvas), m_renderer(nullptr)
{
    if (host == nullptr || canvas == nullptr)
    {
        return;
    }
    // beginCanvasContent hands back a recording renderer whose frame targets
    // the canvas texture. Draw content into it at exactly rasterScale texels
    // per local unit -- not widthPx/w, which would bake ceil()'s rounding into
    // the mapping. Curve flattening and feathering read this transform, so this
    // is also what decides how finely the content is tessellated.
    m_renderer = host->beginCanvasContent(canvas, clearColor);
    if (m_renderer == nullptr)
    {
        return;
    }
    m_renderer->save();
    m_renderer->transform(Mat2D::fromScale(plan.rasterScale, plan.rasterScale));
    m_renderer->translate(-plan.box.left(), -plan.box.top());
}

CanvasContentScope::~CanvasContentScope()
{
    if (m_renderer == nullptr)
    {
        // Nothing was drawn, so never end a bracket that never began.
        return;
    }
    m_renderer->restore();
    m_host->endCanvasContent(m_canvas);
}

CompositePlacement beginComposite(Renderer* renderer,
                                  cmd::DeferredCanvasHost* host,
                                  const RasterPlan& plan,
                                  float compositedRasterScale,
                                  const AABB& compositedBox)
{
    // Some hosts cannot composite through the renderer that was drawing when
    // the offscreen frame interrupted it, and hand back a clean one instead.
    // It shares no state, so the current transform and the modulated opacity
    // both have to be re-applied by hand -- and it inherits no clip. Known
    // limitation: on that path (WebGL today) an ancestor's clip does not
    // constrain the composite, so cached content under a clipping shape can
    // paint outside it where the vector draw would have been cropped.
    Renderer* composite = renderer;
    float compositeOpacity = 1.0f;
    if (host != nullptr)
    {
        if (Renderer* fresh = host->compositeRenderer())
        {
            if (plan.haveCtm && plan.haveOpacity)
            {
                composite = fresh;
                compositeOpacity = plan.modulatedOpacity;
            }
        }
    }
    composite->save();
    if (composite != renderer)
    {
        composite->transform(plan.ctm);
    }
    // Pixel snap. Even at a perfectly matched scale, an origin that lands on a
    // half pixel makes every bilinear tap a blend of two texels -- a box blur
    // over the whole image, and the reason cache-as-bitmap has historically
    // needed a pixelSnapping switch. Only meaningful when the transform is axis
    // aligned; under rotation or skew there is no pixel grid to snap to.
    if (plan.haveCtm && plan.ctm.xy() == 0.0f && plan.ctm.yx() == 0.0f &&
        plan.ctm.xx() != 0.0f && plan.ctm.yy() != 0.0f)
    {
        const Vec2D deviceOrigin =
            plan.ctm * Vec2D(compositedBox.left(), compositedBox.top());
        const Vec2D delta = {std::round(deviceOrigin.x) - deviceOrigin.x,
                             std::round(deviceOrigin.y) - deviceOrigin.y};
        // The nudge is in device space but transform() concatenates in local
        // space, so push it back through the (diagonal) linear part.
        composite->translate(delta.x / plan.ctm.xx(), delta.y / plan.ctm.yy());
    }
    composite->translate(compositedBox.left(), compositedBox.top());
    // The exact inverse of the scale the content was rasterized at. Deriving it
    // from w / widthPx instead would fold ceil()'s rounding into the mapping,
    // leaving a fractional-width box permanently resampled at ~0.995x -- a blur
    // on every pixel for nothing. The cost is that the quad reaches up to one
    // texel past the box on the right and bottom, which is the sliver that
    // rounding allocated anyway.
    const float invScale = 1.0f / compositedRasterScale;
    composite->transform(Mat2D::fromScale(invScale, invScale));

    return {composite, compositeOpacity};
}

CompositePlacement beginComposite(Renderer* renderer,
                                  cmd::DeferredCanvasHost* host,
                                  const RasterPlan& plan,
                                  float compositedRasterScale)
{
    return beginComposite(renderer,
                          host,
                          plan,
                          compositedRasterScale,
                          plan.box);
}

void endComposite(const CompositePlacement& placement)
{
    placement.renderer->restore();
}

} // namespace offscreen
} // namespace rive

#endif // RIVE_CANVAS
