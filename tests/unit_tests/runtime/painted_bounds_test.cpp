/*
 * Copyright 2026 Rive
 *
 * Drawable::paintedWorldBounds and the fidelity each type claims for it.
 *
 * This classification is load-bearing rather than cosmetic: anything sizing an
 * offscreen surface from content bounds trusts it, and a type that over-claims
 * gets its artwork cropped at a raster edge. So the `none` and `approximate`
 * cases below are as much the point as the `exact` ones.
 */

#include <rive/artboard.hpp>
#include <rive/drawable.hpp>
#include <rive/node.hpp>
#include <rive/shapes/rectangle.hpp>
#include <rive/shapes/shape.hpp>
#include <rive/shapes/paint/feather.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/shapes/paint/stroke_cap.hpp>
#include <rive/shapes/paint/stroke_join.hpp>
#include <rive/shapes/paint/trim_path.hpp>
#include <rive/text/text.hpp>
#include <utils/no_op_factory.hpp>

#include <catch.hpp>

using namespace rive;

namespace
{
// A Drawable with no override, standing in for every type that cannot say where
// it paints -- a scripted drawable, a virtualized artboard list. Constructed
// directly rather than registered, because only the virtual is under test.
class UnboundedDrawable : public Drawable
{
public:
    void draw(Renderer*) override {}
    Core* hitTest(HitInfo*, const Mat2D&) override { return nullptr; }
};

// A 10x10 rectangle centred on its origin, with a fill. The caller parents it.
// `fillOut` hands back the Fill, because shapePaints() is not populated until
// the artboard is initialized and a test may need to hang a Feather off it
// first.
Shape* makeShape(Artboard& artboard, Fill** fillOut = nullptr)
{
    auto* shape = new Shape();
    artboard.addObject(shape);

    auto* rect = new Rectangle();
    rect->width(10.0f);
    rect->height(10.0f);
    artboard.addObject(rect);
    rect->parentId(artboard.idOf(shape));

    auto* fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(shape));

    auto* color = new SolidColor();
    color->colorValue(0xFFFF0000);
    artboard.addObject(color);
    color->parentId(artboard.idOf(fill));

    if (fillOut != nullptr)
    {
        *fillOut = fill;
    }
    return shape;
}

Stroke* addStroke(Artboard& artboard, Shape* shape, float thickness)
{
    auto* stroke = new Stroke();
    stroke->thickness(thickness);
    stroke->join(static_cast<uint32_t>(StrokeJoin::round));
    stroke->cap(static_cast<uint32_t>(StrokeCap::butt));
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(shape));

    auto* color = new SolidColor();
    color->colorValue(0xFF00FF00);
    artboard.addObject(color);
    color->parentId(artboard.idOf(stroke));
    return stroke;
}

NoOpFactory gFactory;
} // namespace

TEST_CASE("a drawable that cannot bound itself says so", "[painted-bounds]")
{
    UnboundedDrawable d;
    AABB box(1, 2, 3, 4);
    const AABB untouched = box;
    CHECK(d.paintedWorldBounds(&box) == BoundsFidelity::none);
    // The contract: *out is left alone, so a caller cannot accidentally consume
    // a stale or half-written box.
    CHECK(box == untouched);
}

TEST_CASE("a filled shape bounds its geometry exactly", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* shape = makeShape(artboard);
    shape->parentId(artboard.idOf(&artboard));
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    REQUIRE(shape->paintedWorldBounds(&box) == BoundsFidelity::exact);
    // Centred on its origin, so a 10x10 rect spans -5..5. A fill adds nothing.
    CHECK(box.left() == Approx(-5.0f));
    CHECK(box.top() == Approx(-5.0f));
    CHECK(box.right() == Approx(5.0f));
    CHECK(box.bottom() == Approx(5.0f));
}

TEST_CASE("a stroke widens the box by half its thickness", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* shape = makeShape(artboard);
    shape->parentId(artboard.idOf(&artboard));
    addStroke(artboard, shape, 8.0f);
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    REQUIRE(shape->paintedWorldBounds(&box) == BoundsFidelity::exact);
    // Round join, butt cap: exactly the radius, on every side.
    CHECK(box.left() == Approx(-9.0f));
    CHECK(box.right() == Approx(9.0f));
}

TEST_CASE("a miter join widens the box by the miter limit", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* shape = makeShape(artboard);
    shape->parentId(artboard.idOf(&artboard));
    auto* stroke = addStroke(artboard, shape, 8.0f);
    stroke->join(static_cast<uint32_t>(StrokeJoin::miter));
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    REQUIRE(shape->paintedWorldBounds(&box) == BoundsFidelity::exact);
    // 4 * radius, because a mitre can overshoot the corner that far.
    CHECK(box.left() == Approx(-5.0f - 16.0f));
    CHECK(box.right() == Approx(5.0f + 16.0f));
}

TEST_CASE("a feather widens the box past the shape", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    Fill* fill = nullptr;
    auto* shape = makeShape(artboard, &fill);
    shape->parentId(artboard.idOf(&artboard));

    auto* feather = new Feather();
    feather->strength(10.0f);
    artboard.addObject(feather);
    feather->parentId(artboard.idOf(fill));

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    REQUIRE(shape->paintedWorldBounds(&box) == BoundsFidelity::exact);
    // 1.5x the authored strength: a blur is quoted as two standard deviations.
    // This is the case artboard-sized rasters were hiding -- a tight box that
    // ignored it would crop the blur dead along a straight line.
    CHECK(box.left() == Approx(-20.0f));
    CHECK(box.right() == Approx(20.0f));
}

TEST_CASE("a scaled shape scales its local outset too", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* group = new Node();
    artboard.addObject(group);
    group->parentId(artboard.idOf(&artboard));
    group->scaleX(3.0f);
    group->scaleY(3.0f);

    auto* shape = makeShape(artboard);
    shape->parentId(artboard.idOf(group));
    addStroke(artboard, shape, 8.0f);
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    REQUIRE(shape->paintedWorldBounds(&box) == BoundsFidelity::exact);
    // Geometry and outset both scale: (5 + 4) * 3. Getting this wrong --
    // padding a world box by a local outset -- under-pads every scaled-up
    // shape.
    CHECK(box.left() == Approx(-27.0f));
    CHECK(box.right() == Approx(27.0f));
}

TEST_CASE("a stroke effect drops the shape to approximate", "[painted-bounds]")
{
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* shape = makeShape(artboard);
    shape->parentId(artboard.idOf(&artboard));
    auto* stroke = addStroke(artboard, shape, 4.0f);

    auto* trim = new TrimPath();
    // Required: TrimPath::onAddedDirty rejects any mode outside this enum.
    trim->modeValue(static_cast<uint32_t>(TrimPathMode::sequential));
    trim->start(0.0f);
    trim->end(0.5f);
    artboard.addObject(trim);
    trim->parentId(artboard.idOf(stroke));

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    // A trim only ever subsets its source path, so this is pessimistic -- but
    // nothing distinguishes a trim from a scripted effect that displaces
    // geometry arbitrarily, and guessing wrong in the other direction crops.
    CHECK(shape->paintedWorldBounds(&box) == BoundsFidelity::approximate);
    // Still a usable box, just one a caller has to pad.
    CHECK_FALSE(box.isEmptyOrNaN());
}

TEST_CASE("text is never better than approximate", "[painted-bounds]")
{
    // localBounds() is the layout box. Glyph ink escapes it through negative
    // side bearings, italic overhang, overflow:visible and modifier transforms,
    // so an exact claim here would crop glyphs at a raster edge.
    Artboard artboard(&gFactory);
    artboard.addObject(&artboard);
    auto* text = new Text();
    artboard.addObject(text);
    text->parentId(artboard.idOf(&artboard));
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);

    AABB box;
    const BoundsFidelity fidelity = text->paintedWorldBounds(&box);
    CHECK(fidelity != BoundsFidelity::exact);
}
