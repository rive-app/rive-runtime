#include <catch.hpp>
#include "rive/texture_archive.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace rive;

// ---------------------------------------------------------------------------
// Helper: build a valid RTEX byte stream in memory.
//
// File layout (matches packages/runtime/src/texture_archive.cpp):
//   [4] magic 'RTEX'
//   [2] version  (u16)
//   [2] textureCount (u16)
//   per texture:
//     [2]x4 width, height, paddedWidth, paddedHeight (u16)
//     [1]x4 blockSizeX, blockSizeY, bytesPerBlock, numMips (u8)
//     [1]   format (u8 GPUTextureFormat)
//     per mip (numMips entries):
//       [4] blocksX, blocksY, bytesTotal (i32 each)
//   [blob] raw mip data, size == sum(bytesTotal) over all mips
// ---------------------------------------------------------------------------

namespace
{

struct MipDesc
{
    int32_t blocksX;
    int32_t blocksY;
    int32_t bytesTotal;
};

struct TexDesc
{
    uint16_t width = 4, height = 4, paddedWidth = 4, paddedHeight = 4;
    uint8_t blockSizeX = 4, blockSizeY = 4, bytesPerBlock = 16, numMips = 1;
    GPUTextureFormat format = GPUTextureFormat::bc7;
    std::vector<MipDesc> mips;
};

template <typename T> static void append(std::vector<uint8_t>& buf, T val)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

static std::vector<uint8_t> buildArchive(uint16_t version,
                                         const std::vector<TexDesc>& textures,
                                         const std::vector<uint8_t>& dataBlob)
{
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'R', 'T', 'E', 'X'});
    append(buf, version);
    append(buf, static_cast<uint16_t>(textures.size()));

    for (const auto& t : textures)
    {
        append(buf, t.width);
        append(buf, t.height);
        append(buf, t.paddedWidth);
        append(buf, t.paddedHeight);
        append(buf, t.blockSizeX);
        append(buf, t.blockSizeY);
        append(buf, t.bytesPerBlock);
        append(buf, t.numMips);
        append(buf, static_cast<uint8_t>(t.format));
        for (const auto& m : t.mips)
        {
            append(buf, m.blocksX);
            append(buf, m.blocksY);
            append(buf, m.bytesTotal);
        }
    }

    buf.insert(buf.end(), dataBlob.begin(), dataBlob.end());
    return buf;
}

// Build a minimal valid single-texture archive with one 4x4 BC7 mip.
static std::vector<uint8_t> makeSimpleArchive()
{
    std::vector<uint8_t> blob(16, 0xAB);

    TexDesc tex;
    tex.mips = {{1, 1, 16}};

    return buildArchive(1, {tex}, blob);
}

static std::string writeTempArchive(const std::vector<uint8_t>& bytes)
{
    static int counter = 0;
    std::string path = (std::filesystem::temp_directory_path() /
                        ("rive_ta_test_" + std::to_string(counter++) + ".rtex"))
                           .string();
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(bytes.data()),
              static_cast<std::streamsize>(bytes.size()));
    out.close();
    return path;
}

} // namespace

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_CASE("TextureDirectory import: valid single BC7 texture",
          "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    REQUIRE(dir.dir.size() == 1);
    const TextureData& tex = dir.dir[0];
    CHECK(tex.width == 4);
    CHECK(tex.height == 4);
    CHECK(tex.paddedWidth == 4);
    CHECK(tex.paddedHeight == 4);
    CHECK(tex.blockSizeX == 4);
    CHECK(tex.blockSizeY == 4);
    CHECK(tex.bytesPerBlock == 16);
    CHECK(tex.numMips == 1);
    CHECK(tex.format == GPUTextureFormat::bc7);
    CHECK(tex.totalBytes == 16);

    REQUIRE(tex.mipLevels.size() == 1);
    CHECK(tex.mipLevels[0].blocksX == 1);
    CHECK(tex.mipLevels[0].blocksY == 1);
    CHECK(tex.mipLevels[0].bytesTotal == 16);
}

TEST_CASE("TextureDirectory import: mip blocks pointer into dataBlob",
          "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    const TextureMipLevel& mip = dir.dir[0].mipLevels[0];
    REQUIRE(mip.blocks != nullptr);
    const uint8_t* blobBegin = dir.dataBlob.data();
    const uint8_t* blobEnd = blobBegin + dir.dataBlob.size();
    CHECK(mip.blocks >= blobBegin);
    CHECK(mip.blocks + mip.bytesTotal <= blobEnd);
    CHECK(mip.blocks[0] == 0xAB);
}

TEST_CASE("TextureDirectory import: zero textures", "[texture_archive]")
{
    auto bytes = buildArchive(1, {}, {});
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
    CHECK(dir.dir.empty());
    CHECK(dir.dataBlob.empty());
}

TEST_CASE("TextureDirectory import: multiple textures", "[texture_archive]")
{
    // tex0 + tex1 each contribute 16 bytes of contiguous mip data.
    std::vector<uint8_t> blob(32);
    std::fill(blob.begin(), blob.begin() + 16, 0x11);
    std::fill(blob.begin() + 16, blob.end(), 0x22);

    TexDesc tex0, tex1;
    tex0.mips = {{1, 1, 16}};
    tex1.mips = {{1, 1, 16}};

    auto bytes = buildArchive(1, {tex0, tex1}, blob);
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    REQUIRE(dir.dir.size() == 2);
    CHECK(dir.dir[0].mipLevels[0].blocks[0] == 0x11);
    CHECK(dir.dir[1].mipLevels[0].blocks[0] == 0x22);
}

TEST_CASE("TextureDirectory import: multiple mip levels", "[texture_archive]")
{
    // Mip 0: 2x2 blocks = 64 bytes. Mip 1: 1x1 block = 16 bytes.
    std::vector<uint8_t> blob(80);
    std::fill(blob.begin(), blob.begin() + 64, 0xAA);
    std::fill(blob.begin() + 64, blob.end(), 0xBB);

    TexDesc tex;
    tex.width = 8;
    tex.height = 8;
    tex.paddedWidth = 8;
    tex.paddedHeight = 8;
    tex.numMips = 2;
    tex.mips = {{2, 2, 64}, {1, 1, 16}};

    auto bytes = buildArchive(1, {tex}, blob);
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    REQUIRE(dir.dir[0].mipLevels.size() == 2);
    CHECK(dir.dir[0].mipLevels[0].bytesTotal == 64);
    CHECK(dir.dir[0].mipLevels[0].blocks[0] == 0xAA);
    CHECK(dir.dir[0].mipLevels[1].bytesTotal == 16);
    CHECK(dir.dir[0].mipLevels[1].blocks[0] == 0xBB);
}

TEST_CASE("TextureDirectory import: clears previous state on re-import",
          "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
    REQUIRE(dir.dir.size() == 1);

    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
    CHECK(dir.dir.size() == 1);
}

TEST_CASE("TextureDirectory import: bad magic", "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    bytes[0] = 'X';
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: empty span", "[texture_archive]")
{
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(nullptr, 0)));
}

TEST_CASE("TextureDirectory import: too small for header", "[texture_archive]")
{
    std::vector<uint8_t> bytes = {'R', 'T', 'E', 'X'}; // header needs 8 bytes
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: unsupported version", "[texture_archive]")
{
    auto bytes = buildArchive(99, {}, {}); // bogus version
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: truncated descriptor", "[texture_archive]")
{
    // Claim 1 texture but provide no descriptor bytes.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'R', 'T', 'E', 'X'});
    append(buf, uint16_t(1));
    append(buf, uint16_t(1));
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated mip table", "[texture_archive]")
{
    // Descriptor present but mip entries truncated.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'R', 'T', 'E', 'X'});
    append(buf, uint16_t(1));
    append(buf, uint16_t(1));
    append(buf, uint16_t(4)); // width
    append(buf, uint16_t(4)); // height
    append(buf, uint16_t(4)); // paddedWidth
    append(buf, uint16_t(4)); // paddedHeight
    append(buf, uint8_t(4));  // blockSizeX
    append(buf, uint8_t(4));  // blockSizeY
    append(buf, uint8_t(16)); // bytesPerBlock
    append(buf, uint8_t(1));  // numMips = 1
    append(buf, uint8_t(static_cast<uint8_t>(GPUTextureFormat::bc7)));
    // No mip entry follows.
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: blob size mismatch", "[texture_archive]")
{
    // Mip table claims 16 bytes but blob has only 8.
    std::vector<uint8_t> blob(8, 0);
    TexDesc tex;
    tex.mips = {{1, 1, 16}};

    auto bytes = buildArchive(1, {tex}, blob);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: blob too large", "[texture_archive]")
{
    // Mip table claims 16 bytes but blob has 32 — strict size check rejects.
    std::vector<uint8_t> blob(32, 0);
    TexDesc tex;
    tex.mips = {{1, 1, 16}};

    auto bytes = buildArchive(1, {tex}, blob);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: ASTC format preserved", "[texture_archive]")
{
    std::vector<uint8_t> blob(16, 0xCC);

    TexDesc tex;
    tex.format = GPUTextureFormat::astc;
    tex.blockSizeX = 6;
    tex.blockSizeY = 6;
    tex.bytesPerBlock = 16; // ASTC always 16 bytes per block
    tex.mips = {{1, 1, 16}};

    auto bytes = buildArchive(1, {tex}, blob);
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    CHECK(dir.dir[0].format == GPUTextureFormat::astc);
    CHECK(dir.dir[0].blockSizeX == 6);
    CHECK(dir.dir[0].blockSizeY == 6);
    CHECK(dir.dir[0].bytesPerBlock == 16);
}

TEST_CASE("TextureDirectory import: ETC2 format preserved", "[texture_archive]")
{
    std::vector<uint8_t> blob(8, 0xDD);

    TexDesc tex;
    tex.format = GPUTextureFormat::etc2;
    tex.blockSizeX = 4;
    tex.blockSizeY = 4;
    tex.bytesPerBlock = 8; // ETC2 RGB8: 8 bytes per block
    tex.mips = {{1, 1, 8}};

    auto bytes = buildArchive(1, {tex}, blob);
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    CHECK(dir.dir[0].format == GPUTextureFormat::etc2);
    CHECK(dir.dir[0].bytesPerBlock == 8);
}

TEST_CASE("TextureDirectory addTexture: packs mip data into dataBlob",
          "[texture_archive]")
{
    std::vector<uint8_t> pixelData(16, 0xAB);

    TextureData td;
    td.width = td.height = td.paddedWidth = td.paddedHeight = 4;
    td.blockSizeX = td.blockSizeY = 4;
    td.bytesPerBlock = 16;
    td.numMips = 1;
    td.format = GPUTextureFormat::bc7;
    td.totalBytes = 16;

    TextureMipLevel mip;
    mip.blocksX = mip.blocksY = 1;
    mip.bytesTotal = 16;
    mip.blocks = pixelData.data();
    td.mipLevels.push_back(mip);

    TextureDirectory archive;
    archive.addTexture(td);

    REQUIRE(archive.dataBlob.size() == 16);
    CHECK(archive.dataBlob[0] == 0xAB);

    REQUIRE(archive.dir.size() == 1);

    const uint8_t* blobBegin = archive.dataBlob.data();
    const uint8_t* blobEnd = blobBegin + archive.dataBlob.size();
    const TextureMipLevel& stored = archive.dir[0].mipLevels[0];
    CHECK(stored.blocks >= blobBegin);
    CHECK(stored.blocks + stored.bytesTotal <= blobEnd);
    CHECK(stored.blocks[0] == 0xAB);
}

TEST_CASE("TextureDirectory addTexture: two textures share contiguous dataBlob",
          "[texture_archive]")
{
    auto makeTd = [](uint8_t fill, GPUTextureFormat fmt) {
        std::vector<uint8_t> pixels(16, fill);
        TextureData td;
        td.width = td.height = td.paddedWidth = td.paddedHeight = 4;
        td.blockSizeX = td.blockSizeY = 4;
        td.bytesPerBlock = 16;
        td.numMips = 1;
        td.format = fmt;
        td.totalBytes = 16;
        TextureMipLevel mip;
        mip.blocksX = mip.blocksY = 1;
        mip.bytesTotal = 16;
        mip.blocks = pixels.data();
        td.mipLevels.push_back(mip);
        return std::make_pair(td, pixels);
    };

    auto [td0, px0] = makeTd(0x11, GPUTextureFormat::bc7);
    td0.mipLevels[0].blocks = px0.data();
    auto [td1, px1] = makeTd(0x22, GPUTextureFormat::astc);
    td1.mipLevels[0].blocks = px1.data();

    TextureDirectory archive;
    archive.addTexture(td0);
    archive.addTexture(td1);

    REQUIRE(archive.dataBlob.size() == 32);
    REQUIRE(archive.dir.size() == 2);

    CHECK(archive.dir[0].mipLevels[0].blocks[0] == 0x11);
    CHECK(archive.dir[1].mipLevels[0].blocks[0] == 0x22);
    CHECK(archive.dir[1].mipLevels[0].blocks ==
          archive.dir[0].mipLevels[0].blocks + 16);
}

TEST_CASE("TextureDirectory exportArchive: round-trip preserves data",
          "[texture_archive]")
{
    std::vector<uint8_t> pixelData(16, 0x5A);

    TextureData td;
    td.width = td.height = td.paddedWidth = td.paddedHeight = 4;
    td.blockSizeX = td.blockSizeY = 4;
    td.bytesPerBlock = 16;
    td.numMips = 1;
    td.format = GPUTextureFormat::bc7;
    td.totalBytes = 16;

    TextureMipLevel mip;
    mip.blocksX = mip.blocksY = 1;
    mip.bytesTotal = 16;
    mip.blocks = pixelData.data();
    td.mipLevels.push_back(mip);

    TextureDirectory writer;
    writer.addTexture(td);

    auto path =
        (std::filesystem::temp_directory_path() / "rive_ta_roundtrip.rtex")
            .string();
    REQUIRE(writer.exportArchive(path));

    TextureDirectory reader;
    REQUIRE(reader.import(path));
    REQUIRE(reader.dir.size() == 1);
    CHECK(reader.dir[0].format == GPUTextureFormat::bc7);
    CHECK(reader.dir[0].width == 4);
    CHECK(reader.dir[0].mipLevels[0].bytesTotal == 16);
    CHECK(reader.dir[0].mipLevels[0].blocks[0] == 0x5A);

    std::remove(path.c_str());
}

TEST_CASE("TextureDirectory import (file): valid archive", "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    auto path = writeTempArchive(bytes);
    TextureDirectory dir;
    REQUIRE(dir.import(path));
    REQUIRE(dir.dir.size() == 1);
    CHECK(dir.dir[0].width == 4);
    CHECK(dir.dir[0].mipLevels[0].blocks[0] == 0xAB);
    std::remove(path.c_str());
}

TEST_CASE("TextureDirectory import (file): file not found", "[texture_archive]")
{
    TextureDirectory dir;
    CHECK_FALSE(dir.import((std::filesystem::temp_directory_path() /
                            "rive_does_not_exist_xyz.rtex")
                               .string()));
}

TEST_CASE("TextureDirectory import (file): bad magic", "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    bytes[0] = 'X';
    auto path = writeTempArchive(bytes);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(path));
    std::remove(path.c_str());
}

TEST_CASE("TextureDirectory import (file): too small for header",
          "[texture_archive]")
{
    std::vector<uint8_t> bytes = {'R', 'T', 'E', 'X'};
    auto path = writeTempArchive(bytes);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(path));
    std::remove(path.c_str());
}
