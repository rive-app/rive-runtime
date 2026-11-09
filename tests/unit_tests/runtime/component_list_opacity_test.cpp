#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/shapes/paint/color.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <vector>

namespace
{
class OpacityProbePaint : public rive::RenderPaint
{
public:
  rive::ColorInt colorValue = 0xFF000000;
  void color(rive::ColorInt value) override { colorValue = value; }
  void style(rive::RenderPaintStyle) override {}
  void thickness(float) override {}
  void join(rive::StrokeJoin) override {}
  void cap(rive::StrokeCap) override {}
  void blendMode(rive::BlendMode) override {}
  void shader(rive::rcp<rive::RenderShader>) override {}
  void shaderTransform(const rive::Mat2D&) override {}
  void invalidateStroke() override {}
};

class OpacityProbeFactory : public rive::NoOpFactory
{
  rive::rcp<rive::RenderPaint> makeRenderPaint() override
  {
    return rive::make_rcp<OpacityProbePaint>();
  }
};

// Records, for every path drawn, the paint's own alpha and the opacity the
// renderer was modulating it by.
class OpacityProbeRenderer : public rive::Renderer
{
public:
  struct Draw
  {
    float alpha;
    float modulated;
  };
  std::vector<Draw> draws;

  void save() override { m_stack.push_back(m_stack.back()); }
  void restore() override { m_stack.pop_back(); }
  void transform(const rive::Mat2D&) override {}
  void clipPath(rive::RenderPath*) override {}
  void modulateOpacity(float opacity) override { m_stack.back() *= opacity; }
  void drawPath(rive::RenderPath*, rive::RenderPaint* paint) override
  {
    auto probe = static_cast<OpacityProbePaint*>(paint);
    draws.push_back({rive::colorOpacity(probe->colorValue), m_stack.back()});
  }
  void drawImage(
      const rive::RenderImage*,
      rive::ImageSampler,
      rive::BlendMode,
      float) override
  {}
  void drawImageMesh(
      const rive::RenderImage*,
      rive::ImageSampler,
      rive::rcp<rive::RenderBuffer>,
      rive::rcp<rive::RenderBuffer>,
      rive::rcp<rive::RenderBuffer>,
      uint32_t,
      uint32_t,
      rive::BlendMode,
      float) override
  {}

private:
  std::vector<float> m_stack{1.0f};
};

std::vector<rive::ShapePaint*> rowPaints(rive::ArtboardInstance* row)
{
  std::vector<rive::ShapePaint*> paints;
  for (auto object : row->objects())
  {
    if (object != nullptr && object->is<rive::ShapePaint>())
    {
      paints.push_back(object->as<rive::ShapePaint>());
    }
  }
  return paints;
}

struct ClippedViewportList
{
  OpacityProbeFactory factory;
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;

  // Six 200x50 rows in a virtualized list, four of them in its clipping
  // viewport. The artboard draws a background and an overlay around them.
  ClippedViewportList()
  {
    file = ReadRiveFile("assets/component_list_clipped_viewport.riv", &factory);
    artboard = file->artboardNamed("Main");
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    artboard->bindViewModelInstance(viewModelInstance);
    artboard->advance(0.0f);
    list = artboard->find<rive::ArtboardComponentList>("List");
  }

  std::vector<OpacityProbeRenderer::Draw> draw()
  {
    OpacityProbeRenderer renderer;
    artboard->draw(&renderer);
    return renderer.draws;
  }
};
} // namespace

TEST_CASE("A list fades its rows when it draws them", "[component_list]")
{
  ClippedViewportList scene;
  auto list = scene.list;
  REQUIRE(list != nullptr);
  auto row = list->artboardInstance(0);
  REQUIRE(row != nullptr);
  auto paints = rowPaints(row);
  REQUIRE(!paints.empty());
  const float paintOpacity = paints[0]->renderOpacity();
  REQUIRE(paintOpacity > 0.0f);

  // Background, four rows, overlay.
  auto opaque = scene.draw();
  REQUIRE(opaque.size() == 6);
  for (auto& draw : opaque)
  {
    REQUIRE(draw.modulated == 1.0f);
  }

  const uint32_t serial = row->changeSerial();
  list->opacity(0.5f);
  scene.artboard->advance(0.0f);

  // The fade never reaches the rows' components...
  CHECK(row->changeSerial() == serial);
  CHECK(row->opacity() == 1.0f);
  CHECK(paints[0]->renderOpacity() == paintOpacity);

  // ...it applies as the list draws them, and only them.
  auto faded = scene.draw();
  REQUIRE(faded.size() == opaque.size());
  for (size_t i = 0; i < faded.size(); i++)
  {
    INFO("draw " << i);
    CHECK(faded[i].alpha == opaque[i].alpha);
    const bool isRow = i >= 1 && i <= 4;
    CHECK(faded[i].modulated == (isRow ? 0.5f : 1.0f));
  }
}

TEST_CASE(
    "A list that isn't virtualized fades its rows when it draws them",
    "[component_list]")
{
  OpacityProbeFactory factory;
  auto file = ReadRiveFile("assets/component_list_1.riv", &factory);
  auto artboard = file->artboard("Main")->instance();
  auto viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
  artboard->bindViewModelInstance(viewModelInstance);
  artboard->advance(0.0f);
  auto list = artboard->find<rive::ArtboardComponentList>("List");
  REQUIRE(list != nullptr);
  REQUIRE(list->virtualizationEnabled() == false);
  REQUIRE(list->artboardCount() == 8);

  OpacityProbeRenderer opaque;
  artboard->draw(&opaque);

  list->opacity(0.25f);
  artboard->advance(0.0f);
  OpacityProbeRenderer faded;
  artboard->draw(&faded);

  REQUIRE(faded.draws.size() == opaque.draws.size());
  size_t rowDraws = 0;
  for (size_t i = 0; i < faded.draws.size(); i++)
  {
    INFO("draw " << i);
    CHECK(faded.draws[i].alpha == opaque.draws[i].alpha);
    CHECK(opaque.draws[i].modulated == 1.0f);
    if (faded.draws[i].modulated != 1.0f)
    {
      CHECK(faded.draws[i].modulated == 0.25f);
      rowDraws++;
    }
  }
  // Every row draws something.
  CHECK(rowDraws >= 8);
}

TEST_CASE("A transparent list hides its rows", "[component_list]")
{
  ClippedViewportList scene;
  auto list = scene.list;
  REQUIRE(list != nullptr);
  auto row = list->artboardInstance(0);
  REQUIRE(row != nullptr);
  auto paints = rowPaints(row);
  REQUIRE(!paints.empty());
  const float paintOpacity = paints[0]->renderOpacity();

  // At 0 the rows hear it: their content is hidden from hits, focus and
  // selection as it would be at 0 of its own.
  list->opacity(0.0f);
  scene.artboard->advance(0.0f);
  CHECK(row->childOpacity() == 0.0f);
  CHECK(paints[0]->renderOpacity() == 0.0f);
  CHECK(row->opacity() == 1.0f);
  // Background and overlay.
  CHECK(scene.draw().size() == 2);

  // Shown again in one advance.
  list->opacity(0.5f);
  scene.artboard->advance(0.0f);
  CHECK(row->childOpacity() == 1.0f);
  CHECK(paints[0]->renderOpacity() == paintOpacity);
  auto faded = scene.draw();
  REQUIRE(faded.size() == 6);
  for (size_t i = 1; i <= 4; i++)
  {
    CHECK(faded[i].modulated == 0.5f);
  }

  // A negative opacity hides them too, as focus and selection read it.
  list->opacity(-1.0f);
  scene.artboard->advance(0.0f);
  CHECK(row->childOpacity() == 0.0f);
}

TEST_CASE("A row's own opacity survives its list's", "[component_list]")
{
  ClippedViewportList scene;
  auto list = scene.list;
  REQUIRE(list != nullptr);
  auto row = list->artboardInstance(0);
  REQUIRE(row != nullptr);
  auto paints = rowPaints(row);
  REQUIRE(!paints.empty());
  const float paintOpacity = paints[0]->renderOpacity();

  // As an animation or a data bind on the row's artboard would set it.
  row->opacity(0.4f);
  list->opacity(0.5f);
  scene.artboard->advance(0.0f);

  CHECK(row->opacity() == 0.4f);
  CHECK(paints[0]->renderOpacity() == Approx(paintOpacity * 0.4f));
  auto faded = scene.draw();
  REQUIRE(faded.size() == 6);
  CHECK(faded[1].modulated == 0.5f);

  // Hidden and shown again, the row still has it.
  list->opacity(0.0f);
  scene.artboard->advance(0.0f);
  list->opacity(1.0f);
  scene.artboard->advance(0.0f);
  CHECK(row->opacity() == 0.4f);
  CHECK(paints[0]->renderOpacity() == Approx(paintOpacity * 0.4f));
}

TEST_CASE("Rows a transparent list adds start hidden", "[component_list]")
{
  ClippedViewportList scene;
  auto list = scene.list;
  REQUIRE(list != nullptr);
  REQUIRE(list->artboardInstance(5) == nullptr);
  auto scrolls = scene.artboard->find<rive::ScrollConstraint>();
  REQUIRE(scrolls.size() == 1);

  list->opacity(0.0f);
  scene.artboard->advance(0.0f);

  // Scrolling to the end pools the first rows and realizes the last ones,
  // fresh or from the pool.
  scrolls[0]->setScrollIndex(2);
  scene.artboard->advance(0.0f);
  for (int i = 2; i <= 5; i++)
  {
    INFO("row " << i);
    auto row = list->artboardInstance(i);
    REQUIRE(row != nullptr);
    CHECK(row->childOpacity() == 0.0f);
  }

  list->opacity(1.0f);
  scene.artboard->advance(0.0f);
  for (int i = 2; i <= 5; i++)
  {
    INFO("row " << i);
    CHECK(list->artboardInstance(i)->childOpacity() == 1.0f);
  }
}
