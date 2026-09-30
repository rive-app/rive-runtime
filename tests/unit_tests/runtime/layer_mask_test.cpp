/*
 * Copyright 2026 Rive
 *
 * Structural tests for LayerMask bracketing. No GPU: these assert on the flat
 * draw list and on how the draw walk consumes it, which is everything Step 1 of
 * the layer-mask work delivers. The offscreen rasters and the composite come
 * later; until then drawMasked() reports "I did nothing" and masked content
 * draws inline, which several of these cases pin deliberately.
 */

#include <rive/artboard.hpp>
#include <rive/layer_mask.hpp>
#include <rive/offscreen_raster.hpp>
#include <rive/node.hpp>
#include <rive/drawable_flag.hpp>
#include <rive/shapes/clipping_shape.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/draw_rules.hpp>
#include <rive/draw_target_placement.hpp>
#include <rive/draw_target.hpp>
#include <rive/solo.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>

#include <catch.hpp>
#include <algorithm>
#include <string>
#include <vector>

#ifdef RIVE_CANVAS
#include "common/render_context_null.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#endif

namespace
{
// Counts the draws a walk actually issues, so "the source is suppressed" is
// checked by what reaches the renderer rather than by reading the list back.
class CountingRenderer : public rive::NoOpRenderer
{
public:
    int drawPaths = 0;
    int saves = 0;
    int restores = 0;
    void drawPath(rive::RenderPath* path, rive::RenderPaint* paint) override
    {
        drawPaths++;
    }
    void save() override { saves++; }
    void restore() override { restores++; }
};

// A filled one-rectangle shape. The fill matters: Shape::draw iterates its
// ShapePaints, so a paintless shape issues no drawPath and every "did this
// draw?" assertion below would pass vacuously.
rive::Shape* makeShape(rive::Artboard& artboard, const char* name)
{
    auto* shape = new rive::Shape();
    shape->name(name);
    artboard.addObject(shape);

    auto* rect = new rive::Rectangle();
    rect->width(10.0f);
    rect->height(10.0f);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    auto* fill = new rive::Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));

    auto* color = new rive::SolidColor();
    color->colorValue(0xFFFF0000);
    artboard.addObject(color);
    color->parentId(artboard.idOf(fill));

    return shape;
}

// The draw list as a readable string: "S" for a shape, "[" / "]" for a mask
// bracket, "{" / "}" for a source bracket, "c" / "C" for a clip bracket.
std::string listShape(rive::Artboard& artboard)
{
    std::string out;
    for (auto* d = artboard.firstDrawable(); d != nullptr;
         d = d->prevDrawable())
    {
        if (d->isMaskStart())
        {
            out += '[';
        }
        else if (d->isMaskEnd())
        {
            out += ']';
        }
        else if (d->isClipStart())
        {
            out += 'c';
        }
        else if (d->isClipEnd())
        {
            out += 'C';
        }
        else if (d->isProxy())
        {
            // A source-bracket marker is the only remaining proxy kind here.
            auto* m = static_cast<rive::LayerMaskProxyDrawable*>(d);
            out += m->op() == rive::LayerMaskOp::sourceStart ? '{' : '}';
        }
        else
        {
            out += 'S';
        }
    }
    return out;
}
} // namespace

TEST_CASE("a mask brackets its subtree and its source", "[layer-mask]")
{
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));

    auto* a = makeShape(artboard, "A");
    auto* b = makeShape(artboard, "B");
    a->parentId(artboard.idOf(masked));
    b->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    // sortDrawOrder runs from Artboard::update on DrawOrder dirt, which advance
    // drains; initialize() alone leaves the flat list unbuilt.
    artboard.advance(0.0f);

    REQUIRE(mask->source() == sourceGroup);
    REQUIRE(mask->isSelfReferential() == false);
    REQUIRE(mask->sourceDrawables().size() == 1);

    // Both brackets present, each hugging its own range, neither crossing.
    const auto shape = listShape(artboard);
    INFO("draw list: " << shape);
    REQUIRE(shape == "{S}[SS]");
}

TEST_CASE("the mask source is suppressed in the normal pass", "[layer-mask]")
{
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* a = makeShape(artboard, "A");
    a->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);

    {
        // Two shapes exist, but the source must not reach the renderer: the
        // masked content draws inline (drawMasked is still a stub) and the
        // source contributes coverage only.
        CountingRenderer renderer;
        artboard.draw(&renderer);
        REQUIRE(renderer.drawPaths == 1);
    }

    SECTION("sourceDraws puts it back in its natural slot")
    {
        mask->sourceDraws(true);
        CountingRenderer renderer;
        artboard.draw(&renderer);
        REQUIRE(renderer.drawPaths == 2);
    }

    SECTION("an invisible mask is inert, so the source keeps drawing")
    {
        // Suppressing the source while the content draws unmasked would just
        // make the mask art vanish for nothing.
        mask->isVisible(false);
        CountingRenderer renderer;
        artboard.draw(&renderer);
        REQUIRE(renderer.drawPaths == 2);
    }
}

TEST_CASE("a mask bracket nests inside a clip bracket", "[layer-mask]")
{
    // The ordering that matters: an ancestor clip has to constrain the
    // composite, so the mask bracket must sit inside the clip bracket. Crossed
    // brackets would hand the offscreen pass a restore() without its save().
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* clipped = new rive::Node();
    artboard.addObject(clipped);
    clipped->parentId(artboard.idOf(&artboard));

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(clipped));
    auto* a = makeShape(artboard, "A");
    a->parentId(artboard.idOf(masked));

    auto* clipSource = makeShape(artboard, "ClipSrc");
    clipSource->parentId(artboard.idOf(&artboard));
    auto* maskSource = makeShape(artboard, "MaskSrc");
    maskSource->parentId(artboard.idOf(&artboard));

    auto* clip = new rive::ClippingShape();
    artboard.addObject(clip);
    clip->parentId(artboard.idOf(clipped));
    clip->sourceId(artboard.idOf(clipSource));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(maskSource));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);

    const auto shape = listShape(artboard);
    INFO("draw list: " << shape);
    // The clip opens before the mask and closes after it.
    const auto clipOpen = shape.find('c');
    const auto maskOpen = shape.find('[');
    const auto maskClose = shape.find(']');
    const auto clipClose = shape.find('C');
    REQUIRE(clipOpen != std::string::npos);
    REQUIRE(maskOpen != std::string::npos);
    REQUIRE(maskClose != std::string::npos);
    REQUIRE(clipClose != std::string::npos);
    REQUIRE(clipOpen < maskOpen);
    REQUIRE(maskOpen < maskClose);
    REQUIRE(maskClose < clipClose);
}

TEST_CASE("a self-referential mask is dropped rather than recursing",
          "[layer-mask]")
{
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* a = makeShape(artboard, "A");
    a->parentId(artboard.idOf(masked));

    // The source is inside the very subtree the mask applies to.
    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(a));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);
    REQUIRE(mask->isSelfReferential());

    // No bracket at all, and the content still draws.
    const auto shape = listShape(artboard);
    INFO("draw list: " << shape);
    REQUIRE(shape == "S");

    CountingRenderer renderer;
    artboard.draw(&renderer);
    REQUIRE(renderer.drawPaths == 1);
}

TEST_CASE("an unknown mask mode falls back to alpha", "[layer-mask]")
{
    // A number bound to the mode can carry any value.
    rive::LayerMask mask;
    mask.maskModeValue(8);
    CHECK(mask.maskMode() == rive::MaskMode::alpha);
    mask.maskModeValue(3);
    CHECK(mask.maskMode() == rive::MaskMode::invertedLuminance);
}

TEST_CASE("a mask is not a solo option", "[layer-mask]")
{
    // A LayerMask is metadata of its parent, like a ClippingShape. If it
    // counted as a solo child it would silently shift index-based selection.
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* solo = new rive::Solo();
    artboard.addObject(solo);
    solo->parentId(artboard.idOf(&artboard));

    auto* maskSource = makeShape(artboard, "MaskSrc");
    maskSource->parentId(artboard.idOf(&artboard));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(solo));
    mask->sourceId(artboard.idOf(maskSource));

    auto* first = makeShape(artboard, "First");
    auto* second = makeShape(artboard, "Second");
    first->parentId(artboard.idOf(solo));
    second->parentId(artboard.idOf(solo));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);

    // Index 1 is the second real option, not "the shape after the mask".
    solo->updateByIndex(1);
    REQUIRE(solo->activeComponent() == second);
}

TEST_CASE("a mask whose range a draw rule splits is left unbracketed",
          "[layer-mask]")
{
    // Masking a contiguous range is what makes the offscreen pass expressible.
    // A DrawTarget can lift an unrelated drawable into the middle of a
    // subtree's span; masking it too would be silently wrong, so the mask backs
    // off and its content draws unmasked -- and, because the masked range never
    // bracketed, the source keeps drawing rather than vanishing for nothing.
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* a = makeShape(artboard, "A");
    auto* b = makeShape(artboard, "B");
    a->parentId(artboard.idOf(masked));
    b->parentId(artboard.idOf(masked));

    auto* outsider = makeShape(artboard, "Outsider");
    outsider->parentId(artboard.idOf(&artboard));
    auto* maskSource = makeShape(artboard, "MaskSrc");
    maskSource->parentId(artboard.idOf(&artboard));

    // Relocate the outsider to draw immediately before B, i.e. into the middle
    // of the masked group's span. placementValue is left at its default
    // (before), and drawTargetId is set only after initialize(): both
    // DrawRules::drawTargetIdChanged and DrawTarget::placementValueChanged
    // dereference artboard() unguarded, which is null until onAddedDirty runs.
    // A real .riv avoids this because import writes the fields through
    // deserialize() rather than the setters.
    auto* rules = new rive::DrawRules();
    artboard.addObject(rules);
    rules->parentId(artboard.idOf(outsider));
    auto* target = new rive::DrawTarget();
    artboard.addObject(target);
    target->parentId(artboard.idOf(rules));
    target->drawableId(artboard.idOf(b));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(maskSource));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    rules->drawTargetId(artboard.idOf(target));
    artboard.advance(0.0f);

    const auto shape = listShape(artboard);
    INFO("draw list: " << shape);
    // Four plain shapes and no bracket of either kind: the masked span is not
    // contiguous, so the mask is inert.
    REQUIRE(std::count(shape.begin(), shape.end(), 'S') == 4);
    REQUIRE(shape.find('[') == std::string::npos);
    REQUIRE(shape.find('{') == std::string::npos);

    CountingRenderer renderer;
    artboard.draw(&renderer);
    // All four shapes draw: the two masked ones, the interloper, and the mask
    // source, which must not be suppressed when nothing masks anything.
    REQUIRE(renderer.drawPaths == 4);
}

TEST_CASE("a mask nested inside a masked subtree brackets inside it",
          "[layer-mask]")
{
    // Both brackets must nest, never interleave: the walk hands drawMasked a
    // [start, end) range and would run off the list if an inner end marker
    // escaped the outer one.
    rive::NoOpFactory factory;
    rive::Artboard artboard(&factory);
    artboard.addObject(&artboard);

    auto* outer = new rive::Node();
    artboard.addObject(outer);
    outer->parentId(artboard.idOf(&artboard));
    auto* a = makeShape(artboard, "A");
    a->parentId(artboard.idOf(outer));

    // The inner group is the LAST thing in the outer group's span, which is the
    // arrangement that makes the two brackets collide.
    auto* inner = new rive::Node();
    artboard.addObject(inner);
    inner->parentId(artboard.idOf(outer));
    auto* b = makeShape(artboard, "B");
    b->parentId(artboard.idOf(inner));

    auto* outerSource = makeShape(artboard, "OuterSrc");
    outerSource->parentId(artboard.idOf(&artboard));
    auto* innerSource = makeShape(artboard, "InnerSrc");
    innerSource->parentId(artboard.idOf(&artboard));

    auto* outerMask = new rive::LayerMask();
    artboard.addObject(outerMask);
    outerMask->parentId(artboard.idOf(outer));
    outerMask->sourceId(artboard.idOf(outerSource));

    auto* innerMask = new rive::LayerMask();
    artboard.addObject(innerMask);
    innerMask->parentId(artboard.idOf(inner));
    innerMask->sourceId(artboard.idOf(innerSource));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.advance(0.0f);

    const auto shape = listShape(artboard);
    INFO("draw list: " << shape);

    // Both masks bracketed, and both sources too.
    REQUIRE(std::count(shape.begin(), shape.end(), '[') == 2);
    REQUIRE(std::count(shape.begin(), shape.end(), ']') == 2);
    REQUIRE(std::count(shape.begin(), shape.end(), '{') == 2);
    REQUIRE(std::count(shape.begin(), shape.end(), '}') == 2);

    // Properly nested: scanning left to right the depth never goes negative and
    // returns to zero, which is exactly "no interleaving".
    int depth = 0;
    for (char c : shape)
    {
        if (c == '[')
        {
            depth++;
        }
        else if (c == ']')
        {
            depth--;
            REQUIRE(depth >= 0);
        }
    }
    REQUIRE(depth == 0);

    // Neither source draws in the normal pass.
    artboard.advance(0.0f);
    CountingRenderer renderer;
    artboard.draw(&renderer);
    REQUIRE(renderer.drawPaths == 2);
}

#ifdef RIVE_CANVAS
namespace
{
// canvasContentBegin ops in the recorded stream, i.e. how many times something
// actually rasterized offscreen this frame.
size_t countCanvasOpens(const rive::cmd::RenderCommandBuffer& buffer)
{
    using rive::cmd::RenderCmd;
    auto bytes = buffer.commandBytes();
    rive::cmd::CommandReader<uint8_t> reader(bytes, buffer.blobBytes());
    size_t opens = 0;
    uint8_t type;
    while (reader.next(type))
    {
        auto cmd = static_cast<RenderCmd>(type);
        if (cmd > RenderCmd::lastRenderCmd)
        {
            break;
        }
        if (cmd == RenderCmd::canvasContentBegin)
        {
            opens++;
        }
        reader.skip(rive::cmd::payloadSizeOf(cmd));
    }
    return opens;
}
} // namespace

TEST_CASE("a small mask in a large artboard rasters small and sharp",
          "[layer-mask]")
{
    // This is the case the whole change exists for.
    //
    // Sized from the artboard, a mask on a 2000x2000 artboard at
    // devicePixelRatio 2 wants 4000 texels per axis. kMaxDim caps it at 2048,
    // and the cap is applied by lowering the SCALE rather than clamping an axis
    // -- so the masked content was rasterized at roughly half the resolution
    // the screen asked for and then magnified back up. Masking a layer quietly
    // made it blurry, and raising the `resolution` property could not fix it,
    // because the cap already dominated the scale it fed into.
    //
    // Sized from the content, a 10x10 badge needs ~10 texels. The cap never
    // binds, so the scale comes out exactly as requested.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* content = makeShape(artboard, "Content");
    content->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(2000.0f);
    artboard.height(2000.0f);
    // Well inside the artboard. makeShape centres its rectangle on its origin,
    // so a badge left at (0,0) would hang half outside the artboard box -- and
    // that overhang is clamped away (see "content outside the artboard box is
    // still clamped"), which would make the coverage assertions below measure
    // the clamp rather than the fit.
    masked->x(100.0f);
    masked->y(100.0f);
    sourceGroup->x(100.0f);
    sourceGroup->y(100.0f);

    auto* renderer = session.screenRenderer();
    renderer->save();
    // Stand in for a 2x display: this is the scale planRasterScale reads off
    // the renderer, and the number naive local-unit sizing ignores.
    renderer->transform(rive::Mat2D::fromScale(2.0f, 2.0f));
    artboard.advance(0.0f);
    artboard.drawInternal(renderer);
    renderer->restore();

    // resolution 1 x device scale 2, with no coarsening applied on top.
    CHECK(mask->testRasterScale() == Approx(2.0f));

    // And a raster measured in tens of texels rather than thousands. The
    // 64-texel bucket floor is the lower bound, so this is as small as it can
    // get.
    CHECK(mask->testWidthPx() == 64);
    CHECK(mask->testHeightPx() == 64);

    // The box has to still cover the badge it was measured from, guard band and
    // all -- a tight raster that cropped would be worse than the blurry one it
    // replaces. Asserted against what the content itself reports rather than
    // against hard-coded coordinates, so this stays honest if the rectangle's
    // origin convention ever changes.
    rive::AABB contentBounds;
    REQUIRE(content->paintedWorldBounds(&contentBounds) !=
            rive::BoundsFidelity::none);
    REQUIRE(contentBounds.width() == Approx(10.0f));

    const rive::AABB box = mask->testBox();
    CHECK(box.left() <= contentBounds.left());
    CHECK(box.top() <= contentBounds.top());
    CHECK(box.right() >= contentBounds.right());
    CHECK(box.bottom() >= contentBounds.bottom());
}

// Builds an artboard with a masked 10x10 badge and a 10x10 coverage source,
// both at the origin so they overlap. Hands back the mask and the two group
// nodes so a caller can set custom bounds and re-render.
struct MaskFixture
{
    rive::Node* masked = nullptr;
    rive::Shape* content = nullptr;
    rive::Node* sourceGroup = nullptr;
    rive::Shape* source = nullptr;
    rive::LayerMask* mask = nullptr;
};

MaskFixture buildMaskFixture(rive::Artboard& artboard)
{
    MaskFixture f;
    artboard.addObject(&artboard);

    f.masked = new rive::Node();
    artboard.addObject(f.masked);
    f.masked->parentId(artboard.idOf(&artboard));
    f.content = makeShape(artboard, "Content");
    f.content->parentId(artboard.idOf(f.masked));

    f.sourceGroup = new rive::Node();
    artboard.addObject(f.sourceGroup);
    f.sourceGroup->parentId(artboard.idOf(&artboard));
    f.source = makeShape(artboard, "Src");
    f.source->parentId(artboard.idOf(f.sourceGroup));

    f.mask = new rive::LayerMask();
    artboard.addObject(f.mask);
    f.mask->parentId(artboard.idOf(f.masked));
    f.mask->sourceId(artboard.idOf(f.sourceGroup));
    return f;
}

TEST_CASE("an authored rectangle replaces the measured box", "[layer-mask]")
{
    // The escape hatch: instead of measuring the region from the content, the
    // author draws a rectangle and its x/y/width/height become the region both
    // rasters cover. Worth having for content whose own reach cannot be
    // measured
    // -- a scripted drawable paints wherever its script says -- and for
    // deliberately cropping a mask to a region.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    // Well clear of the 10x10 badge at the origin, and four times its size, so
    // the box can only have come from here.
    f.mask->useCustomBounds(true);
    f.mask->boundsX(60.0f);
    f.mask->boundsY(70.0f);
    f.mask->boundsWidth(40.0f);
    f.mask->boundsHeight(40.0f);

    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    const rive::AABB box = f.mask->testBox();
    // Covers the authored rectangle, guard band and all.
    CHECK(box.left() <= 60.0f);
    CHECK(box.top() <= 70.0f);
    CHECK(box.right() >= 100.0f);
    CHECK(box.bottom() >= 110.0f);
    // And it is the rectangle being used, not the badge: a measured box would
    // sit at the origin, nowhere near this.
    CHECK(box.left() > 50.0f);
}

TEST_CASE("a degenerate rectangle falls back to measuring", "[layer-mask]")
{
    // Zero extent is what a half-drawn rectangle looks like, and what an author
    // gets the instant they enable custom bounds before dragging anything.
    // Trusting it would collapse the raster and drop the layer, so it measures
    // instead.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    f.mask->useCustomBounds(true);
    f.mask->boundsX(10.0f);
    f.mask->boundsY(10.0f);
    f.mask->boundsWidth(0.0f);
    f.mask->boundsHeight(0.0f);

    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    // Still masked, still sized from the content rather than collapsed.
    CHECK(f.mask->testWidthPx() > 0);
    CHECK_FALSE(f.mask->testBox().isEmptyOrNaN());
}

TEST_CASE("a clipping artboard clamps the mask box to itself", "[layer-mask]")
{
    // Content past the edge is cropped at composite time anyway, so rasterizing
    // it would be wasted texels -- and the clamp keeps the raster no larger
    // than the artboard-sized one it replaced, so kMaxDim's coarsening can only
    // bite less than before.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* content = makeShape(artboard, "Content");
    content->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);
    // Centred on the origin, so the badge straddles the artboard's left and top
    // edges: half of it is outside the box.
    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    rive::AABB contentBounds;
    REQUIRE(content->paintedWorldBounds(&contentBounds) !=
            rive::BoundsFidelity::none);
    REQUIRE(contentBounds.left() < 0.0f);

    // The raster does not reach past the artboard box to pick up the overhang.
    // The guard band is added after the clamp, so it may sit a couple of texels
    // out -- which is why this is a bound rather than an equality.
    const rive::AABB box = mask->testBox();
    CHECK(box.left() > contentBounds.left());
    CHECK(box.left() <= 0.0f);
}

TEST_CASE("an unclipped artboard does not clamp the mask box", "[layer-mask]")
{
    // The other half. With clipping off, content reaching past the artboard is
    // genuinely visible -- it draws fine everywhere else on the stage -- so
    // clamping the mask box would crop it inside the mask and nowhere else.
    //
    // Artboard-sized rasters had this bug too, which is why it survived the
    // move to measured bounds: the clamp reproduced it exactly. This is where
    // it is fixed.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);
    artboard.clip(false);

    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    rive::AABB contentBounds;
    REQUIRE(f.content->paintedWorldBounds(&contentBounds) !=
            rive::BoundsFidelity::none);
    // makeShape centres its rectangle on the origin, so the badge straddles the
    // artboard's left and top edges.
    REQUIRE(contentBounds.left() < 0.0f);

    const rive::AABB box = f.mask->testBox();
    CHECK(box.left() <= contentBounds.left());
    CHECK(box.top() <= contentBounds.top());
}

TEST_CASE("disjoint content and source skip the GPU entirely", "[layer-mask]")
{
    // Sizing the rasters from the content and the coverage instead of from the
    // whole artboard makes one case answerable without touching a GPU: if the
    // two boxes do not overlap, every content texel would be multiplied by zero
    // coverage. In alpha mode that is a fully erased layer, so there is nothing
    // to rasterize and nothing to composite.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* content = makeShape(artboard, "Content");
    content->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    // Overlapping: both rasters get built, as ever.
    REQUIRE(frame() == 2);

    // Pull the source clear of the content. The two 10x10 rectangles are
    // centred on their origins, so 80 leaves a wide gap.
    sourceGroup->x(80.0f);
    REQUIRE(frame() == 0);

    // Bring it back and the mask works again -- the shortcut is a per-frame
    // decision, not a latch.
    sourceGroup->x(0.0f);
    REQUIRE(frame() == 2);

    // Inverted alpha reaches the same shortcut from the opposite direction:
    // absent coverage means a factor of one, so the layer is fully VISIBLE.
    // drawMasked declines the bracket and the range draws inline -- also zero
    // canvas opens, but the content still reaches the screen rather than being
    // dropped.
    mask->maskModeValue(static_cast<uint32_t>(rive::MaskMode::invertedAlpha));
    sourceGroup->x(80.0f);
    REQUIRE(frame() == 0);
}

TEST_CASE("a translating mask re-rasters but never re-allocates",
          "[layer-mask]")
{
    // Sliding a mask across the artboard changes the box's POSITION, not its
    // size, and position is deliberately not part of the re-allocation
    // decision. So the pixels are rebuilt every frame (the content really did
    // move) while the two textures are reused -- which is the difference
    // between a re-raster and two fresh driver allocations, an estimated
    // 40-160us native and 0.4-4ms in a browser where each one is an IPC.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(400.0f);
    artboard.height(400.0f);
    // Well inside the artboard, so the clamp never trims the box and change the
    // size for a reason that has nothing to do with translation.
    f.masked->x(100.0f);
    f.masked->y(100.0f);
    f.sourceGroup->x(100.0f);
    f.sourceGroup->y(100.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    REQUIRE(frame() == 2);
    const int allocationsAfterFirst = f.mask->testAllocations;
    REQUIRE(allocationsAfterFirst == 1);
    const uint32_t width = f.mask->testWidthPx();
    const uint32_t height = f.mask->testHeightPx();
    const float firstLeft = f.mask->testBox().left();

    // Sub-texel steps, so the box origin crosses texel boundaries repeatedly.
    for (int step = 1; step <= 40; ++step)
    {
        f.masked->x(100.0f + static_cast<float>(step) * 0.37f);
        f.sourceGroup->x(100.0f + static_cast<float>(step) * 0.37f);
        // Both canvases are rebuilt, because the content genuinely moved.
        CHECK(frame() == 2);
    }

    // ...but nothing was re-allocated, and the size never budged.
    CHECK(f.mask->testAllocations == allocationsAfterFirst);
    CHECK(f.mask->testWidthPx() == width);
    CHECK(f.mask->testHeightPx() == height);
    // And the box really did travel, so the assertions above are not passing
    // because nothing happened.
    CHECK(f.mask->testBox().left() > firstLeft);
}

TEST_CASE("a shrunk mask gives its texture back after kShrinkDraws",
          "[layer-mask]")
{
    // Growth is immediate; shrinking waits. The wait is what stops content
    // animating through a size from re-allocating every frame, and this pins
    // the other end of it -- that a mask which has settled smaller does
    // eventually hand the oversized pair back rather than carrying them
    // forever.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    // A scale the mask inherits, so shrinking it shrinks the measured box.
    auto* scaler = new rive::Node();
    artboard.addObject(scaler);
    scaler->parentId(artboard.idOf(&artboard));
    scaler->scaleX(20.0f);
    scaler->scaleY(20.0f);
    f.masked->parentId(artboard.idOf(scaler));
    f.sourceGroup->parentId(artboard.idOf(scaler));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(1000.0f);
    artboard.height(1000.0f);
    scaler->x(300.0f);
    scaler->y(300.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        session.resetFrame();
    };

    frame();
    const uint32_t big = f.mask->testWidthPx();
    REQUIRE(big > 64);
    const int allocationsWhenBig = f.mask->testAllocations;

    // Shrink it hard, so the bucketed size is unambiguously smaller.
    scaler->scaleX(2.0f);
    scaler->scaleY(2.0f);
    frame();
    // Held, not shrunk: the first frame at the new size must reuse the texture.
    CHECK(f.mask->testWidthPx() == big);
    CHECK(f.mask->testAllocations == allocationsWhenBig);

    // Count the draws it takes. Asserted as an exact number because an
    // off-by-one here is the difference between "waits" and "waits forever".
    int draws = 1; // the frame above already asked to shrink
    while (f.mask->testWidthPx() == big &&
           draws < rive::offscreen::kShrinkDraws * 4)
    {
        frame();
        ++draws;
    }
    CHECK(draws == static_cast<int>(rive::offscreen::kShrinkDraws));
    CHECK(f.mask->testWidthPx() < big);
    CHECK(f.mask->testAllocations == allocationsWhenBig + 1);
}

TEST_CASE("a mask larger than kMaxDim coarsens rather than crops",
          "[layer-mask]")
{
    // The cap is applied by lowering the scale uniformly, never by clamping an
    // axis, so an oversized mask comes out blurry but whole. Cropping instead
    // would put a hard edge through the middle of the artwork.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    // A 10x10 badge under a 300x scale is 3000 units across -- comfortably past
    // the 2048 texel cap even at one texel per unit.
    auto* scaler = new rive::Node();
    artboard.addObject(scaler);
    scaler->parentId(artboard.idOf(&artboard));
    scaler->scaleX(300.0f);
    scaler->scaleY(300.0f);
    f.masked->parentId(artboard.idOf(scaler));
    f.sourceGroup->parentId(artboard.idOf(scaler));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(4000.0f);
    artboard.height(4000.0f);
    scaler->x(2000.0f);
    scaler->y(2000.0f);

    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    rive::AABB contentBounds;
    REQUIRE(f.content->paintedWorldBounds(&contentBounds) !=
            rive::BoundsFidelity::none);
    REQUIRE(contentBounds.width() > 2048.0f);

    // Capped on both axes...
    CHECK(f.mask->testWidthPx() <= rive::offscreen::kMaxDim);
    CHECK(f.mask->testHeightPx() <= rive::offscreen::kMaxDim);
    // ...by coarsening, so fewer than one texel per local unit.
    CHECK(f.mask->testRasterScale() < 1.0f);
    // ...and the box still spans the whole badge. Nothing was cut off.
    const rive::AABB box = f.mask->testBox();
    CHECK(box.left() <= contentBounds.left());
    CHECK(box.top() <= contentBounds.top());
    CHECK(box.right() >= contentBounds.right());
    CHECK(box.bottom() >= contentBounds.bottom());
}

TEST_CASE("an unrelated change leaves a mask's rasters alone", "[layer-mask]")
{
    // The point of putting LayerMask on the dependency graph. Before it, the
    // only signal available at draw time was Artboard::m_didChange --
    // "something in this artboard moved" -- so a mask over a static badge
    // re-rasterized both of its canvases on every frame of an animation
    // happening somewhere else entirely.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    // Outside the masked subtree and outside the source: nothing the mask
    // draws.
    auto* strangerGroup = new rive::Node();
    artboard.addObject(strangerGroup);
    strangerGroup->parentId(artboard.idOf(&artboard));
    auto* stranger = makeShape(artboard, "Stranger");
    stranger->parentId(artboard.idOf(strangerGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    REQUIRE(frame() == 2); // first frame builds both rasters
    REQUIRE(frame() == 0); // nothing moved

    // The artboard changes, but nothing the mask rasterizes does.
    strangerGroup->x(40.0f);
    CHECK(frame() == 0);
    strangerGroup->x(80.0f);
    CHECK(frame() == 0);

    // The masked content moving still has to rebuild them, or this optimisation
    // has simply broken masking.
    f.masked->x(3.0f);
    CHECK(frame() == 2);
    CHECK(frame() == 0);
}

TEST_CASE("recolouring masked content rebuilds the raster", "[layer-mask]")
{
    // A colour edit used to raise no ComponentDirt at all -- SolidColor applied
    // the new colour straight to its RenderPaint and only flagged the artboard.
    // That was fine while masks watched the artboard-wide flag, and became a
    // stale-pixel bug the moment they watched the graph instead: the mask would
    // keep compositing the old colour until something else moved.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    REQUIRE(frame() == 2);
    REQUIRE(frame() == 0);

    REQUIRE_FALSE(f.content->shapePaints().empty());
    rive::Component* mutator = f.content->shapePaints()[0]->paint();
    REQUIRE(mutator != nullptr);
    REQUIRE(mutator->is<rive::SolidColor>());
    auto* color = mutator->as<rive::SolidColor>();

    color->colorValue(0xFF00FF00);
    CHECK(frame() == 2);

    // Twice, because the trap here is dirt that is raised but never cleared:
    // addDirt early-outs on an already-set bit before notifying anyone, so a
    // component outside the dependency order reports its first change and
    // swallows every one after it.
    color->colorValue(0xFF0000FF);
    CHECK(frame() == 2);
    CHECK(frame() == 0);
}

TEST_CASE("moving one stacked mask's source rebuilds both", "[layer-mask]")
{
    // Stacked masks on one element nest, and the outer rasterizes the inner's
    // composited output -- so anything the inner depends on is something the
    // outer depends on too. The dependency graph cannot say that: it delivers
    // dirt to the mask whose own range or source moved, and two masks stacked
    // on one element have different sources. update() used to set only its own
    // m_dirty, so moving the inner mask's source left the outer compositing
    // stale pixels. invalidate() has always walked the siblings; update() now
    // goes through it.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    makeShape(artboard, "Content")->parentId(artboard.idOf(masked));

    // Two sources, one per mask, so dirt on one reaches only one mask.
    auto* srcA = new rive::Node();
    artboard.addObject(srcA);
    srcA->parentId(artboard.idOf(&artboard));
    makeShape(artboard, "SrcA")->parentId(artboard.idOf(srcA));

    auto* srcB = new rive::Node();
    artboard.addObject(srcB);
    srcB->parentId(artboard.idOf(&artboard));
    auto* movingShape = makeShape(artboard, "SrcB");
    movingShape->parentId(artboard.idOf(srcB));

    auto* maskA = new rive::LayerMask();
    artboard.addObject(maskA);
    maskA->parentId(artboard.idOf(masked));
    maskA->sourceId(artboard.idOf(srcA));

    auto* maskB = new rive::LayerMask();
    artboard.addObject(maskB);
    maskB->parentId(artboard.idOf(masked));
    maskB->sourceId(artboard.idOf(srcB));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    // Whatever a full rebuild costs for this arrangement -- asserted against
    // itself rather than against a literal, because how many canvases two
    // stacked masks open is the interleave's business and not what this is
    // about.
    const size_t full = frame();
    REQUIRE(full > 0);
    REQUIRE(frame() == 0);

    // Move a shape under ONE mask's source. A full rebuild is the pass
    // condition: with only the mask that owns the shape invalidated, the other
    // reuses its cache and the count comes back lower.
    movingShape->x(movingShape->x() + 3.0f);
    CHECK(frame() == full);
    CHECK(frame() == 0);
}

TEST_CASE("moving a nested mask's source rebuilds the enclosing mask",
          "[layer-mask]")
{
    // The sibling case generalized. A mask on a DESCENDANT of an outer mask's
    // range is rasterized into that outer mask's content canvas, so anything
    // the inner one depends on is something the outer one depends on. The
    // hierarchy cannot be walked for this from the inner mask's position -- the
    // outer mask is a child of an ancestor, not a sibling -- and
    // buildDependencies deliberately skips LayerMask, so the graph does not
    // carry it either. With the inner mask's source OUTSIDE the outer range,
    // nothing else dirties the outer mask and it composited stale pixels.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    // outerMasked > innerMasked > Content, a mask on each level.
    auto* outerMasked = new rive::Node();
    artboard.addObject(outerMasked);
    outerMasked->parentId(artboard.idOf(&artboard));
    auto* innerMasked = new rive::Node();
    artboard.addObject(innerMasked);
    innerMasked->parentId(artboard.idOf(outerMasked));
    makeShape(artboard, "Content")->parentId(artboard.idOf(innerMasked));

    // Both sources sit outside outerMasked, so dirt on the inner one cannot
    // reach the outer mask through any range it owns.
    auto* outerSource = new rive::Node();
    artboard.addObject(outerSource);
    outerSource->parentId(artboard.idOf(&artboard));
    makeShape(artboard, "OuterSrc")->parentId(artboard.idOf(outerSource));

    auto* innerSource = new rive::Node();
    artboard.addObject(innerSource);
    innerSource->parentId(artboard.idOf(&artboard));
    auto* movingShape = makeShape(artboard, "InnerSrc");
    movingShape->parentId(artboard.idOf(innerSource));

    auto* outerMask = new rive::LayerMask();
    artboard.addObject(outerMask);
    outerMask->parentId(artboard.idOf(outerMasked));
    outerMask->sourceId(artboard.idOf(outerSource));

    auto* innerMask = new rive::LayerMask();
    artboard.addObject(innerMask);
    innerMask->parentId(artboard.idOf(innerMasked));
    innerMask->sourceId(artboard.idOf(innerSource));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);
    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    const size_t full = frame();
    REQUIRE(full > 0);
    REQUIRE(frame() == 0);

    // Only the inner mask's source moved. Both masks must rebuild.
    movingShape->x(movingShape->x() + 3.0f);
    CHECK(frame() == full);
    CHECK(frame() == 0);
}

TEST_CASE("a source above the masked node is rejected", "[layer-mask]")
{
    // The check used to walk up from the source looking for the masked node,
    // which catches a source at or below it and misses a source ABOVE it. That
    // direction is worse: the source subtree then contains every masked
    // drawable, so the content is its own coverage. With sourceDraws false the
    // enclosing source bracket suppresses the masked range and the layer
    // vanishes; with it true the coverage pass walks back into this mask's own
    // bracket. The below direction has its own case, "a self-referential mask
    // is dropped rather than recursing"; this is the mirror of it.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    // outer > inner > shape, with the mask on `inner` sourcing `outer`.
    auto* outer = new rive::Node();
    artboard.addObject(outer);
    outer->parentId(artboard.idOf(&artboard));
    auto* inner = new rive::Node();
    artboard.addObject(inner);
    inner->parentId(artboard.idOf(outer));
    makeShape(artboard, "Content")->parentId(artboard.idOf(inner));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(inner));
    mask->sourceId(artboard.idOf(outer));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    CHECK(mask->isSelfReferential());

    // Inert means no brackets and no canvases -- the content draws as if the
    // mask were not there.
    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());
    CHECK(countCanvasOpens(session.commandBuffer()) == 0);
    session.resetFrame();
}

TEST_CASE("two masks pointing at each other terminate", "[layer-mask]")
{
    // A mask whose source sits under its own parent is rejected at hydration,
    // but two masks naming each other's subtrees pass that test -- each source
    // is under the OTHER mask's parent -- and the coverage pass draws a range
    // that drawDrawableRange dispatches from, so the arrangement looks like it
    // ought to recurse.
    //
    // It does not, and this pins the reason rather than the guard.
    // spliceLayerMaskBracket grows every range until it balances with the
    // brackets already in the list, so brackets nest instead of interleaving,
    // and the inner mask's bracket ends up enclosing the outer one's source
    // range rather than sitting inside it. Were that invariant relaxed this
    // would recurse until the stack died, which is what the re-entry guard in
    // drawMasked exists for -- but the guard is not what makes this pass today.

    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    // Two sibling groups, each masked by the other.
    auto* groupA = new rive::Node();
    artboard.addObject(groupA);
    groupA->parentId(artboard.idOf(&artboard));
    makeShape(artboard, "A")->parentId(artboard.idOf(groupA));

    auto* groupB = new rive::Node();
    artboard.addObject(groupB);
    groupB->parentId(artboard.idOf(&artboard));
    makeShape(artboard, "B")->parentId(artboard.idOf(groupB));

    auto* maskA = new rive::LayerMask();
    artboard.addObject(maskA);
    maskA->parentId(artboard.idOf(groupA));
    maskA->sourceId(artboard.idOf(groupB));

    auto* maskB = new rive::LayerMask();
    artboard.addObject(maskB);
    maskB->parentId(artboard.idOf(groupB));
    maskB->sourceId(artboard.idOf(groupA));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    // Neither is self-referential: each source is under the OTHER mask's
    // parent, which is exactly why the hydration-time test cannot see this.
    CHECK_FALSE(maskA->isSelfReferential());
    CHECK_FALSE(maskB->isSelfReferential());

    // The point of the test is that this returns at all.
    artboard.advance(0.0f);
    artboard.drawInternal(session.screenRenderer());

    // One of the pair still rasterizes; which one depends on draw order and is
    // not the point. The bound is.
    const size_t opens = countCanvasOpens(session.commandBuffer());
    CHECK(opens <= 4);
    session.resetFrame();
}

TEST_CASE("recolouring a mask source rebuilds the raster", "[layer-mask]")
{
    // The other half of the colour story, and the half that was missing. The
    // notification in SolidColor::colorValueChanged walked up to the nearest
    // Drawable and read layerMasks() -- the masks drawn OVER that drawable. A
    // mask's source generally sits nowhere near the range it masks, and it gets
    // no entry in that list, so recolouring a source left the coverage canvas
    // holding the old colour. Invisible for alpha (only the shape matters), and
    // plainly wrong for luminance, where the colour IS the coverage.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    MaskFixture f = buildMaskFixture(artboard);
    f.mask->maskModeValue(static_cast<uint32_t>(rive::MaskMode::luminance));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    REQUIRE(frame() == 2);
    REQUIRE(frame() == 0);

    REQUIRE(f.source != nullptr);
    REQUIRE_FALSE(f.source->shapePaints().empty());
    rive::Component* mutator = f.source->shapePaints()[0]->paint();
    REQUIRE(mutator != nullptr);
    REQUIRE(mutator->is<rive::SolidColor>());
    auto* color = mutator->as<rive::SolidColor>();

    color->colorValue(0xFF00FF00);
    CHECK(frame() == 2);

    // Twice, for the same reason the masked-content case checks twice: a path
    // that reports only its first change would pass a single assertion.
    color->colorValue(0xFF0000FF);
    CHECK(frame() == 2);
    CHECK(frame() == 0);
}

TEST_CASE("a mask re-rasterizes when its content or source moves",
          "[layer-mask]")
{
    // The regression this exists for: Artboard::drawContent clears m_didChange
    // *before* walking the draw list, so a mask deciding "did anything move?"
    // from inside that walk saw false on every frame after the first and
    // composited a stale raster forever. Animating anything under a mask simply
    // did nothing.
    //
    // Counted rather than rendered: this is about the re-raster *decision*, and
    // the pixel harness cannot drive two frames from one session anyway.
    auto renderContext = RenderContextNULL::MakeContext();
    rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
    session.bindRenderContext(renderContext.get());

    rive::Artboard artboard(&session);
    artboard.addObject(&artboard);

    auto* masked = new rive::Node();
    artboard.addObject(masked);
    masked->parentId(artboard.idOf(&artboard));
    auto* content = makeShape(artboard, "Content");
    content->parentId(artboard.idOf(masked));

    auto* sourceGroup = new rive::Node();
    artboard.addObject(sourceGroup);
    sourceGroup->parentId(artboard.idOf(&artboard));
    auto* src = makeShape(artboard, "Src");
    src->parentId(artboard.idOf(sourceGroup));

    auto* mask = new rive::LayerMask();
    artboard.addObject(mask);
    mask->parentId(artboard.idOf(masked));
    mask->sourceId(artboard.idOf(sourceGroup));

    REQUIRE(artboard.initialize() == rive::StatusCode::Ok);
    artboard.width(200.0f);
    artboard.height(200.0f);

    auto frame = [&]() {
        artboard.advance(0.0f);
        artboard.drawInternal(session.screenRenderer());
        size_t opens = countCanvasOpens(session.commandBuffer());
        session.resetFrame();
        return opens;
    };

    // First frame builds both rasters: content and coverage.
    REQUIRE(frame() == 2);

    // Nothing moved, so both are reused.
    REQUIRE(frame() == 0);

    // Moving the mask SOURCE must rebuild them -- this is the case the user
    // hit.
    //
    // The offsets are deliberately small. makeShape builds a 10x10 rectangle
    // centred on its origin, so a larger nudge would pull the source clear of
    // the content -- and disjoint content and coverage in alpha mode is now its
    // own shortcut (see "disjoint content and source skip the GPU entirely"),
    // which would report zero canvas opens for a reason that has nothing to do
    // with the re-raster decision this test exists to pin.
    sourceGroup->x(4.0f);
    REQUIRE(frame() == 2);
    REQUIRE(frame() == 0);

    // And moving the masked content must too.
    masked->x(3.0f);
    REQUIRE(frame() == 2);
    REQUIRE(frame() == 0);
}
#endif
