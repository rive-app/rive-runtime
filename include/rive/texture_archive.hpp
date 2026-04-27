#pragma once

#include "rive/span.hpp"

#include <vector>
#include <string>

namespace rive
{

// Enum class to describe the format of a texture.
// Only formats which are directly read by a GPU are listed here
enum class GPUTextureFormat : uint8_t
{
    rgba32,
    bc1,
    bc2,
    bc3,
    bc7,
    astc,
    etc2
};

// Block size of various GPUTextureFormats
enum class GPUTextureFormatBlockSize : uint8_t
{
    p_1x1, // Pixels are 1x1 for uncompressed
    b_4x4, // B means Block here
    b_6x6,
    b_8x8,
    b_12x12,
};

struct TextureMipLevel
{
    uint16_t blocksX;
    uint16_t blocksY;
    uint32_t bytesTotal;
    const uint8_t* blocks; // or offset into a big buffer
};

struct TextureData
{
    uint16_t width;
    uint16_t height;
    uint16_t paddedWidth;
    uint16_t paddedHeight;

    uint8_t blockSizeX;
    uint8_t blockSizeY;
    uint8_t bytesPerBlock;
    uint8_t numMips;

    GPUTextureFormat format;

    std::vector<TextureMipLevel> mipLevels;

    // Combined pixel data size
    uint32_t totalBytes;
};

class TextureDirectory
{
public:
    std::vector<TextureData> dir;
    std::vector<uint8_t> dataBlob;

    void addTexture(TextureData& header);
    void exportArchive();
    bool import(const std::string& path);
    bool import(Span<const uint8_t> texBytes);
};
} // namespace rive
