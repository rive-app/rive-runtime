/*
 * Copyright 2026 Rive
 *
 * A text style's background drawn from rects a text modifier changed: faded
 * ones draw from their own opacity bucket, and moved ones rebuild whatever
 * a dash or trim made of the old geometry. A PaintImage on it fits its
 * Text's layout box, as on the glyphs and in the editor.
 */

#include <rive/artboard.hpp>
#include <rive/assets/image_asset.hpp>
#include <rive/shapes/paint/dash.hpp>
#include <rive/shapes/paint/dash_path.hpp>
#include <rive/shapes/paint/fill.hpp>
#include <rive/shapes/paint/paint_image.hpp>
#include <rive/shapes/paint/solid_color.hpp>
#include <rive/shapes/paint/stroke.hpp>
#include <rive/text/text.hpp>
#include <rive/text/text_style_background.hpp>
#include <rive/text/text_style_paint.hpp>
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

class TestImage : public RenderImage
{};

// Paints that keep their image, and images decoded from any bytes.
class ImagePaintFactory : public NoOpFactory
{
public:
  rcp<RenderPaint> makeRenderPaint() override { return make_rcp<ImagePaint>(); }
  rcp<RenderImage> decodeImage(Span<const uint8_t>) override
  {
    return make_rcp<TestImage>();
  }
};

// The image transforms of the drawn paths that carried an image.
class ImageRecorder : public NoOpRenderer
{
public:
  std::vector<Mat2D> imageTransforms;

  void drawPath(RenderPath*, RenderPaint* paint) override
  {
    // Every paint in this scene comes from ImagePaintFactory.
    auto* imagePaint = static_cast<ImagePaint*>(paint);
    if (imagePaint->image != nullptr)
    {
      imageTransforms.push_back(imagePaint->imageTransform);
    }
  }
};

// A global style's paint (straight under the artboard, so no Text or font is
// needed) with a background: an image modulated fill and a dashed stroke.
struct BackgroundScene
{
  ImagePaintFactory factory;
  rcp<ImageAsset> asset = make_rcp<ImageAsset>();
  rcp<RenderImage> image = make_rcp<TestImage>();
  Artboard artboard{&factory};
  TextStyleBackground* background = nullptr;
  PaintImage* paintImage = nullptr;
  Stroke* stroke = nullptr;
  DashPath* dashPath = nullptr;

  BackgroundScene()
  {
    artboard.addObject(&artboard);
    auto* style = new TextStylePaint();
    artboard.addObject(style);
    style->parentId(artboard.idOf(&artboard));
    background = new TextStyleBackground();
    artboard.addObject(background);
    background->parentId(artboard.idOf(style));

    auto* fill = new Fill();
    artboard.addObject(fill);
    fill->parentId(artboard.idOf(background));
    auto* fillColor = new SolidColor();
    fillColor->colorValue(0xFFFFFFFF);
    artboard.addObject(fillColor);
    fillColor->parentId(artboard.idOf(fill));
    paintImage = new PaintImage();
    artboard.addObject(paintImage);
    paintImage->parentId(artboard.idOf(fill));

    stroke = new Stroke();
    stroke->thickness(1.0f);
    artboard.addObject(stroke);
    stroke->parentId(artboard.idOf(background));
    auto* strokeColor = new SolidColor();
    strokeColor->colorValue(0xFF000000);
    artboard.addObject(strokeColor);
    strokeColor->parentId(artboard.idOf(stroke));
    dashPath = new DashPath();
    artboard.addObject(dashPath);
    dashPath->parentId(artboard.idOf(stroke));
    auto* dash = new Dash();
    dash->length(2.0f);
    artboard.addObject(dash);
    dash->parentId(artboard.idOf(dashPath));

    REQUIRE(artboard.initialize() == StatusCode::Ok);
    artboard.advance(0.0f);
    paintImage->setAsset(asset);
    asset->renderImage(image);
  }

  // Lays out a glyph at [x], and a second one 20 to its right that a
  // modifier moved down by [dy] and faded to [opacity].
  void layout(float x, float dy, float opacity)
  {
    background->resetPath();
    background->addRect(AABB(x, 0.0f, x + 10.0f, 10.0f));
    const Mat2D moved = Mat2D::fromTranslate(0.0f, dy);
    background->addModifiedRect(
        AABB(x + 20.0f, 0.0f, x + 30.0f, 10.0f),
        dy == 0.0f ? nullptr : &moved,
        opacity);
    background->updatePath();
    artboard.advance(0.0f);
  }

  AABB dashBounds()
  {
    return dashPath->effectPath(stroke)->rawPath()->bounds();
  }
};
} // namespace

TEST_CASE("a background fits its image to the box it's handed", "[text]")
{
  BackgroundScene scene;
  // The drawing Text's layout box: bigger than the rects, so a fit to them
  // instead would show.
  const AABB box(-5.0f, -5.0f, 60.0f, 30.0f);
  const Mat2D fit = scene.paintImage->textureTransform(box, 0, 0);
  auto draw = [&]() {
    ImageRecorder drawn;
    scene.background->draw(&drawn, Mat2D(), &box);
    return drawn.imageTransforms;
  };

  // Without modifiers.
  scene.layout(0.0f, 0.0f, 1.0f);
  auto drawn = draw();
  REQUIRE(drawn.size() == 1);
  CHECK(drawn[0] == fit);

  // A faded glyph: the opaque bucket and the faded one fit the same box.
  scene.layout(0.0f, 0.0f, 0.5f);
  drawn = draw();
  REQUIRE(drawn.size() == 2);
  CHECK(drawn[0] == fit);
  CHECK(drawn[1] == fit);

  // Faded out entirely, in place or moved: the other glyph's image doesn't
  // move.
  scene.layout(0.0f, 0.0f, 0.0f);
  drawn = draw();
  REQUIRE(drawn.size() == 1);
  CHECK(drawn[0] == fit);
  scene.layout(0.0f, 15.0f, 0.0f);
  drawn = draw();
  REQUIRE(drawn.size() == 1);
  CHECK(drawn[0] == fit);
}

// text_adornment_modifiers.riv's "Imaged": origin 0.5, an image-filled
// background, and an inverted opacity modifier fading the first half (see
// text_adornment_modifier_test.dart).
TEST_CASE(
    "a Text fits its background's image to its layout box in glyph space",
    "[text]")
{
  ImagePaintFactory factory;
  auto file = ReadRiveFile("assets/text_adornment_modifiers.riv", &factory);
  auto artboard = file->artboard("Main")->instance();
  Text* text = artboard->find<Text>("Imaged");
  REQUIRE(text != nullptr);
  artboard->advance(0.0f);
  const AABB box = text->glyphSpaceBounds();
  // The origin moves the local box, not the glyphs, so a fit to it would
  // show.
  REQUIRE(!(box == text->localBounds()));

  REQUIRE(text->runs().size() == 1);
  TextStylePaint* style = text->runs().front()->style();
  REQUIRE(style->background() != nullptr);
  PaintImage* paintImage = nullptr;
  for (ShapePaint* shapePaint : style->background()->shapePaints())
  {
    if (shapePaint->paintImage() != nullptr)
    {
      paintImage = shapePaint->paintImage();
    }
  }
  REQUIRE(paintImage != nullptr);

  ImageRecorder drawn;
  artboard->draw(&drawn);
  // The opaque glyphs' bucket and the faded ones'.
  REQUIRE(drawn.imageTransforms.size() > 1);
  const Mat2D fit = paintImage->textureTransform(box, 0, 0);
  for (const Mat2D& transform : drawn.imageTransforms)
  {
    CHECK(transform == fit);
  }
}

TEST_CASE("a moved background glyph redoes the background's dash", "[text]")
{
  BackgroundScene scene;
  scene.layout(0.0f, 0.0f, 1.0f);
  NoOpRenderer renderer;
  scene.background->draw(&renderer, Mat2D());
  CHECK(scene.dashBounds().maxY == Approx(10.0f).margin(1.0f));

  // A modifier moves the second glyph down; the dash follows it.
  scene.layout(0.0f, 15.0f, 1.0f);
  scene.background->draw(&renderer, Mat2D());
  CHECK(scene.dashBounds().maxY == Approx(25.0f).margin(1.0f));

  // So does a relayout without modifiers.
  scene.layout(50.0f, 0.0f, 1.0f);
  scene.background->draw(&renderer, Mat2D());
  CHECK(scene.dashBounds().minX == Approx(50.0f).margin(1.0f));
}
