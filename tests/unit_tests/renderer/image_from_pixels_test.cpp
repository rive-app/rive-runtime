/*
 * Copyright 2026 Rive
 */

// Images made from pixels: the size check, and the recorded stream a deferred
// replay reads.

#include "rive/renderer/cmd/deferred_render_factory.hpp"
#include "rive/renderer/cmd/deferred_render_resource.hpp"
#include "rive/renderer/cmd/render_replay.hpp"
#include "rive/renderer/render_context.hpp"
#include "common/render_context_null.hpp"
#include "pixel_factory.hpp"
#include "utils/no_op_factory.hpp"
#include "utils/no_op_renderer.hpp"

#include <catch.hpp>
#include <vector>

using namespace rive;

namespace
{
cmd::RenderHandle imageId(RenderImage* image)
{
  return lite_rtti_cast<cmd::DeferredRenderImage*>(image)->id();
}
} // namespace

TEST_CASE(
    "validImagePixels rejects sizes that wrap or pass the limit",
    "[image_from_pixels]")
{
  std::vector<uint8_t> pixels(2 * 3 * 4);
  Span<const uint8_t> span(pixels.data(), pixels.size());
  CHECK(Factory::validImagePixels(2, 3, span));
  CHECK(!Factory::validImagePixels(3, 3, span));
  CHECK(!Factory::validImagePixels(0, 6, span));
  CHECK(
      !Factory::validImagePixels(2, 3, Span<const uint8_t>(pixels.data(), 23)));
  // 32768 x 32769 x 4 wraps to 131,072 in 32 bits; 0x80000000 squared x 4
  // wraps to 0 in 64. Neither may match a short span.
  std::vector<uint8_t> small(131072);
  CHECK(!Factory::validImagePixels(
      32768,
      32769,
      Span<const uint8_t>(small.data(), small.size())));
  CHECK(!Factory::validImagePixels(
      0x80000000u,
      0x80000000u,
      Span<const uint8_t>(small.data(), 0)));
  const uint32_t maxSize = Factory::maxImageSize;
  std::vector<uint8_t> row((maxSize + 1) * 4);
  CHECK(Factory::validImagePixels(
      maxSize,
      1,
      Span<const uint8_t>(row.data(), maxSize * 4)));
  CHECK(!Factory::validImagePixels(
      maxSize + 1,
      1,
      Span<const uint8_t>(row.data(), row.size())));
}

TEST_CASE(
    "A deferred image from pixels replays with its pixels",
    "[image_from_pixels]")
{
  cmd::DeferredFactory recorder;
  PixelFactory target;
  NoOpRenderer renderer;
  cmd::ResourceTable table;
  std::vector<uint8_t> pixels(2 * 3 * 4);
  pixels[0] = 0x11;
  pixels[1] = 0x22;
  pixels[2] = 0x33;
  pixels[3] = 0x44;
  Span<const uint8_t> span(pixels.data(), pixels.size());

  CHECK(recorder.makeImageFromPixels(3, 3, span) == nullptr);
  rcp<RenderImage> made = recorder.makeImageFromPixels(2, 3, span);
  REQUIRE(made != nullptr);
  // Sized at record time, so layout can read it during advance.
  CHECK(made->width() == 2);
  CHECK(made->height() == 3);
  // The recorder keeps its own copy.
  pixels[0] = 0;

  cmd::replayRenderCommands(
      &target,
      &renderer,
      recorder.commandBuffer(),
      table);
  RenderImage* replayed = table.images.get(imageId(made.get()));
  REQUIRE(replayed != nullptr);
  CHECK(replayed->width() == 2);
  CHECK(replayed->height() == 3);
  CHECK(target.firstPixel == 0x44332211u);
}

TEST_CASE("A factory without pixel images makes none", "[image_from_pixels]")
{
  NoOpFactory factory;
  std::vector<uint8_t> pixels(2 * 3 * 4);
  CHECK(
      factory.makeImageFromPixels(
          2,
          3,
          Span<const uint8_t>(pixels.data(), pixels.size())) == nullptr);
}

TEST_CASE(
    "A render context refuses images past its max texture size",
    "[image_from_pixels]")
{
  auto ctx = RenderContextNULL::MakeContext();
  const uint32_t maxSize = ctx->platformFeatures().maxTextureSize;
  std::vector<uint8_t> pixels((size_t)(maxSize + 1) * 4);
  Span<const uint8_t> fits(pixels.data(), (size_t)maxSize * 4);
  Span<const uint8_t> tooWide(pixels.data(), pixels.size());

  rcp<RenderImage> image = ctx->makeImageFromPixels(maxSize, 1, fits);
  REQUIRE(image != nullptr);
  CHECK(image->width() == (int)maxSize);
  CHECK(ctx->makeImageFromPixels(maxSize + 1, 1, tooWide) == nullptr);
  CHECK(ctx->makeImageFromPixels(1, maxSize + 1, tooWide) == nullptr);
}
