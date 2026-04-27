#include "rive/texture_archive.hpp"

#include <fstream>
#include <cstring>

using namespace rive;

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

bool TextureDirectory::import(Span<const uint8_t> texBytes)
{
    dir.clear();
    dataBlob.clear();

    // Cursor-based reader helpers
    size_t pos = 0;
    const size_t total = texBytes.size();

    auto readBytes = [&](void* dst, size_t n) -> bool {
        if (pos + n > total)
            return false;
        std::memcpy(dst, texBytes.data() + pos, n);
        pos += n;
        return true;
    };

    // Magic: 'TXTR'
    char magic[4];
    if (!readBytes(magic, 4))
    {
        printf("TextureDirectory::Import: truncated magic\n");
        return false;
    }
    if (magic[0] != 'T' || magic[1] != 'X' || magic[2] != 'T' ||
        magic[3] != 'R')
    {
        printf("TextureDirectory::Import: bad magic\n");
        return false;
    }

    uint32_t version = 0, textureCount = 0;
    if (!readBytes(&version, 4) || !readBytes(&textureCount, 4))
    {
        printf("TextureDirectory::Import: truncated header\n");
        return false;
    }

    uint32_t metaSize = 0;
    if (!readBytes(&metaSize, 4))
    {
        printf("TextureDirectory::Import: truncated metaSize\n");
        return false;
    }

    std::vector<uint8_t> metaBuf(metaSize);
    if (metaSize > 0 && !readBytes(metaBuf.data(), metaSize))
    {
        printf("TextureDirectory::Import: truncated metadata\n");
        return false;
    }

    uint32_t uncompressedSize = 0, compressedSize = 0;
    if (!readBytes(&uncompressedSize, 4) || !readBytes(&compressedSize, 4))
    {
        printf("TextureDirectory::Import: truncated data sizes\n");
        return false;
    }

    if (uncompressedSize > 0)
    {
        dataBlob.resize(uncompressedSize);
        if (compressedSize == uncompressedSize)
        {
            // Uncompressed — read directly
            if (!readBytes(dataBlob.data(), uncompressedSize))
            {
                printf("TextureDirectory::Import: truncated data blob\n");
                return false;
            }
        }
        else
        {
            // Compressed — skip for now (LZ4 not yet integrated)
            printf(
                "TextureDirectory::Import: compressed blobs not yet supported\n");
            return false;
        }
    }

    // Parse metadata buffer
    size_t mpos = 0;
    auto readMeta = [&](void* dst, size_t n) -> bool {
        if (mpos + n > metaBuf.size())
            return false;
        std::memcpy(dst, metaBuf.data() + mpos, n);
        mpos += n;
        return true;
    };

    for (uint32_t t = 0; t < textureCount; ++t)
    {
        TextureData tex = {};

        if (!readMeta(&tex.width, 2) || !readMeta(&tex.height, 2) ||
            !readMeta(&tex.paddedWidth, 2) || !readMeta(&tex.paddedHeight, 2))
        {
            printf("TextureDirectory::Import: truncated dims for texture %u\n",
                   t);
            return false;
        }

        if (!readMeta(&tex.blockSizeX, 1) || !readMeta(&tex.blockSizeY, 1) ||
            !readMeta(&tex.bytesPerBlock, 1) || !readMeta(&tex.numMips, 1))
        {
            printf(
                "TextureDirectory::Import: truncated block info for texture %u\n",
                t);
            return false;
        }

        uint32_t fmt = 0;
        if (!readMeta(&fmt, 4))
        {
            printf(
                "TextureDirectory::Import: truncated format for texture %u\n",
                t);
            return false;
        }
        tex.format = static_cast<GPUTextureFormat>(fmt);

        uint32_t totalBytes = 0;
        if (!readMeta(&totalBytes, 4))
        {
            printf(
                "TextureDirectory::Import: truncated totalBytes for texture %u\n",
                t);
            return false;
        }
        tex.totalBytes = static_cast<int>(totalBytes);

        uint32_t mipCount = 0;
        if (!readMeta(&mipCount, 4))
        {
            printf(
                "TextureDirectory::Import: truncated mipCount for texture %u\n",
                t);
            return false;
        }

        for (uint32_t m = 0; m < mipCount; ++m)
        {
            int32_t blocksX = 0, blocksY = 0, bytesTotal = 0;
            uint32_t offset = 0;
            if (!readMeta(&blocksX, 4) || !readMeta(&blocksY, 4) ||
                !readMeta(&bytesTotal, 4) || !readMeta(&offset, 4))
            {
                printf(
                    "TextureDirectory::Import: truncated mip %u for texture %u\n",
                    m,
                    t);
                return false;
            }

            if (bytesTotal < 0 ||
                offset + static_cast<uint32_t>(bytesTotal) > dataBlob.size())
            {
                printf(
                    "TextureDirectory::Import: invalid mip range t=%u m=%u\n",
                    t,
                    m);
                return false;
            }

            TextureMipLevel mip = {};
            mip.blocksX = blocksX;
            mip.blocksY = blocksY;
            mip.bytesTotal = bytesTotal;
            mip.blocks = dataBlob.data() + offset;
            tex.mipLevels.push_back(mip);
        }

        dir.push_back(std::move(tex));
    }

    return true;
}

bool TextureDirectory::import(const std::string& path)
{
    dir.clear();
    dataBlob.clear();

    std::ifstream in(path, std::ios::binary);
    if (!in)
    {
        printf("TextureDirectory::Import: failed to open %s\n", path.c_str());
        return false;
    }

    char magic[4];
    if (!in.read(magic, 4) || magic[0] != 'T' || magic[1] != 'X' ||
        magic[2] != 'T' || magic[3] != 'R')
    {
        printf("TextureDirectory::Import: bad magic in %s\n", path.c_str());
        return false;
    }

    uint32_t version = 0, textureCount = 0;
    if (!in.read(reinterpret_cast<char*>(&version), 4) ||
        !in.read(reinterpret_cast<char*>(&textureCount), 4))
    {
        printf("TextureDirectory::Import: truncated header in %s\n",
               path.c_str());
        return false;
    }

    uint32_t metaSize = 0;
    if (!in.read(reinterpret_cast<char*>(&metaSize), 4))
    {
        printf("TextureDirectory::Import: truncated metaSize in %s\n",
               path.c_str());
        return false;
    }

    std::vector<uint8_t> metaBuf(metaSize);
    if (metaSize > 0 && !in.read(reinterpret_cast<char*>(metaBuf.data()),
                                 static_cast<std::streamsize>(metaSize)))
    {
        printf("TextureDirectory::Import: truncated metadata in %s\n",
               path.c_str());
        return false;
    }

    uint32_t uncompressedSize = 0, compressedSize = 0;
    if (!in.read(reinterpret_cast<char*>(&uncompressedSize), 4) ||
        !in.read(reinterpret_cast<char*>(&compressedSize), 4))
    {
        printf("TextureDirectory::Import: truncated data sizes in %s\n",
               path.c_str());
        return false;
    }

    if (uncompressedSize > 0)
    {
        dataBlob.resize(uncompressedSize);
        if (compressedSize == uncompressedSize)
        {
            if (!in.read(reinterpret_cast<char*>(dataBlob.data()),
                         static_cast<std::streamsize>(uncompressedSize)))
            {
                printf("TextureDirectory::Import: truncated data blob in %s\n",
                       path.c_str());
                return false;
            }
        }
        else
        {
            printf(
                "TextureDirectory::Import: compressed blobs not yet supported\n");
            return false;
        }
    }

    // Parse metadata
    size_t mpos = 0;
    auto readMeta = [&](void* dst, size_t n) -> bool {
        if (mpos + n > metaBuf.size())
            return false;
        std::memcpy(dst, metaBuf.data() + mpos, n);
        mpos += n;
        return true;
    };

    for (uint32_t t = 0; t < textureCount; ++t)
    {
        TextureData tex = {};

        if (!readMeta(&tex.width, 2) || !readMeta(&tex.height, 2) ||
            !readMeta(&tex.paddedWidth, 2) || !readMeta(&tex.paddedHeight, 2))
            return false;

        if (!readMeta(&tex.blockSizeX, 1) || !readMeta(&tex.blockSizeY, 1) ||
            !readMeta(&tex.bytesPerBlock, 1) || !readMeta(&tex.numMips, 1))
            return false;

        uint32_t fmt = 0;
        if (!readMeta(&fmt, 4))
            return false;
        tex.format = static_cast<GPUTextureFormat>(fmt);

        uint32_t totalBytes = 0;
        if (!readMeta(&totalBytes, 4))
            return false;
        tex.totalBytes = static_cast<int>(totalBytes);

        uint32_t mipCount = 0;
        if (!readMeta(&mipCount, 4))
            return false;

        for (uint32_t m = 0; m < mipCount; ++m)
        {
            int32_t blocksX = 0, blocksY = 0, bytesTotal = 0;
            uint32_t offset = 0;
            if (!readMeta(&blocksX, 4) || !readMeta(&blocksY, 4) ||
                !readMeta(&bytesTotal, 4) || !readMeta(&offset, 4))
                return false;

            if (bytesTotal < 0 ||
                offset + static_cast<uint32_t>(bytesTotal) > dataBlob.size())
                return false;

            TextureMipLevel mip = {};
            mip.blocksX = blocksX;
            mip.blocksY = blocksY;
            mip.bytesTotal = bytesTotal;
            mip.blocks = dataBlob.data() + offset;
            tex.mipLevels.push_back(mip);
        }

        dir.push_back(std::move(tex));
    }

    printf("TextureDirectory::Import: read %u textures from %s (v%u)\n",
           textureCount,
           path.c_str(),
           version);
    return true;
}
