#ifndef _RIVE_TEXTURE_ARCHIVE_HPP_
#define _RIVE_TEXTURE_ARCHIVE_HPP_

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
    int blocksX;
    int blocksY;
    int bytesTotal;
    const uint8_t* blocks; // or offset into a big buffer
};

struct TextureData
{
    uint16_t Width;
    uint16_t Height;
    uint16_t PaddedWidth;
    uint16_t PaddedHeight;

    uint8_t blockSizeX;
    uint8_t blockSizeY;
    uint8_t bytesPerBlock;
    uint8_t NumMips;

    GPUTextureFormat format;

    std::vector<TextureMipLevel> mipLevels;

    // Combined Pixel Data size
    int totalBytes;
};

class TextureDirectory
{
public:
    std::vector<TextureData> dir;
    std::vector<uint8_t> dataBlob;

    void AddTexture(TextureData& header);
    bool Export(const std::string& path);
    bool Import(const std::string& path);
    bool Import(Span<const uint8_t> texBytes);
};

} // namespace rive

#endif
