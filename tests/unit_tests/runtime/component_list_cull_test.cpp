#include "rive/artboard_component_list.hpp"
#include "rive/constraints/scrolling/scroll_constraint.hpp"
#include "rive/layout_component.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/viewmodel/viewmodel_instance.hpp"
#include "utils/no_op_factory.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <functional>
#include <vector>
#ifdef RIVE_CANVAS
#include "common/render_context_null.hpp"
#include "rive/layer_mask.hpp"
#include "rive/node.hpp"
#include "rive/renderer/cmd/deferred_session.hpp"
#include "rive/renderer/cmd/render_commands.hpp"
#include "rive/shapes/paint/fill.hpp"
#include "rive/shapes/paint/solid_color.hpp"
#include "rive/shapes/rectangle.hpp"
#include "rive/shapes/shape.hpp"
#endif

namespace
{
class CullProbePath : public rive::RenderPath
{
public:
  rive::RawPath rawPath;
  CullProbePath() {}
  CullProbePath(rive::RawPath& path) : rawPath(path) {}
  void rewind() override { rawPath.rewind(); }
  void fillRule(rive::FillRule) override {}
  void addPath(rive::CommandPath*, const rive::Mat2D&) override {}
  void addRenderPath(const rive::RenderPath*, const rive::Mat2D&) override {}
  void addRawPath(const rive::RawPath& path) override
  {
    rawPath.addPath(path, nullptr);
  }
  void moveTo(float, float) override {}
  void lineTo(float, float) override {}
  void cubicTo(float, float, float, float, float, float) override {}
  void close() override {}
};

class CullProbeFactory : public rive::NoOpFactory
{
  rive::rcp<rive::RenderPath> makeRenderPath(
      rive::RawPath& rawPath,
      rive::FillRule) override
  {
    return rive::make_rcp<CullProbePath>(rawPath);
  }
  rive::rcp<rive::RenderPath> makeEmptyRenderPath() override
  {
    return rive::make_rcp<CullProbePath>();
  }
};

// Records the device bounds of every path drawn. Reports its transform, as
// the GPU renderers do, unless told not to.
class CullProbeRenderer : public rive::Renderer
{
public:
  explicit CullProbeRenderer(bool reportsTransform = true)
      : m_reportsTransform(reportsTransform)
  {}

  std::vector<rive::AABB> draws;

  void save() override { m_stack.push_back(m_stack.back()); }
  void restore() override { m_stack.pop_back(); }
  void transform(const rive::Mat2D& m) override
  {
    m_stack.back() = m_stack.back() * m;
  }
  bool currentTransform(rive::Mat2D* out) const override
  {
    if (!m_reportsTransform)
    {
      return false;
    }
    *out = m_stack.back();
    return true;
  }
  void clipPath(rive::RenderPath*) override {}
  void modulateOpacity(float) override {}
  void drawPath(rive::RenderPath* path, rive::RenderPaint*) override
  {
    auto probe = static_cast<CullProbePath*>(path);
    draws.push_back(m_stack.back().mapBoundingBox(
        probe->rawPath.points().data(),
        probe->rawPath.points().size()));
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
  bool m_reportsTransform;
  std::vector<rive::Mat2D> m_stack{rive::Mat2D()};
};

// Six 200x50 rows, 10 apart, in a virtualized list in a 200x200 clipping
// viewport at (100, 100) of a 400x400 artboard, which draws a background
// before them and an overlay after. Two rows are buffered on each side.
struct BufferedList
{
  CullProbeFactory factory;
  rive::rcp<rive::File> file;
  std::unique_ptr<rive::ArtboardInstance> artboard;
  rive::rcp<rive::ViewModelInstance> viewModelInstance;
  rive::ArtboardComponentList* list = nullptr;
  rive::ScrollConstraint* scroll = nullptr;

  // `prepare` adds objects to an instance the scene is then cloned from. The
  // clone holds more objects than the file's artboard, so it sorts its own
  // update order rather than replaying the file's, which lacks them.
  explicit BufferedList(
      bool rowsClip = true,
      rive::Factory* fileFactory = nullptr,
      const std::function<void(rive::Artboard&)>& prepare = nullptr)
  {
    file = ReadRiveFile(
        "assets/component_list_clipped_viewport.riv",
        fileFactory != nullptr ? fileFactory : &factory);
    artboard = file->artboardNamed("Main");
    if (prepare)
    {
      prepare(*artboard);
      artboard = artboard->instance();
    }
    viewModelInstance = file->createDefaultViewModelInstance(artboard.get());
    artboard->bindViewModelInstance(viewModelInstance);
    auto scrolls = artboard->find<rive::ScrollConstraint>();
    REQUIRE(scrolls.size() == 1);
    scroll = scrolls[0];
    scroll->virtualizeBuffer(2);
    artboard->advance(0.0f);
    list = artboard->find<rive::ArtboardComponentList>("List");
    REQUIRE(list != nullptr);
    setRowsClip(rowsClip);
  }

  void setRowsClip(bool value)
  {
    for (int i = 0; i < (int)list->artboardCount(); i++)
    {
      if (auto row = list->artboardInstance(i))
      {
        row->clip(value);
      }
    }
  }

  std::string realized()
  {
    std::string indices;
    for (int i = 0; i < (int)list->artboardCount(); i++)
    {
      if (list->artboardInstance(i) != nullptr)
      {
        indices += std::to_string(i) + " ";
      }
    }
    return indices;
  }

  std::vector<rive::AABB> draw(bool reportsTransform = true)
  {
    CullProbeRenderer renderer(reportsTransform);
    artboard->draw(&renderer);
    return renderer.draws;
  }
};

const rive::AABB kViewport(100.0f, 100.0f, 300.0f, 300.0f);

bool overlapsViewport(const rive::AABB& bounds)
{
  return bounds.maxX > kViewport.minX && bounds.minX < kViewport.maxX &&
         bounds.maxY > kViewport.minY && bounds.minY < kViewport.maxY;
}
} // namespace

TEST_CASE(
    "A virtualized list skips buffered rows its viewport clips away",
    "[component_list]")
{
  BufferedList scene;
  // Rows 0-3 show, 4 and 5 are buffered below the viewport.
  REQUIRE(scene.realized() == "0 1 2 3 4 5 ");

  // Background, the four rows that show, overlay.
  auto draws = scene.draw();
  REQUIRE(draws.size() == 6);
  for (size_t i = 1; i <= 4; i++)
  {
    INFO("draw " << i);
    CHECK(overlapsViewport(draws[i]));
  }
  // A skipped row takes its change as a draw would.
  CHECK(scene.list->artboardInstance(5)->didChange() == false);

  // Scrolled to the end, the rows buffered above are skipped.
  scene.scroll->setScrollIndex(2);
  scene.artboard->advance(0.0f);
  REQUIRE(scene.realized() == "0 1 2 3 4 5 ");
  scene.setRowsClip(true);
  draws = scene.draw();
  REQUIRE(draws.size() == 6);
  for (size_t i = 1; i <= 4; i++)
  {
    INFO("draw " << i);
    CHECK(overlapsViewport(draws[i]));
  }
}

TEST_CASE(
    "A virtualized list draws buffered rows that could show",
    "[component_list]")
{
  SECTION("rows that don't clip their content")
  {
    BufferedList scene(false);
    CHECK(scene.draw().size() == 8);
  }

  SECTION("a renderer that doesn't report its transform")
  {
    BufferedList scene;
    CHECK(scene.draw(false).size() == 8);
  }

  SECTION("a viewport that doesn't clip")
  {
    BufferedList scene;
    auto viewport = scene.scroll->viewport();
    viewport->clip(false);
    scene.artboard->advance(0.0f);
    REQUIRE(scene.realized() == "0 1 2 3 4 5 ");
    CHECK(scene.draw().size() == 8);
  }

  SECTION("a list drawn outside its viewport's clip")
  {
    // As a draw rule moving it out of the viewport's range would.
    BufferedList scene;
    CullProbeRenderer renderer;
    scene.list->draw(&renderer);
    CHECK(renderer.draws.size() == 6);
  }

  SECTION("a row with its own rotation")
  {
    BufferedList scene;
    scene.list->artboardInstance(5)->rotation(0.1f);
    scene.artboard->advance(0.0f);
    CHECK(scene.draw().size() == 7);
  }

  SECTION("a visitor that moves the list after the viewport clipped")
  {
    // Up two rows, which brings the rows buffered below into the clip.
    BufferedList scene;
    scene.list->markHasCustomProperties();
    CullProbeRenderer renderer;
    scene.artboard->drawInternal(
        &renderer,
        [](void*, rive::Drawable* drawable, rive::Renderer* renderer) {
          renderer->transform(rive::Mat2D::fromTranslate(0.0f, -120.0f));
          drawable->draw(renderer);
        });
    REQUIRE(renderer.draws.size() == 8);
    CHECK(overlapsViewport(renderer.draws[5]));
    CHECK(overlapsViewport(renderer.draws[6]));
  }
}

#ifdef RIVE_CANVAS
namespace
{
// The paths drawn into each offscreen canvas this frame, in the order the
// canvases opened.
std::vector<size_t> canvasPathCounts(
    const rive::cmd::RenderCommandBuffer& buffer)
{
  using rive::cmd::RenderCmd;
  rive::cmd::CommandReader<uint8_t> reader(
      buffer.commandBytes(),
      buffer.blobBytes());
  std::vector<size_t> counts;
  bool inCanvas = false;
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
      counts.push_back(0);
      inCanvas = true;
    }
    else if (cmd == RenderCmd::canvasContentEnd)
    {
      inCanvas = false;
    }
    else if (cmd == RenderCmd::drawPath && inCanvas)
    {
      counts.back()++;
    }
    reader.skip(rive::cmd::payloadSizeOf(cmd));
  }
  return counts;
}

// Masks the viewport's content with a rectangle covering the whole artboard.
void maskContent(rive::Artboard& artboard)
{
  auto* group = new rive::Node();
  artboard.addObject(group);
  group->parentId(0);
  auto* shape = new rive::Shape();
  shape->x(200.0f);
  shape->y(200.0f);
  artboard.addObject(shape);
  shape->parentId(artboard.idOf(group));
  auto* rect = new rive::Rectangle();
  rect->width(400.0f);
  rect->height(400.0f);
  artboard.addObject(rect);
  rect->parentId(artboard.idOf(shape));
  auto* fill = new rive::Fill();
  artboard.addObject(fill);
  fill->parentId(artboard.idOf(shape));
  auto* color = new rive::SolidColor();
  color->colorValue(0xFFFFFFFF);
  artboard.addObject(color);
  color->parentId(artboard.idOf(fill));

  auto* mask = new rive::LayerMask();
  artboard.addObject(mask);
  mask->parentId(
      artboard.idOf(artboard.find<rive::LayoutComponent>("Content")));
  mask->sourceId(artboard.idOf(group));
}
} // namespace

TEST_CASE(
    "A layer mask over a virtualized list rasters its buffered rows",
    "[component_list]")
{
  // The mask's range is the list and its layout. The viewport's clip is on
  // the frame's renderer, not the mask's canvas, and the canvas is kept
  // until something in the range changes, which the viewport isn't.
  auto renderContext = RenderContextNULL::MakeContext();
  rive::cmd::DeferredSession session{rive::ore::ReplayCaps{}};
  session.bindRenderContext(renderContext.get());
  BufferedList scene(true, &session, maskContent);
  REQUIRE(scene.realized() == "0 1 2 3 4 5 ");

  auto frame = [&]() {
    scene.artboard->advance(0.0f);
    scene.artboard->drawInternal(session.screenRenderer());
    auto counts = canvasPathCounts(session.commandBuffer());
    session.resetFrame();
    return counts;
  };

  // Coverage, then content: every row, buffered ones included.
  auto counts = frame();
  REQUIRE(counts.size() == 2);
  CHECK(counts[1] == 6);

  // The viewport stops clipping. Nothing in the range changed, so the
  // canvas is reused, and a row left out of it would stay missing.
  scene.scroll->viewport()->clip(false);
  CHECK(frame().empty());
}
#endif
