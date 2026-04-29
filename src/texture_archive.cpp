#include "rive/texture_archive.hpp"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>

using namespace rive;

namespace
{
// Per-texture descriptor on disk: 4*u16 + 4*u8 + 1*u8(format) = 13 bytes.
constexpr size_t kDescriptorSize = 13;
// Per-mip entry on disk: 3*u32 = 12 bytes.
constexpr size_t kMipEntrySize = 12;

// Defensive caps to bound allocation and reject hostile inputs early.
constexpr uint16_t kMaxTextures = 4096;
constexpr uint8_t kMaxMipsPerTexture = 16;
// 256 MB per mip is far above any realistic compressed texture.
constexpr uint32_t kMaxBytesPerMip = 256u * 1024u * 1024u;
} // namespace

// ---------------------------------------------------------------------------
// Binary file format (.rtex)
//
//   [4]  Magic: "RTEX"
//   [2]  Version: uint16_t  (currently 1)
//   [2]  TextureCount: uint16_t
//   [per texture]:
//     [2]  width:        uint16_t  (original, un-padded)
//     [2]  height:       uint16_t
//     [2]  paddedWidth:  uint16_t
//     [2]  paddedHeight: uint16_t
//     [1]  blockSizeX:   uint8_t
//     [1]  blockSizeY:   uint8_t
//     [1]  bytesPerBlock:uint8_t
//     [1]  numMips:      uint8_t
//     [1]  format:       uint8_t  (GPUTextureFormat)
//     [per mip (numMips entries)]:
//       [4]  blocksX:    uint32_t
//       [4]  blocksY:    uint32_t
//       [4]  bytesTotal: uint32_t
//   [data blob: all mip data for every texture, in declaration order]
// ---------------------------------------------------------------------------

void TextureDirectory::addTexture(TextureData& td)
{
    const uint8_t* oldBase = dataBlob.empty() ? nullptr : dataBlob.data();

    std::vector<size_t> offsets;
    offsets.reserve(td.mipLevels.size());
    for (auto& mip : td.mipLevels)
    {
        offsets.push_back(dataBlob.size());
        dataBlob.insert(dataBlob.end(),
                        mip.blocks,
                        mip.blocks + mip.bytesTotal);
    }

    const uint8_t* newBase = dataBlob.data();

    if (oldBase != nullptr && oldBase != newBase)
    {
        for (auto& existing : dir)
            for (auto& mip : existing.mipLevels)
                mip.blocks = newBase + (mip.blocks - oldBase);
    }

    for (size_t i = 0; i < td.mipLevels.size(); ++i)
        td.mipLevels[i].blocks = newBase + offsets[i];

    dir.push_back(td);
}

bool TextureDirectory::exportArchive(const std::string& path)
{
    FILE* fp = std::fopen(path.c_str(), "wb");
    if (!fp)
    {
        std::fprintf(stderr,
                     "TextureDirectory::exportArchive: cannot open '%s'\n",
                     path.c_str());
        return false;
    }

    TextureArchiveHeader header;
    std::memcpy(header.magic,
                TextureArchiveHeader::kMagic,
                sizeof(header.magic));
    header.version = TextureArchiveHeader::kCurrentVersion;
    header.textureCount = static_cast<uint16_t>(dir.size());
    std::fwrite(&header, sizeof(header), 1, fp);

    for (const auto& td : dir)
    {
        std::fwrite(&td.width, sizeof(td.width), 1, fp);
        std::fwrite(&td.height, sizeof(td.height), 1, fp);
        std::fwrite(&td.paddedWidth, sizeof(td.paddedWidth), 1, fp);
        std::fwrite(&td.paddedHeight, sizeof(td.paddedHeight), 1, fp);
        std::fwrite(&td.blockSizeX, sizeof(td.blockSizeX), 1, fp);
        std::fwrite(&td.blockSizeY, sizeof(td.blockSizeY), 1, fp);
        std::fwrite(&td.bytesPerBlock, sizeof(td.bytesPerBlock), 1, fp);
        std::fwrite(&td.numMips, sizeof(td.numMips), 1, fp);
        const uint8_t fmt = static_cast<uint8_t>(td.format);
        std::fwrite(&fmt, sizeof(fmt), 1, fp);

        for (const auto& mip : td.mipLevels)
        {
            const uint32_t bx = static_cast<uint32_t>(mip.blocksX);
            const uint32_t by = static_cast<uint32_t>(mip.blocksY);
            const uint32_t bt = mip.bytesTotal;
            std::fwrite(&bx, sizeof(bx), 1, fp);
            std::fwrite(&by, sizeof(by), 1, fp);
            std::fwrite(&bt, sizeof(bt), 1, fp);
        }
    }

    std::fwrite(dataBlob.data(), 1, dataBlob.size(), fp);
    std::fclose(fp);
    return true;
}

bool TextureDirectory::import(Span<const uint8_t> texBytes)
{
    dir.clear();
    dataBlob.clear();

    if (texBytes.size() < sizeof(TextureArchiveHeader))
    {
        printf("TextureDirectory::import: file too small\n");
        return false;
    }

    const uint8_t* p = texBytes.data();
    const uint8_t* end = p + texBytes.size();

    TextureArchiveHeader header;
    std::memcpy(&header, p, sizeof(header));
    p += sizeof(header);

    if (std::memcmp(header.magic,
                    TextureArchiveHeader::kMagic,
                    sizeof(header.magic)) != 0)
    {
        printf("TextureDirectory::import: bad magic\n");
        return false;
    }
    if (header.version != TextureArchiveHeader::kCurrentVersion)
    {
        printf("TextureDirectory::import: unsupported version %u\n",
               static_cast<unsigned>(header.version));
        return false;
    }

    const uint16_t count = header.textureCount;
    if (count > kMaxTextures)
    {
        printf("TextureDirectory::import: textureCount %u exceeds cap %u\n",
               static_cast<unsigned>(count),
               static_cast<unsigned>(kMaxTextures));
        return false;
    }

    struct MipDesc
    {
        uint32_t blocksX, blocksY, bytesTotal;
    };
    struct TexDesc
    {
        TextureData td;
        std::vector<MipDesc> mips;
    };

    std::vector<TexDesc> descs(count);
    size_t totalData = 0;

    for (auto& desc : descs)
    {
        TextureData& td = desc.td;
        if (static_cast<size_t>(end - p) < kDescriptorSize)
        {
            printf("TextureDirectory::import: truncated descriptor\n");
            return false;
        }
        std::memcpy(&td.width, p, 2);
        p += 2;
        std::memcpy(&td.height, p, 2);
        p += 2;
        std::memcpy(&td.paddedWidth, p, 2);
        p += 2;
        std::memcpy(&td.paddedHeight, p, 2);
        p += 2;
        td.blockSizeX = *p++;
        td.blockSizeY = *p++;
        td.bytesPerBlock = *p++;
        td.numMips = *p++;
        td.format = static_cast<GPUTextureFormat>(*p++);

        if (td.numMips > kMaxMipsPerTexture)
        {
            printf("TextureDirectory::import: numMips %u exceeds cap %u\n",
                   static_cast<unsigned>(td.numMips),
                   static_cast<unsigned>(kMaxMipsPerTexture));
            return false;
        }
        desc.mips.resize(td.numMips);
        const size_t mipTableBytes =
            static_cast<size_t>(td.numMips) * kMipEntrySize;
        if (static_cast<size_t>(end - p) < mipTableBytes)
        {
            printf("TextureDirectory::import: truncated mip table\n");
            return false;
        }
        td.totalBytes = 0;
        for (auto& m : desc.mips)
        {
            std::memcpy(&m.blocksX, p, 4);
            p += 4;
            std::memcpy(&m.blocksY, p, 4);
            p += 4;
            std::memcpy(&m.bytesTotal, p, 4);
            p += 4;
            if (m.bytesTotal > kMaxBytesPerMip)
            {
                printf("TextureDirectory::import: mip bytesTotal %u "
                       "exceeds cap %u\n",
                       static_cast<unsigned>(m.bytesTotal),
                       static_cast<unsigned>(kMaxBytesPerMip));
                return false;
            }
            // Sum without overflow: cap remaining headroom.
            if (m.bytesTotal >
                std::numeric_limits<uint32_t>::max() - td.totalBytes)
            {
                printf("TextureDirectory::import: totalBytes overflow\n");
                return false;
            }
            td.totalBytes += m.bytesTotal;
        }
        if (td.totalBytes > std::numeric_limits<size_t>::max() - totalData)
        {
            printf("TextureDirectory::import: totalData overflow\n");
            return false;
        }
        totalData += td.totalBytes;
    }

    if (static_cast<size_t>(end - p) != totalData)
    {
        printf("TextureDirectory::import: blob size mismatch "
               "(expected %zu, got %zu)\n",
               totalData,
               static_cast<size_t>(end - p));
        return false;
    }

    dataBlob.assign(p, end);
    const uint8_t* blob = dataBlob.data();
    size_t offset = 0;

    dir.reserve(count);
    for (auto& desc : descs)
    {
        TextureData& td = desc.td;
        td.mipLevels.resize(desc.mips.size());
        for (size_t i = 0; i < desc.mips.size(); i++)
        {
            const MipDesc& m = desc.mips[i];
            if (m.blocksX > std::numeric_limits<uint16_t>::max() ||
                m.blocksY > std::numeric_limits<uint16_t>::max())
            {
                printf("TextureDirectory::import: blocks dimension "
                       "exceeds uint16\n");
                return false;
            }
            td.mipLevels[i].blocksX = static_cast<uint16_t>(m.blocksX);
            td.mipLevels[i].blocksY = static_cast<uint16_t>(m.blocksY);
            td.mipLevels[i].bytesTotal = m.bytesTotal;
            td.mipLevels[i].blocks = blob + offset;
            offset += m.bytesTotal;
        }
        dir.push_back(std::move(td));
    }

    return true;
}

bool TextureDirectory::import(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    if (!in)
    {
        printf("TextureDirectory::import: cannot open %s\n", path.c_str());
        return false;
    }
    const std::streamsize len = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(static_cast<size_t>(len));
    if (!in.read(reinterpret_cast<char*>(buf.data()), len))
    {
        printf("TextureDirectory::import: read failed for %s\n", path.c_str());
        return false;
    }
    return import(Span<const uint8_t>(buf.data(), buf.size()));
}
