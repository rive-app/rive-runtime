/*
 * Copyright 2022 Rive
 */

#pragma once

#include <stdint.h>
#include <vector>

enum class PNGCompression
{
    fast_rle,
    compact
};

void WritePNGFile(uint8_t* pixels,
                  int width,
                  int height,
                  bool flipY,
                  const char* file_name,
                  PNGCompression);

std::vector<uint8_t> EncodePNGToBuffer(uint32_t width,
                                       uint32_t height,
                                       uint8_t* imageDataRGBA,
                                       PNGCompression);
