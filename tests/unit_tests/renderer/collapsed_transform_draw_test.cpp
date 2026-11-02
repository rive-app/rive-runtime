/*
 * Copyright 2026 Rive
 */

// A transform with determinant 0 collapses whatever is drawn under it onto a
// line or a point, so RiveRenderer drops those draws instead of tessellating
// them. (On the GPU they would otherwise leave slivers wherever rounding put
// the collapsed points slightly off their line.) Only an exactly singular
// transform counts: a nearly flat one still covers some area.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include "assets/nomoon.png.hpp"
#include <catch.hpp>
#include <cmath>
#include <cstring>
#include <functional>

using namespace rive;
using namespace rive::gpu;

namespace
{
class BatchCountingContext : public RenderContextNULL
{
public:
  size_t draws = 0;

  void flush(const FlushDescriptor& desc) override
  {
    // Feathers render into the atlas first.
    draws += desc.featherAtlasFillBatchCount +
             desc.featherAtlasStrokeBatchCount;
    if (desc.drawList == nullptr)
    {
      return;
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
      if (batch.drawType != DrawType::renderPassInitialize &&
          batch.drawType != DrawType::renderPassResolve)
      {
        ++draws;
      }
    }
  }
};

constexpr uint32_t TargetSize = 256;

// How many batches a frame flushes when 'draw' runs under 'matrix'.
size_t drawsUnder(
    const Mat2D& matrix,
    const std::function<void(RenderContext*, RiveRenderer&)>& draw,
    InterlockMode mode = InterlockMode::rasterOrdering)
{
  auto impl = std::make_unique<BatchCountingContext>();
  BatchCountingContext* counter = impl.get();
  auto target = impl->makeRenderTarget(TargetSize, TargetSize);
  RenderContext ctx(std::move(impl));
  ctx.beginFrame({
      .renderTargetWidth = TargetSize,
      .renderTargetHeight = TargetSize,
      .disableRasterOrdering = mode == InterlockMode::atomics,
  });
  REQUIRE(ctx.frameInterlockMode() == mode);
  {
    RiveRenderer renderer(&ctx);
    renderer.save();
    // Off the pixel grid, so collapsed bounds still span pixels and nothing
    // gets culled for empty bounds first.
    renderer.translate(100.37f, 100.61f);
    renderer.transform(matrix);
    draw(&ctx, renderer);
    renderer.restore();
  }
  ctx.flush({.renderTarget = target.get()});
  return counter->draws;
}

rcp<RenderPath> makeOval(RenderContext* ctx)
{
  // Four cubics around (0, 0), 70 wide.
  constexpr float k = 0.5522847f;
  auto path = ctx->makeEmptyRenderPath();
  path->moveTo(35, 0);
  path->cubicTo(35, 30 * k, 35 * k, 30, 0, 30);
  path->cubicTo(-35 * k, 30, -35, 30 * k, -35, 0);
  path->cubicTo(-35, -30 * k, -35 * k, -30, 0, -30);
  path->cubicTo(35 * k, -30, 35, -30 * k, 35, 0);
  path->close();
  return path;
}

const Mat2D kCollapsed[] = {
    Mat2D(1, 0, 0, 0, 0, 0),             // scale y 0
    Mat2D(0, 0, 0, 1, 0, 0),             // scale x 0
    Mat2D(0, 0, 0, 0, 0, 0),             // scale 0
    Mat2D(1, 1, 1, 1, 0, 0),             // skewed onto a line
    Mat2D(0.6f, 0.3f, 1.2f, 0.6f, 0, 0), // singular in float, not only in reals
    // A node scaled to 0 under a rotated parent: its world transform keeps an
    // exactly zero axis, whatever is applied above it.
    Mat2D::fromRotation(0.6f) * Mat2D(1, 0, 0, 0, 0, 0),
};

void fillOval(RenderContext* ctx, RiveRenderer& renderer)
{
  auto paint = ctx->makeRenderPaint();
  paint->color(0xff336699);
  renderer.drawPath(makeOval(ctx).get(), paint.get());
}

void strokeOval(RenderContext* ctx, RiveRenderer& renderer)
{
  auto paint = ctx->makeRenderPaint();
  paint->color(0xff000000);
  paint->style(RenderPaintStyle::stroke);
  paint->thickness(12);
  renderer.drawPath(makeOval(ctx).get(), paint.get());
}

void featherOval(RenderContext* ctx, RiveRenderer& renderer)
{
  auto paint = ctx->makeRenderPaint();
  paint->color(0xff006060);
  paint->feather(16);
  // Feathered fills only draw clockwise.
  auto oval = makeOval(ctx);
  oval->fillRule(FillRule::clockwise);
  renderer.drawPath(oval.get(), paint.get());
}

void outerStrokeOval(RenderContext* ctx, RiveRenderer& renderer)
{
  auto paint = ctx->makeRenderPaint();
  paint->color(0xff600060);
  paint->style(RenderPaintStyle::stroke);
  paint->thickness(8);
  paint->strokePosition(StrokePosition::outside);
  renderer.drawPath(makeOval(ctx).get(), paint.get());
}

void drawImage(RenderContext* ctx, RiveRenderer& renderer)
{
  auto image = ctx->decodeImage(assets::nomoon_png());
  REQUIRE(image != nullptr);
  renderer.drawImage(
      image.get(),
      ImageSampler::LinearClamp(),
      BlendMode::srcOver,
      1);
}

// A 50x50 textured quad.
struct Mesh
{
  rcp<RenderImage> image;
  rcp<RenderBuffer> vertices;
  rcp<RenderBuffer> uvs;
  rcp<RenderBuffer> indices;
};

Mesh makeMesh(RenderContext* ctx)
{
  const Vec2D quad[4] = {{0, 0}, {50, 0}, {50, 50}, {0, 50}};
  const Vec2D uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
  const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
  auto buffer = [ctx](RenderBufferType type, const void* source, size_t size) {
    auto b = ctx->makeRenderBuffer(type, RenderBufferFlags::none, size);
    memcpy(b->map(), source, size);
    b->unmap();
    return b;
  };
  Mesh mesh = {
      ctx->decodeImage(assets::nomoon_png()),
      buffer(RenderBufferType::vertex, quad, sizeof(quad)),
      buffer(RenderBufferType::vertex, uv, sizeof(uv)),
      buffer(RenderBufferType::index, idx, sizeof(idx)),
  };
  REQUIRE(mesh.image != nullptr);
  return mesh;
}

void drawImageMesh(RenderContext* ctx, RiveRenderer& renderer)
{
  Mesh mesh = makeMesh(ctx);
  renderer.drawImageMesh(
      mesh.image.get(),
      ImageSampler::LinearClamp(),
      mesh.vertices,
      mesh.uvs,
      mesh.indices,
      4,
      6,
      BlendMode::srcOver,
      1);
}

void drawImageMeshInstanced(RenderContext* ctx, RiveRenderer& renderer)
{
  Mesh mesh = makeMesh(ctx);
  auto instances = ctx->makeImageMeshInstances(3);
  Span<ImageMeshInstanceData> data = instances->edit();
  for (size_t i = 0; i < data.size(); ++i)
  {
    data[i].transform = Mat2D::fromTranslate(i * 20.0f, 0);
  }
  instances->endEdit();
  renderer.drawImageMeshInstanced(
      mesh.image.get(),
      ImageSampler::LinearClamp(),
      mesh.vertices,
      mesh.uvs,
      mesh.indices,
      4,
      6,
      instances);
}
} // namespace

TEST_CASE(
    "draws under a collapsed transform are dropped",
    "[RiveRenderer][collapsed]")
{
  const std::
      pair<const char*, std::function<void(RenderContext*, RiveRenderer&)>>
          draws[] = {
              {"fill", fillOval},
              {"stroke", strokeOval},
              {"feather", featherOval},
              {"outer stroke", outerStrokeOval},
              {"image", drawImage},
              {"image mesh", drawImageMesh},
              {"instanced image mesh", drawImageMeshInstanced},
          };
  // Atomic mode draws an instanced mesh one instance at a time.
  for (InterlockMode mode :
       {InterlockMode::rasterOrdering, InterlockMode::atomics})
  {
    INFO("interlock mode " << static_cast<int>(mode));
    for (const auto& [name, draw] : draws)
    {
      INFO(name);
      // Drawn normally, it flushes something...
      CHECK(drawsUnder(Mat2D(), draw, mode) > 0);
      // ...and collapsed in any way, nothing.
      for (const Mat2D& matrix : kCollapsed)
      {
        INFO(
            "matrix " << matrix[0] << " " << matrix[1] << " " << matrix[2]
                      << " " << matrix[3]);
        CHECK(drawsUnder(matrix, draw, mode) == 0);
      }
    }
  }
}

TEST_CASE(
    "a nearly collapsed transform still draws",
    "[RiveRenderer][collapsed]")
{
  // Very thin, but not zero area.
  CHECK(drawsUnder(Mat2D(1, 0, 0, 1e-3f, 0, 0), fillOval) > 0);
  CHECK(drawsUnder(Mat2D(0.6f, 0.3f, 1.2f, 0.61f, 0, 0), fillOval) > 0);
  CHECK(drawsUnder(Mat2D(1, 0, 0, 1e-3f, 0, 0), strokeOval) > 0);
  // Singular in real numbers, but not after rounding: a rotation applied on
  // both sides of a zero scale. Drawn, as before.
  const Mat2D rounded = Mat2D::fromRotation(0.6f) * Mat2D(1, 0, 0, 0, 0, 0) *
                        Mat2D::fromRotation(-1.1f);
  REQUIRE(
      static_cast<double>(rounded[0]) * rounded[3] !=
      static_cast<double>(rounded[1]) * rounded[2]);
  CHECK(drawsUnder(rounded, fillOval) > 0);
}

TEST_CASE("only the collapsed scope is dropped", "[RiveRenderer][collapsed]")
{
  size_t draws = drawsUnder(Mat2D(), [](RenderContext* ctx, RiveRenderer& r) {
    r.save();
    r.transform(Mat2D(1, 0, 0, 0, 0, 0));
    fillOval(ctx, r);
    r.restore();
  });
  CHECK(draws == 0);
  draws = drawsUnder(Mat2D(), [](RenderContext* ctx, RiveRenderer& r) {
    r.save();
    r.transform(Mat2D(1, 0, 0, 0, 0, 0));
    fillOval(ctx, r);
    r.restore();
    fillOval(ctx, r);
  });
  CHECK(draws > 0);
}
