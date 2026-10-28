/*
 * Copyright 2026 Rive
 */

// Inkbleed hairlines reuse the tessellation of their fill's first pass. If the
// draw sort ever orders a hairline's subpass differently from its fill's
// passes, the hairlines' patches stop being contiguous and they stop batching,
// which still renders correctly. These tests count batches and their
// ElementRanges instead of pixels.

#include "common/render_context_null.hpp"
#include "rive/renderer/rive_renderer.hpp"
#include <catch.hpp>
#include <array>
#include <map>
#include <vector>

using namespace rive;
using namespace rive::gpu;

namespace
{
class CapturingRenderContext : public RenderContextNULL
{
public:
  explicit CapturingRenderContext(bool supportsPipelineDynamicState)
  {
    m_platformFeatures
        .supportsPipelineDynamicState = supportsPipelineDynamicState;
  }

  void flush(const FlushDescriptor& desc) override
  {
    if (desc.drawList == nullptr)
    {
      return;
    }
    for (const DrawBatch& batch : *desc.drawList)
    {
      ++m_batchCounts[batch.drawType];
      for ([[maybe_unused]] const auto& range : batch.elementRanges())
      {
        ++m_rangeCounts[batch.drawType];
      }
    }
  }

  size_t count(DrawType drawType) const
  {
    return find(m_batchCounts, drawType);
  }

  size_t rangeCount(DrawType drawType) const
  {
    return find(m_rangeCounts, drawType);
  }

private:
  static size_t find(
      const std::map<DrawType, size_t>& counts,
      DrawType drawType)
  {
    auto it = counts.find(drawType);
    return it != counts.end() ? it->second : 0;
  }

  std::map<DrawType, size_t> m_batchCounts;
  std::map<DrawType, size_t> m_rangeCounts;
};

constexpr uint32_t TargetSize = 256;

// Opaque squares: a 6x6 grid that doesn't overlap (so it can share batches),
// then a 5x5 grid offset to overlap it (so it lands in later draw groups). With
// this many draws the sort is large enough that std::sort no longer falls back
// to insertion sort, which would hide an unstable ordering of equal keys.
constexpr int GridSize = 6;
constexpr float Cell = 40;
constexpr float Square = 30;
constexpr size_t PathCount = GridSize * GridSize +
                             (GridSize - 1) * (GridSize - 1);

std::vector<std::array<float, 4>> makeRects()
{
  std::vector<std::array<float, 4>> rects;
  for (int layer = 0; layer < 2; ++layer)
  {
    const int n = GridSize - layer;
    const float offset = 4 + layer * Cell / 2;
    for (int y = 0; y < n; ++y)
    {
      for (int x = 0; x < n; ++x)
      {
        const float l = offset + x * Cell, t = offset + y * Cell;
        rects.push_back({l, t, l + Square, t + Square});
      }
    }
  }
  assert(rects.size() == PathCount);
  return rects;
}

std::unique_ptr<RenderContext> drawRects(
    bool supportsPipelineDynamicState,
    CapturingRenderContext** capture)
{
  auto impl = std::make_unique<CapturingRenderContext>(
      supportsPipelineDynamicState);
  *capture = impl.get();
  auto ctx = std::make_unique<RenderContext>(std::move(impl));
  ctx->beginFrame({
      .renderTargetWidth = TargetSize,
      .renderTargetHeight = TargetSize,
      .msaaSampleCount = 1,
      .clockwiseFillOverride = true,
      .inkbleedOverride = true,
  });
  REQUIRE(ctx->frameInterlockMode() == InterlockMode::depthStencil);

  auto paint = ctx->makeRenderPaint();
  paint->color(0xff336699);
  RiveRenderer renderer(ctx.get());
  for (const auto& r : makeRects())
  {
    auto path = ctx->makeEmptyRenderPath();
    path->moveTo(r[0], r[1]);
    path->lineTo(r[2], r[1]);
    path->lineTo(r[2], r[3]);
    path->lineTo(r[0], r[3]);
    path->close();
    renderer.drawPath(path.get(), paint.get());
  }

  auto target = (*capture)->makeRenderTarget(TargetSize, TargetSize);
  ctx->flush({.renderTarget = target.get()});
  return ctx;
}
} // namespace

TEST_CASE(
    "opaque hairlines batch like their fills (dynamic state)",
    "[RiveRenderer][hairline]")
{
  CapturingRenderContext* capture;
  auto ctx = drawRects(/*supportsPipelineDynamicState=*/true, &capture);

  // Dynamic fills and hairlines each merge into one batch with a list of
  // ranges, and the hairlines' ranges line up with the fills'.
  CHECK(capture->count(DrawType::stencilDynamicMidpointFans) == 1);
  CHECK(capture->count(DrawType::depthAAOuterHairline) == 1);
  const size_t fills = capture->rangeCount(
      DrawType::stencilDynamicMidpointFans);
  const size_t hairlines = capture->rangeCount(DrawType::depthAAOuterHairline);
  CHECK(fills > 1);
  CHECK(fills < PathCount);
  CHECK(hairlines == fills);
}

TEST_CASE(
    "opaque hairlines batch like their fills (no dynamic state)",
    "[RiveRenderer][hairline]")
{
  CapturingRenderContext* capture;
  auto ctx = drawRects(/*supportsPipelineDynamicState=*/false, &capture);

  const size_t borrowedCoverage = capture->count(
      DrawType::stencilMidpointFanBorrowedCoverage);
  const size_t fans = capture->count(DrawType::stencilMidpointFans);
  const size_t resets = capture->count(DrawType::stencilMidpointFanReset);
  CHECK(fans > 1);
  CHECK(fans < PathCount);
  CHECK(borrowedCoverage == fans);
  CHECK(resets == fans);
  // Static fills can't hold a list of ranges, but hairlines can: they merge
  // into one batch with a range per fan batch.
  CHECK(capture->count(DrawType::depthAAOuterHairline) == 1);
  CHECK(capture->rangeCount(DrawType::depthAAOuterHairline) == fans);
}
