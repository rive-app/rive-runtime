/*
 * Copyright 2026 Rive
 */

#pragma once

#include "utils/no_op_factory.hpp"

#include <cstring>

namespace rive
{
// Makes images the size it is given, as a renderer's factory would, and keeps
// the first pixel of the last one.
class PixelFactory : public NoOpFactory
{
public:
  uint32_t firstPixel = 0;

protected:
  rcp<RenderImage> onMakeImageFromPixels(
      uint32_t width,
      uint32_t height,
      Span<const uint8_t> rgbaPremul) override
  {
    memcpy(&firstPixel, rgbaPremul.data(), 4);
    return make_rcp<SizedImage>(width, height);
  }

private:
  class SizedImage : public RenderImage
  {
  public:
    SizedImage(uint32_t width, uint32_t height)
    {
      m_Width = (int)width;
      m_Height = (int)height;
    }
  };
};
} // namespace rive
