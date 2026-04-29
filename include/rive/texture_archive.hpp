#pragma once

#include "rive/span.hpp"

#include <cstdint>
#include <vector>
#include <string>

namespace rive
{

// On-disk header for a .rtex archive. Fields are little-endian; all Rive
// runtime targets (web/wasm, ARM iOS/Android, x86_64 desktop, Apple Silicon)
// are little-endian, so the struct is written and read by raw memcpy.
#pragma pack(push, 1)
struct TextureArchiveHeader
{
    static constexpr char kMagic[4] = {'R', 'T', 'E', 'X'};
    static constexpr uint16_t kCurrentVersion = 1;

    char magic[4];
    uint16_t version;
    uint16_t textureCount;
};
#pragma pack(pop)
static_assert(sizeof(TextureArchiveHeader) == 8,
              "TextureArchiveHeader must be tightly packed (8 bytes)");

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
    bool exportArchive(const std::string& path);
    bool import(const std::string& path);
    bool import(Span<const uint8_t> texBytes);
};
} // namespace rive
