/*
 * Copyright 2026 Rive
 *
 * A text style's paint draws its glyphs bucketed by opacity: the opaque ones
 * through its own render paint, the rest through pooled ones. A PaintImage
 * child modulates every bucket, fit to the drawing text's layout box (not to
 * its glyphs), so the image lines up across glyphs whatever their opacity.
 * The pooled paints are shared by every paint of the style, so one without
 * an image never draws with the image another paint left there.
 */

#include <rive/artboard.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/math/raw_path.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/paint_image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_style_geometry.hpp>
#include <rive/text/text_style_paint.hpp>
#include <rive/text/text_value_run.hpp>
#include <utils/no_op_factory.hpp>
#include <utils/no_op_renderer.hpp>
#include "rive_file_reader.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive;

namespace
{
// Keeps the image it's modulated with now.
class ImagePaint : public RenderPaint
{
public:
  const RenderImage* image = nullptr;
  Mat2D imageTransform;

  void style(RenderPaintStyle) override {}
  void color(ColorInt) override {}
  void thickness(float) override {}
  void join(StrokeJoin) override {}
  void cap(StrokeCap) override {}
  void blendMode(BlendMode) override {}
  void shader(rcp<RenderShader>) override {}
  void shaderTransform(const Mat2D&) override {}
  void invalidateStroke() override {}
  void modulatedImage(
      const RenderImage* value,
      ImageSampler,
      const Mat2D& transform) override
  {
    image = value;
    imageTransform = transform;
  }
};

class ImagePaintFactory : public NoOpFactory
{
public:
  rcp<RenderPaint> makeRenderPaint() override { return make_rcp<ImagePaint>(); }
};

// A drawn path's paint, and the image that paint carried as it drew.
struct Draw
{
  RenderPaint* paint;
  const RenderImage* image;
  Mat2D imageTransform;
};

class DrawRecorder : public NoOpRenderer
{
public:
  std::vector<Draw> draws;

  void drawPath(RenderPath*, RenderPaint* paint) override
  {
    // Every paint in these scenes comes from ImagePaintFactory.
    auto* imagePaint = static_cast<ImagePaint*>(paint);
    draws.push_back({paint, imagePaint->image, imagePaint->imageTransform});
  }
};

class TestImage : public RenderImage
{};

// The layout box the scenes draw with: wider than the glyphs, so a fit to
// the glyphs instead would show.
const AABB kLayoutBox(-5.0f, -5.0f, 45.0f, 35.0f);

RawPath square(float x, float y, float size)
{
  RawPath path;
  path.addRect(AABB(x, y, x + size, y + size));
  return path;
}

// A global style's paint (straight under the artboard, so no Text or font is
// needed) with an image modulated fill followed by a plain stroke.
struct TextScene
{
  ImagePaintFactory factory;
  rcp<ImageAsset> asset = make_rcp<ImageAsset>();
  rcp<RenderImage> image = make_rcp<TestImage>();
  Artboard artboard{&factory};
  TextStylePaint* style = nullptr;
  Fill* fill = nullptr;
  PaintImage* paintImage = nullptr;
  Stroke* stroke = nullptr;

  TextScene()
  {
    artboard.addObject(&artboard);
    style = new TextStylePaint();
    artboard.addObject(style);
    style->parentId(artboard.idOf(&artboard));

    fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(style));
    auto* fillColor = new SolidColor();
    fillColor->colorValue(0xFFFFFFFF);
    artboard.addObject(fillColor);
    fillColor->parentId(artboard.idOf(fill));
    paintImage = new PaintImage();
    artboard.addObject(paintImage);
    paintImage->parentId(artboard.idOf(fill));

    stroke = new Stroke();
    stroke->thickness(2.0f);
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(style));
    auto* strokeColor = new SolidColor();
    strokeColor->colorValue(0xFF000000);
    artboard.addObject(strokeColor);
    strokeColor->parentId(artboard.idOf(stroke));

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);
    paintImage->setAsset(asset);
    asset->renderImage(image);

    // Two glyphs side by side, the second faded by a modifier.
    style->rewindPath();
    style->addPath(square(0.0f, 0.0f, 10.0f), 1.0f, nullptr);
    style->addPath(square(20.0f, 0.0f, 10.0f), 0.5f, nullptr);
  }

  DrawRecorder draw()
  {
    DrawRecorder recorder;
    style->draw(&recorder, Mat2D(), kLayoutBox);
    return recorder;
  }
};
} // namespace

TEST_CASE("every glyph bucket is modulated by the image", "[paint-image]")
{
  TextScene scene;
  DrawRecorder drawn = scene.draw();
  // The fill's opaque and faded buckets, then the stroke's.
  REQUIRE(drawn.draws.size() == 4);
  const Mat2D whole = scene.paintImage->textureTransform(kLayoutBox, 0, 0);

  CHECK(drawn.draws[0].paint == scene.fill->renderPaint());
  CHECK(drawn.draws[0].image == scene.image.get());
  CHECK(drawn.draws[0].imageTransform == whole);
  CHECK(drawn.draws[1].paint != scene.fill->renderPaint());
  CHECK(drawn.draws[1].image == scene.image.get());
  CHECK(drawn.draws[1].imageTransform == whole);

  // The stroke's faded bucket reuses the pooled paint the fill drew with,
  // without its image.
  CHECK(drawn.draws[2].paint == scene.stroke->renderPaint());
  CHECK(drawn.draws[2].image == nullptr);
  CHECK(drawn.draws[3].paint == drawn.draws[1].paint);
  CHECK(drawn.draws[3].image == nullptr);

  SECTION("and the next frame draws the same")
  {
    DrawRecorder again = scene.draw();
    REQUIRE(again.draws.size() == 4);
    CHECK(again.draws[1].image == scene.image.get());
    CHECK(again.draws[3].image == nullptr);
  }
}

TEST_CASE(
    "a shared style's paint fits the image to the drawing text",
    "[paint-image]")
{
  TextScene scene;
  // Another Text's glyphs, drawn at that Text's opacity: every bucket goes
  // through the geometry's pooled paints.
  TextStyleGeometry geometry;
  geometry.addPath(square(0.0f, 0.0f, 10.0f), 1.0f);
  geometry.addPath(square(0.0f, 20.0f, 10.0f), 0.5f);
  DrawRecorder drawn;
  const AABB otherBox(0.0f, 0.0f, 10.0f, 30.0f);
  scene.style->drawGeometry(&drawn, Mat2D(), geometry, 0.5f, otherBox);

  REQUIRE(drawn.draws.size() == 4);
  const Mat2D whole = scene.paintImage->textureTransform(otherBox, 0, 0);
  CHECK(drawn.draws[0].image == scene.image.get());
  CHECK(drawn.draws[0].imageTransform == whole);
  CHECK(drawn.draws[1].image == scene.image.get());
  CHECK(drawn.draws[1].imageTransform == whole);
  CHECK(drawn.draws[2].image == nullptr);
  CHECK(drawn.draws[3].image == nullptr);
}

TEST_CASE("a missing image leaves every glyph bucket undrawn", "[paint-image]")
{
  TextScene scene;
  scene.asset->renderImage(nullptr);
  DrawRecorder drawn = scene.draw();
  // Only the stroke's buckets.
  REQUIRE(drawn.draws.size() == 2);
  CHECK(drawn.draws[0].paint == scene.stroke->renderPaint());
  for (const Draw& draw : drawn.draws)
  {
    CHECK(draw.image == nullptr);
  }
}

TEST_CASE(
    "a text's layout box is fit in the space its glyphs draw in",
    "[paint-image]")
{
  rcp<File> file = ReadRiveFile("assets/new_text.riv");
  auto artboard = file->artboard()->instance();
  auto texts = artboard->find<Text>();
  REQUIRE(!texts.empty());
  Text* text = texts.front();
  // One word, so a fit can only shrink it, never wrap it.
  auto runs = text->runs();
  REQUIRE(!runs.empty());
  runs.front()->text("Introduction");
  for (size_t i = 1; i < runs.size(); i++)
  {
    runs[i]->text("");
  }
  text->originX(0.0f);
  text->originY(0.0f);
  text->sizingValue((uint32_t)TextSizing::autoWidth);
  text->overflowValue((uint32_t)TextOverflow::visible);
  artboard->advance(0.0f);
  const AABB box = text->localBounds();
  REQUIRE(box.width() > 0.0f);
  CHECK(text->glyphSpaceBounds() == box);

  SECTION("the origin moves the box, not the glyphs in it")
  {
    text->originX(0.5f);
    text->originY(1.0f);
    artboard->advance(0.0f);
    CHECK(
        text->localBounds().left() == Approx(box.left() - box.width() * 0.5f));
    CHECK(text->localBounds().top() == Approx(box.top() - box.height()));
    const AABB glyphBox = text->glyphSpaceBounds();
    CHECK(glyphBox.left() == Approx(box.left()));
    CHECK(glyphBox.top() == Approx(box.top()));
    CHECK(glyphBox.width() == Approx(box.width()));
    CHECK(glyphBox.height() == Approx(box.height()));
  }

  SECTION("a fit scale shrinks the glyphs, not the box")
  {
    text->sizingValue((uint32_t)TextSizing::fixed);
    text->overflowValue((uint32_t)TextOverflow::fit);
    text->width(box.width() * 0.5f);
    text->height(box.height());
    artboard->advance(0.0f);
    const AABB fitted = text->localBounds();
    CHECK(fitted.width() == Approx(box.width() * 0.5f));
    // The glyphs drew at half size, so the box spans twice as much of
    // their space.
    CHECK(text->glyphSpaceBounds().width() > fitted.width() * 1.5f);
  }
}
