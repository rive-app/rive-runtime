/*
 * Copyright 2026 Rive
 */

// Pixel-level validation of the layer-mask offscreen path.
//
// STEP 3 of the layer-mask work composites the masked content *unmasked* on
// purpose, and this is the test that makes that worth doing. It proves the
// entire geometry chain -- one raster plan shared by both canvases, the two
// CanvasContentScopes, the composite placement, the pixel snap, the exact
// raster-scale inverse, the opacity carry-over -- against a real GPU context,
// with zero dependency on the mask shader that does not exist yet.
//
// If this passes, the only thing left that can be wrong in step 4 is the blend.
//
// As in bitmap_cache_pixel_test.cpp the golden is a second render rather than a
// stored PNG: nothing to rebaseline, no per-backend drift, and no way to bless
// a blurry baseline. Here the golden is the plain vector draw of the same
// content, and the masked render has to match it.
//
// The fixture is built in code, not loaded: no .riv can carry a LayerMask until
// the editor can author one.

#include "common/testing_window.hpp"

#ifdef RIVE_CANVAS

#include "common/testing_window_deferred_sink.hpp"
#include "rive/artboard.hpp"
#include "rive/drawable_flag.hpp"
#include "rive/layer_mask.hpp"
#include "rive/math/mat2d.hpp"
#include "rive/node.hpp"
#include "rive/renderer/cmd/deferred_replayer.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/shapes/paint/feather.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"

#include <catch.hpp>
#include <cmath>
#include <cstdio>
#include <memory>
#include <vector>

using namespace rive;

namespace
{
constexpr int kArtboardSize = 200;

struct Diff
{
    uint32_t maxChannel = 0;
    double meanChannel = 0;
    double significantFraction = 0;

    static constexpr uint32_t kChannelEpsilon = 2;

    static Diff between(const std::vector<uint8_t>& a,
                        const std::vector<uint8_t>& b)
    {
        REQUIRE(a.size() == b.size());
        REQUIRE(a.size() % 4 == 0);
        Diff d;
        uint64_t total = 0;
        size_t significant = 0;
        const size_t pixels = a.size() / 4;
        for (size_t p = 0; p < pixels; p++)
        {
            uint32_t worst = 0;
            for (size_t c = 0; c < 4; c++)
            {
                uint32_t delta = static_cast<uint32_t>(
                    std::abs(static_cast<int>(a[p * 4 + c]) -
                             static_cast<int>(b[p * 4 + c])));
                total += delta;
                worst = std::max(worst, delta);
            }
            d.maxChannel = std::max(d.maxChannel, worst);
            if (worst > kChannelEpsilon)
            {
                significant++;
            }
        }
        d.meanChannel = static_cast<double>(total) / (pixels * 4);
        d.significantFraction = static_cast<double>(significant) / pixels;
        return d;
    }

    void report(const char* what) const
    {
        printf("[layer-mask] %s: max channel %u, mean %.4f, %.4f%% of pixels "
               "off by more than %u\n",
               what,
               maxChannel,
               meanChannel,
               significantFraction * 100.0,
               kChannelEpsilon);
    }
};

// A masked render of nothing and a vector render of nothing match perfectly, so
// without this guard a fixture that built wrong would pass every comparison.
// Total of the R, G and B channels over the whole image. The background is
// black, so it contributes nothing, and every masked pixel is the content
// scaled by the mask's factor -- which makes this sum proportional to that
// factor and therefore a direct readout of it.
uint64_t sumRGB(const std::vector<uint8_t>& px)
{
    uint64_t total = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4)
    {
        total += px[p];
        total += px[p + 1];
        total += px[p + 2];
    }
    return total;
}

size_t nonBackgroundPixels(const std::vector<uint8_t>& px)
{
    size_t count = 0;
    for (size_t p = 0; p + 3 < px.size(); p += 4)
    {
        if (px[p] != 0 || px[p + 1] != 0 || px[p + 2] != 0)
        {
            count++;
        }
    }
    return count;
}

void addFilledRect(Artboard& artboard,
                   Node* parent,
                   float x,
                   float y,
                   float size,
                   uint32_t color)
{
    auto* shape = new Shape();
    shape->x(x);
    shape->y(y);
    artboard.addObject(shape);
    shape->parentId(artboard.idOf(parent));

    auto* rect = new Rectangle();
    rect->width(size);
    rect->height(size);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    auto* fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));

    auto* solid = new SolidColor();
    solid->colorValue(color);
    artboard.addObject(solid);
    solid->parentId(artboard.idOf(fill));
}

// Same as addFilledRect but with a Feather on the fill, which reaches 1.5x its
// strength past the shape's own geometry. That overhang is the thing tight
// bounds most easily gets wrong, and the thing a cropped raster cuts off.
void addFeatheredRect(Artboard& artboard,
                      Node* parent,
                      float x,
                      float y,
                      float size,
                      uint32_t color,
                      float feather)
{
    auto* shape = new Shape();
    shape->x(x);
    shape->y(y);
    artboard.addObject(shape);
    shape->parentId(artboard.idOf(parent));

    auto* rect = new Rectangle();
    rect->width(size);
    rect->height(size);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    auto* fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));

    auto* solid = new SolidColor();
    solid->colorValue(color);
    artboard.addObject(solid);
    solid->parentId(artboard.idOf(fill));

    auto* blur = new Feather();
    blur->strength(feather);
    artboard.addObject(blur);
    blur->parentId(artboard.idOf(fill));
}

struct MaskSpec
{
    bool present = true;
    LayerMaskMode mode = LayerMaskMode::alpha;
    uint32_t color = 0xffffffff;
    // Covering the whole artboard makes an alpha mask an identity, which is the
    // single most useful assertion available: it catches every sampling,
    // placement, snapping and scale-inversion mistake at once.
    bool coversEverything = false;
    // Wraps part of the masked subtree in a second, inner LayerMask, so the
    // content canvas of the outer mask has another canvas bracket opening
    // inside it.
    bool nested = false;
    // Adds a feathered square, whose blur reaches well past its own geometry.
    bool feathered = false;
};

// `spec.present == false` renders the same content with no mask object and the
// would-be source hidden -- plain vectors, which is the golden.
std::vector<uint8_t> renderOnce(TestingWindow* window,
                                MaskSpec spec,
                                float resolution,
                                float deviceScale,
                                float rotationRadians = 0.f,
                                bool disableRasterOrdering = false)
{
    auto* rc = window->renderContext();
    REQUIRE(rc != nullptr);

    cmd::DeferredSession session(rive::ore::ReplayCaps{});
    session.bindRenderContext(rc);

    Artboard artboard(&session);
    artboard.addObject(&artboard);

    // The subtree under test: two overlapping translucent squares, so the
    // comparison is sensitive to group-isolation mistakes as well as to
    // placement, plus one opaque square away from the overlap.
    auto* masked = new Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    addFilledRect(artboard, masked, 30.f, 30.f, 80.f, 0x99ff2020);
    addFilledRect(artboard, masked, 70.f, 70.f, 80.f, 0x9920ff20);
    if (spec.feathered)
    {
        // Centred at 150 with a 40 side, so the square spans 130..170 and its
        // blur reaches 1.5 * 12 = 18 further, out to 112..188. Clear of the
        // artboard edge at 200, so the clamp cannot be what decides the box.
        addFeatheredRect(artboard,
                         masked,
                         150.f,
                         150.f,
                         40.f,
                         0xffffcc20,
                         12.f);
    }
    Node* extraHidden = nullptr;
    if (spec.nested)
    {
        // The blue square gets its own mask, so its bracket opens while the
        // outer mask's content canvas is still open.
        auto* inner = new Node();
        artboard.addObject(inner);
        inner->parentId(artboard.idOf(masked));
        addFilledRect(artboard, inner, 150.f, 30.f, 40.f, 0xff2020ff);

        auto* innerSource = new Node();
        artboard.addObject(innerSource);
        innerSource->parentId(artboard.idOf(&artboard));
        addFilledRect(artboard,
                      innerSource,
                      kArtboardSize * .5f,
                      kArtboardSize * .5f,
                      kArtboardSize * 2.f,
                      0xffffffff);

        if (spec.present)
        {
            auto* innerMask = new LayerMask();
            artboard.addObject(innerMask);
            innerMask->parentId(artboard.idOf(inner));
            innerMask->sourceId(artboard.idOf(innerSource));
        }
        else
        {
            // The golden must carry no masks at all, so this source has to be
            // hidden by hand like the outer one below.
            extraHidden = innerSource;
        }
    }
    else
    {
        addFilledRect(artboard, masked, 150.f, 30.f, 40.f, 0xff2020ff);
    }

    // The coverage source, always present so both renders have identical object
    // graphs; only its visibility and the mask differ.
    auto* sourceGroup = new Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    if (spec.coversEverything)
    {
        // Centred, and oversized: a Rectangle is centred on its origin, so a
        // kArtboardSize square at (0,0) would only cover a quadrant.
        addFilledRect(artboard,
                      sourceGroup,
                      kArtboardSize * .5f,
                      kArtboardSize * .5f,
                      kArtboardSize * 2.f,
                      spec.color);
    }
    else
    {
        addFilledRect(artboard, sourceGroup, 20.f, 20.f, 120.f, spec.color);
    }

    if (spec.present)
    {
        auto* mask = new LayerMask();
        artboard.addObject(mask);
        mask->parentId(artboard.idOf(masked));
        mask->sourceId(artboard.idOf(sourceGroup));
        mask->resolution(resolution);
        mask->maskModeValue(static_cast<uint8_t>(spec.mode));
    }

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    // After initialize, not before: LayoutComponent::widthChanged reaches
    // artboard()->markLayoutDirty(), and artboard() is only wired up once
    // onAddedDirty has run. A real .riv sets these through deserialize(), which
    // bypasses the setter entirely.
    artboard.width(static_cast<float>(kArtboardSize));
    artboard.height(static_cast<float>(kArtboardSize));

    if (!spec.present)
    {
        // No mask means nothing suppresses the source, so hide it by hand to
        // leave the same visible content the masked render produces.
        auto hide = [](Node* group) {
            group->forAll([](Component* c) -> bool {
                if (c->is<Drawable>())
                {
                    c->as<Drawable>()->drawableFlags(
                        c->as<Drawable>()->drawableFlags() |
                        static_cast<uint16_t>(DrawableFlag::Hidden));
                }
                return true;
            });
        };
        hide(sourceGroup);
        if (extraHidden != nullptr)
        {
            hide(extraHidden);
        }
    }

    const int w = static_cast<int>(std::ceil(kArtboardSize * deviceScale)) + 8;
    const int h = static_cast<int>(std::ceil(kArtboardSize * deviceScale)) + 8;
    window->resize(w, h);

    auto windowRenderer =
        window->beginFrame({.name = "layer_mask",
                            .clearColor = 0xff000000,
                            .disableRasterOrdering = disableRasterOrdering});
    auto mainDesc = rc->frameDescriptor();

    artboard.advance(0.0f);
    Renderer* screen = session.screenRenderer();
    screen->save();
    screen->transform(Mat2D::fromScale(deviceScale, deviceScale));
    if (rotationRadians != 0.f)
    {
        // Around the artboard's middle, so the content stays on screen. A
        // rotated CTM is not axis aligned, which turns off beginComposite's
        // pixel snap and leaves the composite sampling the raster bilinearly --
        // the case the guard band exists for.
        const float mid = kArtboardSize * .5f;
        screen->translate(mid, mid);
        screen->transform(Mat2D::fromRotation(rotationRadians));
        screen->translate(-mid, -mid);
    }
    artboard.drawInternal(screen);
    screen->restore();

    cmd::DeferredFrame frame = cmd::snapshotFrame(session);
    session.resetFrame();
    rive_tests::TestingWindowFrameSink sink(window, rc, mainDesc);
    cmd::DeferredReplayer replayer;
    replayer.replayFrame(frame, sink);
    if (spec.present && !disableRasterOrdering)
    {
        // Two canvases per mask -- coverage and content. If a mask silently
        // fell back to an inline draw every comparison below would be
        // tautological.
        CHECK(sink.canvasFrames() == (spec.nested ? 4 : 2));
    }
    else
    {
        // No mask, or a frame that cannot apply one: either way nothing should
        // have rasterized offscreen.
        CHECK(sink.canvasFrames() == 0);
    }

    std::vector<uint8_t> pixels;
    window->endFrame(&pixels);
    return pixels;
}

std::unique_ptr<TestingWindow> makeWindow()
{
#if defined(__APPLE__)
    return std::unique_ptr<TestingWindow>(TestingWindow::MakeMetalTexture({}));
#else
    return nullptr;
#endif
}
} // namespace

TEST_CASE("a fully covering opaque mask is an identity", "[layer-mask-pixel]")
{
    // The highest-value single assertion available. A mask that covers
    // everything at alpha 1 must leave the layer exactly as the vector draw
    // left it -- so this catches any error in the raster plan, the shared box
    // and scale, the composite placement, the pixel snap or the scale inverse,
    // none of which a partial mask would isolate from the masking itself.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }

    for (float deviceScale : {1.0f, 2.0f})
    {
        auto masked = renderOnce(window.get(),
                                 {.present = true,
                                  .mode = LayerMaskMode::alpha,
                                  .color = 0xffffffff,
                                  .coversEverything = true},
                                 1.0f,
                                 deviceScale);
        auto vectors =
            renderOnce(window.get(), {.present = false}, 1.0f, deviceScale);

        REQUIRE(nonBackgroundPixels(vectors) > 1000);
        REQUIRE(nonBackgroundPixels(masked) > 1000);

        auto d = Diff::between(masked, vectors);
        char label[64];
        snprintf(label, sizeof(label), "identity mask at %.1fx", deviceScale);
        d.report(label);
        CHECK(d.maxChannel <= 2);
        CHECK(d.significantFraction == 0.0);
    }
}

TEST_CASE("alpha and inverted alpha partition the layer", "[layer-mask-pixel]")
{
    // out(alpha) + out(invertedAlpha) == unmasked, per pixel: the two modes
    // multiply by f and 1-f and the content is common, so they have to sum back
    // to it. Catches a sign error that either mode alone would happily pass,
    // and it holds across the mask's antialiased edge too.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }

    auto alpha = renderOnce(window.get(),
                            {.present = true, .mode = LayerMaskMode::alpha},
                            1.0f,
                            1.0f);
    auto inverted =
        renderOnce(window.get(),
                   {.present = true, .mode = LayerMaskMode::invertedAlpha},
                   1.0f,
                   1.0f);
    auto vectors = renderOnce(window.get(), {.present = false}, 1.0f, 1.0f);

    REQUIRE(alpha.size() == vectors.size());
    REQUIRE(nonBackgroundPixels(alpha) > 100);
    REQUIRE(nonBackgroundPixels(inverted) > 100);

    // The frame is cleared to opaque black, so the background contributes once
    // to each render and twice to the sum; subtract it back out.
    uint32_t worst = 0;
    size_t significant = 0;
    const size_t pixels = alpha.size() / 4;
    for (size_t p = 0; p < pixels; p++)
    {
        for (size_t c = 0; c < 4; c++)
        {
            const size_t i = p * 4 + c;
            const int background = (c == 3) ? 255 : 0;
            const int sum = static_cast<int>(alpha[i]) +
                            static_cast<int>(inverted[i]) - background;
            const int delta = std::abs(sum - static_cast<int>(vectors[i]));
            worst = std::max(worst, static_cast<uint32_t>(delta));
            if (delta > 2)
            {
                significant++;
            }
        }
    }
    printf("[layer-mask] alpha + invertedAlpha vs unmasked: worst %u, %zu "
           "channels off by more than 2\n",
           worst,
           significant);
    CHECK(significant == 0);
}

TEST_CASE("an opaque white mask reads the same by luminance as by alpha",
          "[layer-mask-pixel]")
{
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }
    auto byAlpha = renderOnce(window.get(),
                              {.present = true, .mode = LayerMaskMode::alpha},
                              1.0f,
                              1.0f);
    auto byLuma =
        renderOnce(window.get(),
                   {.present = true, .mode = LayerMaskMode::luminance},
                   1.0f,
                   1.0f);
    REQUIRE(nonBackgroundPixels(byAlpha) > 100);
    auto d = Diff::between(byAlpha, byLuma);
    d.report("opaque white: luminance vs alpha");
    CHECK(d.maxChannel <= 2);
}

TEST_CASE("a half transparent white mask reads the same either way",
          "[layer-mask-pixel]")
{
    // The unmultiply trap. A 50% white mask is premultiplied to
    // (128,128,128,128), so alpha gives f = 0.5 and luminance gives
    // dot((0.5,0.5,0.5), coeffs) = 0.5 -- but only if luminance is taken on the
    // PREMULTIPLIED texel. A shader that "helpfully" unmultiplies first yields
    // f = 1.0 and this fails, which is exactly the bug worth pinning.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }
    auto byAlpha = renderOnce(
        window.get(),
        {.present = true, .mode = LayerMaskMode::alpha, .color = 0x80ffffff},
        1.0f,
        1.0f);
    auto byLuma = renderOnce(window.get(),
                             {.present = true,
                              .mode = LayerMaskMode::luminance,
                              .color = 0x80ffffff},
                             1.0f,
                             1.0f);
    REQUIRE(nonBackgroundPixels(byAlpha) > 100);
    auto d = Diff::between(byAlpha, byLuma);
    d.report("50% white: luminance vs alpha");
    CHECK(d.maxChannel <= 2);
}

TEST_CASE("a feather survives the raster edge", "[layer-mask-pixel]")
{
    // The crop tight bounds most easily causes. A feather reaches 1.5x its
    // strength past its own geometry, so a box measured from geometry alone
    // cuts the blur off along a straight line -- far more visible than the
    // oversized texture it replaced.
    //
    // paint_outset pins the arithmetic; this pins that the raster actually
    // contains what the arithmetic asked for, end to end: measure, outset,
    // size, rasterize, composite.
    //
    // A fully covering alpha mask is an identity, so the masked render has to
    // match the plain vector draw of the same content exactly. Any crop shows
    // up as a diff on the feather's outer edge.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }
    auto masked = renderOnce(window.get(),
                             {.present = true,
                              .mode = LayerMaskMode::alpha,
                              .coversEverything = true,
                              .feathered = true},
                             1.0f,
                             1.0f);
    auto vectors = renderOnce(window.get(),
                              {.present = false, .feathered = true},
                              1.0f,
                              1.0f);
    REQUIRE(nonBackgroundPixels(vectors) > 1000);
    REQUIRE(nonBackgroundPixels(masked) > 1000);

    auto d = Diff::between(masked, vectors);
    d.report("feathered content under an identity mask");
    CHECK(d.maxChannel <= 2);
}

TEST_CASE("a rotated composite still matches the vector draw",
          "[layer-mask-pixel]")
{
    // Every other comparison here runs axis aligned, where beginComposite snaps
    // the raster onto the pixel grid and the mapping is exact. Rotation turns
    // that off -- the snap only applies when the CTM has no skew -- so the
    // composite lands on fractional device pixels and is resampled bilinearly.
    // That exercises the placement, the exact-rasterScale inverse and the box
    // origin together, in the one configuration where nothing is rounding them
    // into agreement.
    //
    // Budgeted rather than exact, because both renders antialias a rotated edge
    // and the masked one resamples twice: into the raster, then out of it. A
    // misplacement of even one pixel shifts whole edges and blows straight past
    // this.
    //
    // Measured at max channel 33 over 1.5% of pixels; the thresholds are that
    // with headroom.
    //
    // Note this does NOT exercise the guard band, despite the obvious guess
    // that it would. Setting kGuardTexels to 0 leaves these numbers unchanged
    // to three decimal places, here and with a magnified composite -- the
    // composite quad spans exactly the image, so a clamped tap at the border
    // reads the edge texel, which is the value that belongs there. The
    // bucketing also grows the box right and bottom, so there is usually
    // transparent padding regardless.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }
    // Deliberately not a multiple of 90 degrees, which would stay axis aligned
    // and keep the snap -- and so would prove nothing.
    constexpr float kRotation = 0.37f;
    auto masked = renderOnce(window.get(),
                             {.present = true,
                              .mode = LayerMaskMode::alpha,
                              .coversEverything = true},
                             1.0f,
                             1.0f,
                             kRotation);
    auto vectors =
        renderOnce(window.get(), {.present = false}, 1.0f, 1.0f, kRotation);
    REQUIRE(nonBackgroundPixels(vectors) > 1000);
    REQUIRE(nonBackgroundPixels(masked) > 1000);

    auto d = Diff::between(masked, vectors);
    d.report("identity mask under rotation");
    CHECK(d.maxChannel <= 48);
    CHECK(d.significantFraction < 0.03);
}

TEST_CASE("luminance weights the channels by Rec.601", "[layer-mask-pixel]")
{
    // The existing luminance cases both use a WHITE source, where luma and
    // alpha agree trivially and any set of coefficients summing to one would
    // pass. This is the case that actually pins them.
    //
    // On an opaque premultiplied texel, dot(rgb, (.30, .59, .11)) is just the
    // coefficient of whichever channel is lit: red keeps 30% of the content,
    // green 59%, blue 11%. Equal weights, or a swapped pair, or a shader
    // reaching for a different standard, all fail here and nowhere else.
    //
    // The constant has to match lum_from_rgb in advanced_blend.glsl, or a
    // luminance mask disagrees with Rive's own luminosity blending -- and the
    // editor, which mirrors the same numbers.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }

    // Opaque white by alpha: f is 1 wherever the source covers, so this is the
    // full-strength reference every luminance render is a fraction of.
    auto reference = renderOnce(window.get(),
                                {.present = true, .mode = LayerMaskMode::alpha},
                                1.0f,
                                1.0f);
    REQUIRE(nonBackgroundPixels(reference) > 100);
    const double full = static_cast<double>(sumRGB(reference));
    REQUIRE(full > 0.0);

    struct Channel
    {
        const char* name;
        uint32_t color;
        double expected;
    };
    const Channel channels[] = {
        {"red", 0xffff0000, 0.30},
        {"green", 0xff00ff00, 0.59},
        {"blue", 0xff0000ff, 0.11},
    };

    double measured[3] = {0, 0, 0};
    for (size_t i = 0; i < 3; i++)
    {
        auto lit = renderOnce(window.get(),
                              {.present = true,
                               .mode = LayerMaskMode::luminance,
                               .color = channels[i].color},
                              1.0f,
                              1.0f);
        measured[i] = static_cast<double>(sumRGB(lit)) / full;
        printf("[layer-mask] luminance %s: kept %.4f of full, expected %.2f\n",
               channels[i].name,
               measured[i],
               channels[i].expected);
        CHECK(measured[i] == Approx(channels[i].expected).margin(0.01));
    }

    // And the ordering, which is the part a wrong-but-normalised coefficient
    // set would still get wrong.
    CHECK(measured[1] > measured[0]);
    CHECK(measured[0] > measured[2]);
}

TEST_CASE("a mask nested inside a masked subtree still composites",
          "[layer-mask-pixel]")
{
    // Nesting is the case that needs DeferredSession to keep a *stack* of open
    // canvas brackets: the inner mask's canvases open while the outer mask's
    // content canvas is still recording, and the inner end has to resume the
    // outer canvas rather than jumping back to the screen. With a single open
    // slot the rest of the outer content is credited to the screen and the
    // frame comes out wrong.
    //
    // Both masks cover everything, so the whole arrangement is an identity and
    // any routing mistake shows up as a difference from the plain vector draw.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }

    auto nested = renderOnce(window.get(),
                             {.present = true,
                              .mode = LayerMaskMode::alpha,
                              .color = 0xffffffff,
                              .coversEverything = true,
                              .nested = true},
                             1.0f,
                             1.0f);
    auto vectors = renderOnce(window.get(),
                              {.present = false, .nested = true},
                              1.0f,
                              1.0f);

    REQUIRE(nonBackgroundPixels(vectors) > 1000);
    REQUIRE(nonBackgroundPixels(nested) > 1000);

    auto d = Diff::between(nested, vectors);
    d.report("nested identity masks");
    CHECK(d.maxChannel <= 2);
    CHECK(d.significantFraction == 0.0);
}

TEST_CASE("a partial mask only ever removes content", "[layer-mask-pixel]")
{
    // Asserted structurally rather than at hand-picked coordinates: the
    // fixture's rectangles are centred on their origins and the readback's
    // channel order is the window's business, so pixel probes encode
    // assumptions that have nothing to do with masking. These three properties
    // are what "masking" actually means, and they hold whatever the geometry.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }
    auto masked = renderOnce(window.get(),
                             {.present = true, .mode = LayerMaskMode::alpha},
                             1.0f,
                             1.0f);
    auto vectors = renderOnce(window.get(), {.present = false}, 1.0f, 1.0f);
    REQUIRE(masked.size() == vectors.size());

    auto isBackground = [](const std::vector<uint8_t>& px, size_t p) {
        return px[p * 4] == 0 && px[p * 4 + 1] == 0 && px[p * 4 + 2] == 0;
    };

    size_t removed = 0;
    size_t addedOrBrightened = 0;
    const size_t pixels = masked.size() / 4;
    for (size_t p = 0; p < pixels; p++)
    {
        const bool wasContent = !isBackground(vectors, p);
        const bool isContent = !isBackground(masked, p);
        if (wasContent && !isContent)
        {
            removed++;
        }
        if (!wasContent && isContent)
        {
            addedOrBrightened++;
        }
        // Masking scales toward zero, so no channel may come out brighter than
        // the unmasked draw. This is the property that would catch a factor
        // applied as (1 + f), or the mask compositing over instead of scaling.
        for (size_t c = 0; c < 3; c++)
        {
            CHECK(static_cast<int>(masked[p * 4 + c]) <=
                  static_cast<int>(vectors[p * 4 + c]) + 2);
        }
    }

    printf("[layer-mask] partial mask: %zu pixels removed, %zu appeared\n",
           removed,
           addedOrBrightened);
    // Something was genuinely masked out...
    CHECK(removed > 100);
    // ...and nothing was conjured up.
    CHECK(addedOrBrightened == 0);
}

TEST_CASE("a frame that cannot mask draws the layer unmasked",
          "[layer-mask-pixel]")
{
    // The documented degrade. applyLayerMask no-ops on any interlock mode but
    // rasterOrdering, so the promise is "unmasked, not wrong" -- but that
    // promise is only keepable if the mask never rasterizes in the first place.
    // Once it has, the layer is unmasked *and* clipped to a box computed as
    // content-and-coverage, which is content the author never asked to lose.
    // So this asserts the strong form: byte-for-byte the plain vector draw, and
    // nothing rasterized offscreen at all.
    auto window = makeWindow();
    if (window == nullptr)
    {
        SUCCEED("no GPU testing window on this platform");
        return;
    }

    const MaskSpec spec = {.present = true, .mode = LayerMaskMode::alpha};

    // Each comparison stays inside one interlock mode. The modes do not agree
    // with each other to the byte -- coverage and blending are computed
    // differently -- so a cross-mode diff would be measuring the renderer, not
    // the mask.
    auto supported = renderOnce(window.get(), spec, 1.0f, 1.0f);
    auto supportedVectors =
        renderOnce(window.get(), {.present = false}, 1.0f, 1.0f);

    // Positive control. Without it a fix that disabled masking outright would
    // pass the real assertion below, and so would a testing window that
    // quietly ignored disableRasterOrdering.
    REQUIRE(supported.size() == supportedVectors.size());
    REQUIRE(nonBackgroundPixels(supported) > 100);
    {
        Diff d = Diff::between(supported, supportedVectors);
        d.report("masked vs vectors, masking supported");
        // The fixture's source does not cover everything, so a working mask
        // must differ from the unmasked draw.
        CHECK(d.significantFraction > 0.0);
    }

    auto degraded = renderOnce(window.get(),
                               spec,
                               1.0f,
                               1.0f,
                               0.f,
                               /*disableRasterOrdering=*/true);
    auto degradedVectors = renderOnce(window.get(),
                                      {.present = false},
                                      1.0f,
                                      1.0f,
                                      0.f,
                                      /*disableRasterOrdering=*/true);
    REQUIRE(degraded.size() == degradedVectors.size());
    REQUIRE(nonBackgroundPixels(degraded) > 100);
    Diff d = Diff::between(degraded, degradedVectors);
    d.report("masked vs vectors, masking unsupported");
    CHECK(d.maxChannel <= 2);
    CHECK(d.significantFraction == 0.0);
}

#endif // RIVE_CANVAS
