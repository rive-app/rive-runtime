#include "rive/artboard.hpp"
#include "rive/assets/image_asset.hpp"
#include "rive/core/vector_binary_writer.hpp"
#include "rive/data_bind/data_bind.hpp"
#include "rive/layout/layout_component_style.hpp"
#include "rive/layout/layout_participant.hpp"
#include "rive/layout_component.hpp"
#include "rive/shapes/image.hpp"
#include "rive/viewmodel/viewmodel_instance_asset_image.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"
#include <catch.hpp>
#include <limits>

using namespace rive;

namespace
{
class SizedImage : public RenderImage
{
public:
  SizedImage(uint32_t width, uint32_t height)
  {
    m_Width = width;
    m_Height = height;
  }
};

class ImageIdentityRenderer : public NoOpRenderer
{
public:
  const RenderImage* lastImage = nullptr;

  void drawImage(const RenderImage* image, ImageSampler, BlendMode, float)
      override
  {
    lastImage = image;
  }
};

rcp<ImageAsset> assetOfSize(uint32_t width, uint32_t height)
{
  auto asset = make_rcp<ImageAsset>();
  asset->renderImage(make_rcp<SizedImage>(width, height));
  return asset;
}

// These two fields must exist before initialize resolves styles and inserts
// clipping proxies. Follow the file-import path, which assigns authored fields
// without invoking live-edit dirt handlers on a not-yet-linked object graph.
void authoredStyleId(LayoutComponent* layout, uint32_t id)
{
  std::vector<uint8_t> bytes;
  VectorBinaryWriter writer(&bytes);
  writer.writeVarUint((uint64_t)id);
  BinaryReader reader(bytes);
  REQUIRE(layout->deserialize(LayoutComponent::styleIdPropertyKey, reader));
}

void authoredClip(LayoutComponent* layout)
{
  uint8_t value = 1;
  BinaryReader reader(Span<const uint8_t>(&value, 1));
  REQUIRE(layout->deserialize(LayoutComponent::clipPropertyKey, reader));
}

// A fixed artboard, a Fill/Fill parent, and a bounded Hug/Hug image. Hug
// remains intrinsic for every fit mode; Cover fits the box layout actually
// resolves. The fake decoded image carries dimensions only. The artboard owns
// all components added below.
struct ImageLayoutScene
{
  NoOpFactory factory;
  Artboard artboard{&factory};
  LayoutComponent* parent = new LayoutComponent();
  Image* image = new Image();
  LayoutParticipant* participant = new LayoutParticipant();

  ImageLayoutScene(
      uint32_t sourceWidth = 96,
      uint32_t sourceHeight = 96,
      ImageFit fit = ImageFit::cover)
  {
    artboard.addObject(&artboard);
    auto* rootStyle = new LayoutComponentStyle();
    artboard.addObject(rootStyle);
    rootStyle->parentId(0);
    authoredStyleId(&artboard, artboard.idOf(rootStyle));

    artboard.addObject(parent);
    parent->parentId(0);
    authoredClip(parent);
    auto* parentStyle = new LayoutComponentStyle();
    artboard.addObject(parentStyle);
    parentStyle->parentId(artboard.idOf(parent));
    authoredStyleId(parent, artboard.idOf(parentStyle));

    artboard.addObject(image);
    image->parentId(artboard.idOf(parent));
    artboard.addObject(participant);
    participant->parentId(artboard.idOf(image));
    REQUIRE(artboard.initialize() == StatusCode::Ok);
    // Live property setters may notify their artboard/parent. Configure them
    // only once the graph is linked, still before the first layout advance.
    artboard.width(640);
    artboard.height(421);
    parentStyle->layoutWidthScaleType((uint32_t)LayoutScaleType::fill);
    parentStyle->layoutHeightScaleType((uint32_t)LayoutScaleType::fill);
    participant->layoutWidthScaleType((uint32_t)LayoutScaleType::hug);
    participant->layoutHeightScaleType((uint32_t)LayoutScaleType::hug);
    participant->hugUnbounded(false);
    image->fit((uint32_t)fit);
    image->setAsset(assetOfSize(sourceWidth, sourceHeight));
  }

  void advance() { artboard.advance(0.0f); }

  rcp<ViewModelInstanceAssetImage> bindImageProperty()
  {
    auto property = make_rcp<ViewModelInstanceAssetImage>();
    auto* binding = new DataBind();
    binding->propertyKey(Image::assetIdPropertyKey);
    binding->target(image);
    binding->source(property);
    artboard.addDataBind(binding);
    binding->bind();
    return property;
  }

  void checkSlot(float width, float height)
  {
    CHECK(participant->resolvedWidth() == Approx(width));
    CHECK(participant->resolvedHeight() == Approx(height));
  }

  void checkCenteredCover()
  {
    const float width = participant->resolvedWidth();
    const float height = participant->resolvedHeight();
    Mat2D inverseParent;
    REQUIRE(parent->worldTransform().invert(&inverseParent));
    const auto center = inverseParent * image->worldTransform() * Vec2D(0, 0);
    CHECK(center.x == Approx(participant->resolvedLeft() + width * 0.5f));
    CHECK(center.y == Approx(participant->resolvedTop() + height * 0.5f));
    CHECK(image->computedWidth() >= width - 0.001f);
    CHECK(image->computedHeight() >= height - 0.001f);
    // Cover must touch at least one destination dimension, rather than add
    // extra zoom. Both axes use the same scale, preserving source aspect.
    CHECK(
        (image->computedWidth() == Approx(width) ||
         image->computedHeight() == Approx(height)));
    CHECK(image->renderScaleX() == Approx(image->renderScaleY()));
  }
};
} // namespace

constexpr ImageFit imageFits[] = {
    ImageFit::resize,
    ImageFit::contain,
    ImageFit::cover,
    ImageFit::fill,
    ImageFit::fitWidth,
    ImageFit::fitHeight,
    ImageFit::none,
    ImageFit::scaleDown};

TEST_CASE(
    "Hug image sizing remains intrinsic for every fit mode",
    "[image-layout]")
{
  for (auto fit : imageFits)
  {
    INFO("fit " << static_cast<uint32_t>(fit));
    for (auto size :
         {Vec2D(95, 95), Vec2D(96, 96), Vec2D(95, 160), Vec2D(640, 421)})
    {
      INFO("source " << size.x << "x" << size.y);
      ImageLayoutScene scene((uint32_t)size.x, (uint32_t)size.y, fit);
      scene.advance();
      CHECK(scene.parent->layoutWidth() == Approx(640));
      CHECK(scene.parent->layoutHeight() == Approx(421));
      scene.checkSlot(size.x, size.y);
      // A matching authored source can naturally occupy the parent. A small
      // source must not be enlarged merely because its fit is Cover.
      CHECK(scene.image->computedWidth() == Approx(size.x));
      CHECK(scene.image->computedHeight() == Approx(size.y));
    }
  }
}

TEST_CASE(
    "asset identity swaps preserve the existing Hug measurement behavior",
    "[image-layout]")
{
  for (auto fit : imageFits)
  {
    INFO("fit " << static_cast<uint32_t>(fit));
    for (auto initial : {Vec2D(96, 96), Vec2D(640, 421)})
    {
      INFO("initial source " << initial.x << "x" << initial.y);
      ImageLayoutScene scene((uint32_t)initial.x, (uint32_t)initial.y, fit);
      scene.advance();
      scene.checkSlot(initial.x, initial.y);
      auto replacement = assetOfSize(95, 160);
      scene.image->setAsset(replacement);
      scene.advance();
      // Scope guard for the redraw fix: replacing the asset identity does
      // not currently remeasure Hug. Changing that is a separate behavior
      // change; this test records the unmodified runtime's result.
      scene.checkSlot(initial.x, initial.y);
      replacement->renderImage(make_rcp<SizedImage>(95, 95));
      scene.advance();
      scene.checkSlot(95, 95);
      // Replacing the texture notifies the image and does remeasure Hug.
      // A subsequent identity swap still preserves that measured box.
      auto finalAsset = assetOfSize(640, 421);
      scene.image->setAsset(finalAsset);
      scene.advance();
      scene.checkSlot(95, 95);
      finalAsset->renderImage(make_rcp<SizedImage>(640, 421));
      scene.advance();
      scene.checkSlot(640, 421);
    }
  }
}

TEST_CASE(
    "an image swap drawn before its fit updates redraws without resizing Hug",
    "[image-layout][redraw]")
{
  ImageLayoutScene scene(640, 421);
  NoOpRenderer renderer;
  scene.advance();
  scene.artboard.draw(&renderer);
  scene.image->setAsset(assetOfSize(64, 64));
  scene.advance();
  scene.checkSlot(640, 421);
  REQUIRE(scene.image->worldTransform()[0] == Approx(640.0f / 64.0f));
  scene.artboard.draw(&renderer);
  REQUIRE_FALSE(scene.artboard.didChange());

  // The texture changes immediately, while the fit's world transform is
  // applied on the next update. A draw in between must not consume the
  // redraw needed for that updated transform.
  scene.image->setAsset(assetOfSize(96, 96));
  REQUIRE(scene.artboard.didChange());
  scene.artboard.draw(&renderer);
  REQUIRE_FALSE(scene.artboard.didChange());
  REQUIRE(scene.image->worldTransform()[0] == Approx(640.0f / 64.0f));
  scene.advance();
  scene.checkSlot(640, 421);
  scene.checkCenteredCover();
  CHECK(scene.image->worldTransform()[0] == Approx(640.0f / 96.0f));
  CHECK(scene.artboard.didChange());
  scene.artboard.draw(&renderer);
  scene.advance();
  CHECK_FALSE(scene.artboard.didChange());
}

TEST_CASE(
    "an already bound image requests redraw after pending fit updates apply",
    "[image-layout][image-binding-redraw][redraw]")
{
  ImageLayoutScene scene(640, 421);
  ImageIdentityRenderer renderer;
  constexpr float frameTime = 1.0f / 60.0f;
  scene.artboard.advance(frameTime);
  scene.artboard.draw(&renderer);
  auto property = scene.bindImageProperty();
  auto preview = make_rcp<SizedImage>(64, 64);
  property->value(preview.get());
  scene.artboard.advance(frameTime);
  REQUIRE(scene.image->imageAsset() == property->asset().get());
  scene.checkSlot(640, 421);
  REQUIRE(scene.image->worldTransform()[0] == Approx(10.0f));
  scene.artboard.draw(&renderer);
  REQUIRE(renderer.lastImage == preview.get());
  REQUIRE_FALSE(scene.artboard.didChange());

  // The same path used by a public view-model image property setter: the
  // already-bound private asset immediately notifies its image referencer.
  // No direct Image::setAsset call is used for either bound assignment.
  scene.artboard.advance(frameTime);
  auto photo = make_rcp<SizedImage>(96, 96);
  property->value(photo.get());
  REQUIRE(scene.image->imageAsset()->renderImage() == photo.get());
  const bool afterAssignment = scene.artboard.didChange();
  CHECK(afterAssignment);
  scene.artboard.draw(&renderer);
  REQUIRE(renderer.lastImage == photo.get());
  REQUIRE(scene.image->worldTransform()[0] == Approx(10.0f));
  const bool afterEarlyDraw = scene.artboard.didChange();
  CHECK_FALSE(afterEarlyDraw);

  scene.artboard.advance(frameTime);
  // Updating an existing asset's texture remeasures Hug intrinsically.
  // Preserve that behavior: this regression changes redraw bookkeeping,
  // not the parent size, image layout rules or resolved final transform.
  CHECK(scene.parent->layoutWidth() == Approx(640));
  CHECK(scene.parent->layoutHeight() == Approx(421));
  scene.checkSlot(96, 96);
  CHECK(scene.image->worldTransform()[0] == Approx(1.0f));
  const bool afterUpdate = scene.artboard.didChange();
  CHECK(afterUpdate);
  scene.artboard.draw(&renderer);
  const bool afterCorrectDraw = scene.artboard.didChange();
  CHECK_FALSE(afterCorrectDraw);
  scene.artboard.advance(frameTime);
  const bool afterIdle = scene.artboard.didChange();
  CHECK_FALSE(afterIdle);
}

TEST_CASE(
    "a first bound image identity swap requests redraw at unchanged dimensions",
    "[image-layout][image-binding-first-swap]")
{
  ImageLayoutScene scene(640, 421);
  ImageIdentityRenderer renderer;
  scene.advance();
  scene.artboard.draw(&renderer);
  auto original = ref_rcp(scene.image->imageAsset()->renderImage());
  REQUIRE(renderer.lastImage == original.get());
  REQUIRE_FALSE(scene.artboard.didChange());
  auto property = scene.bindImageProperty();
  auto replacement = make_rcp<SizedImage>(640, 421);
  REQUIRE(replacement.get() != original.get());
  property->value(replacement.get());
  REQUIRE(scene.artboard.didChange());
  SECTION("binding applied before drawing") {}
  SECTION("drawing before the first binding is applied")
  {
    scene.artboard.draw(&renderer);
    REQUIRE(renderer.lastImage == original.get());
    REQUIRE(scene.image->imageAsset()->renderImage() == original.get());
    REQUIRE_FALSE(scene.artboard.didChange());
  }
  scene.advance();
  REQUIRE(scene.image->imageAsset() == property->asset().get());
  REQUIRE(scene.image->imageAsset()->renderImage() == replacement.get());
  scene.checkSlot(640, 421);
  CHECK(scene.image->worldTransform()[0] == Approx(1.0f));
  const bool afterBindingApplied = scene.artboard.didChange();
  CHECK(afterBindingApplied);
  // Drawing directly verifies the new identity is present even if a host
  // relying on didChange would incorrectly skip submitting this frame.
  scene.artboard.draw(&renderer);
  REQUIRE(renderer.lastImage == replacement.get());
  CHECK_FALSE(scene.artboard.didChange());
}

TEST_CASE(
    "changing image fit does not redefine Hug dimensions",
    "[image-layout]")
{
  ImageLayoutScene scene(95, 160);
  scene.advance();
  scene.checkSlot(95, 160);
  for (auto fit : imageFits)
  {
    scene.image->fit((uint32_t)fit);
    scene.advance();
    scene.checkSlot(95, 160);
    CHECK(scene.image->computedWidth() == Approx(95));
    CHECK(scene.image->computedHeight() == Approx(160));
  }
}

TEST_CASE(
    "bounded unbounded and mixed image sizing retain their authored modes",
    "[image-layout]")
{
  for (auto fit : imageFits)
  {
    INFO("fit " << static_cast<uint32_t>(fit));
    ImageLayoutScene scene(96, 96, fit);
    for (bool unbounded : {false, true, false})
    {
      scene.participant->hugUnbounded(unbounded);
      scene.advance();
      scene.checkSlot(96, 96);
    }
    scene.artboard.width(320);
    scene.artboard.height(240);
    scene.advance();
    scene.checkSlot(96, 96);
    scene.participant->layoutWidthScaleType((uint32_t)LayoutScaleType::fixed);
    scene.participant->width(150);
    scene.advance();
    scene.checkSlot(150, 96);
    scene.participant->layoutWidthScaleType((uint32_t)LayoutScaleType::hug);
    scene.participant->layoutHeightScaleType((uint32_t)LayoutScaleType::fixed);
    scene.participant->height(80);
    scene.advance();
    scene.checkSlot(96, 80);
    scene.participant->layoutWidthScaleType((uint32_t)LayoutScaleType::fixed);
    scene.advance();
    scene.checkSlot(150, 80);
  }
}

TEST_CASE(
    "Cover preserves centered aspect fill when the image box is explicit",
    "[image-layout]")
{
  ImageLayoutScene scene;
  // Cover still fills an explicitly sized image component. This control is
  // distinct from Hug: the redraw fix must neither break fitting nor silently
  // change the authored sizing mode to achieve a particular appearance.
  scene.participant->layoutWidthScaleType((uint32_t)LayoutScaleType::fixed);
  scene.participant->layoutHeightScaleType((uint32_t)LayoutScaleType::fixed);
  scene.participant->width(640);
  scene.participant->height(421);
  for (auto size :
       {Vec2D(96, 96), Vec2D(640, 421), Vec2D(95, 160), Vec2D(1200, 800)})
  {
    INFO("source " << size.x << "x" << size.y);
    scene.image->setAsset(assetOfSize((uint32_t)size.x, (uint32_t)size.y));
    scene.advance();
    scene.checkSlot(640, 421);
    scene.checkCenteredCover();
  }
}

TEST_CASE(
    "image measurement preserves exact and intrinsic constraint semantics",
    "[image-layout]")
{
  const auto atMost = LayoutMeasureMode::atMost;
  const auto undefined = LayoutMeasureMode::undefined;
  const auto exactly = LayoutMeasureMode::exactly;
  const float nan = std::numeric_limits<float>::quiet_NaN();
  for (auto fit : imageFits)
  {
    ImageLayoutScene scene(95, 160, fit);
    for (auto available : {Vec2D(0, 0), Vec2D(10, 20), Vec2D(640, 421)})
    {
      const auto size = scene.image->measureLayout(
          available.x,
          atMost,
          available.y,
          atMost);
      CHECK(size.x == 95);
      CHECK(size.y == 160);
    }
    auto size = scene.image->measureLayout(640, exactly, nan, undefined);
    CHECK(size.x == 640);
    CHECK(size.y == 160);
    size = scene.image->measureLayout(nan, undefined, 421, exactly);
    CHECK(size.x == 95);
    CHECK(size.y == 421);
    size = scene.image->measureLayout(640, exactly, 421, exactly);
    CHECK(size.x == 640);
    CHECK(size.y == 421);

    // Non-participating images retain the same intrinsic measure path.
    Image standalone;
    standalone.fit((uint32_t)fit);
    standalone.setAsset(assetOfSize(95, 160));
    size = standalone.measureLayout(640, atMost, 421, atMost);
    CHECK(size.x == 95);
    CHECK(size.y == 160);
  }
}
