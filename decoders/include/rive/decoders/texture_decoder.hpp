/*
 * Copyright 2026 Rive
 */

#ifndef _RIVE_TEXTURE_DECODER_HPP_
#define _RIVE_TEXTURE_DECODER_HPP_

#include "rive/decoders/bitmap_decoder.hpp"
#include "rive/gpu_texture_format.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>

// Decode one mip level of block-compressed texture data to an RGBA Bitmap.
// `blocks` points at the start of the level's block grid; `byteCount` is its
// size in bytes. `width` / `height` are the level's logical pixel dimensions.
// `blockWidth` / `blockHeight` default to 4 (BC/ETC); ASTC callers pass the
// block footprint from the format.
//
// Returns nullptr if the format's software decoder was not compiled in or
// decoding fails.
//
// Build flags that enable each family:
//   RIVE_ASTC_DECODER  --  astc (any block size)
//   RIVE_BC_DECODER    --  bc1 / bc2 / bc3 / bc7
//   RIVE_ETC_DECODER   --  etc2 (RGB8 and RGBA8)
std::unique_ptr<Bitmap> decode_texture(const uint8_t* blocks,
                                       size_t byteCount,
                                       uint32_t width,
                                       uint32_t height,
                                       rive::GPUTextureFormat format,
                                       uint32_t blockWidth = 1,
                                       uint32_t blockHeight = 1);

#endif
