#include <catch.hpp>
#include "rive/texture_archive.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

using namespace rive;

// ---------------------------------------------------------------------------
// Helper: build a valid TXTR byte stream in memory.
//
// File layout:
//   [4]  magic          'TXTR'
//   [4]  version        (u32 LE)
//   [4]  textureCount   (u32 LE)
//   [4]  metaSize       (u32 LE)
//   [N]  metaBuf        per-texture metadata
//   [4]  uncompressedSize (u32 LE)
//   [4]  compressedSize   (u32 LE)   equal to uncompressedSize = uncompressed
//   [M]  dataBlob
//
// Per-texture metadata:
//   [2]  width, height, paddedWidth, paddedHeight  (u16 LE each)
//   [1]  blockSizeX, blockSizeY, bytesPerBlock, numMips (u8 each)
//   [4]  format     (u32 LE)
//   [4]  totalBytes (u32 LE)
//   [4]  mipCount   (u32 LE)
//   per mip:
//     [4] blocksX, blocksY, bytesTotal (i32 LE each)
//     [4] offset (u32 LE)
// ---------------------------------------------------------------------------

namespace
{

struct MipDesc
{
    int32_t blocksX;
    int32_t blocksY;
    int32_t bytesTotal;
    uint32_t offset;
};

struct TexDesc
{
    uint16_t width = 4, height = 4, paddedWidth = 4, paddedHeight = 4;
    uint8_t blockSizeX = 4, blockSizeY = 4, bytesPerBlock = 16, numMips = 1;
    GPUTextureFormat format = GPUTextureFormat::bc7;
    uint32_t totalBytes = 16;
    std::vector<MipDesc> mips;
};

template <typename T> static void append(std::vector<uint8_t>& buf, T val)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&val);
    buf.insert(buf.end(), p, p + sizeof(T));
}

static std::vector<uint8_t> buildArchive(uint32_t version,
                                         const std::vector<TexDesc>& textures,
                                         const std::vector<uint8_t>& dataBlob)
{
    // Build metadata buffer first so we know its size.
    std::vector<uint8_t> meta;
    for (const auto& t : textures)
    {
        append(meta, t.width);
        append(meta, t.height);
        append(meta, t.paddedWidth);
        append(meta, t.paddedHeight);
        append(meta, t.blockSizeX);
        append(meta, t.blockSizeY);
        append(meta, t.bytesPerBlock);
        append(meta, t.numMips);
        append(meta, static_cast<uint32_t>(t.format));
        append(meta, t.totalBytes);
        append(meta, static_cast<uint32_t>(t.mips.size()));
        for (const auto& m : t.mips)
        {
            append(meta, m.blocksX);
            append(meta, m.blocksY);
            append(meta, m.bytesTotal);
            append(meta, m.offset);
        }
    }

    std::vector<uint8_t> buf;
    // Magic
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, version);
    append(buf, static_cast<uint32_t>(textures.size()));
    append(buf, static_cast<uint32_t>(meta.size()));
    buf.insert(buf.end(), meta.begin(), meta.end());
    // Data blob (uncompressed: compressedSize == uncompressedSize)
    append(buf, static_cast<uint32_t>(dataBlob.size()));
    append(buf, static_cast<uint32_t>(dataBlob.size()));
    buf.insert(buf.end(), dataBlob.begin(), dataBlob.end());
    return buf;
}

// Build a minimal valid single-texture archive with one 4x4 BC7 mip.
static std::vector<uint8_t> makeSimpleArchive()
{
    // 4x4 BC7 = 1 block = 16 bytes
    std::vector<uint8_t> blob(16, 0xAB);

    TexDesc tex;
    tex.mips = {{1, 1, 16, 0}};

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
    // Pointer must sit within the owned dataBlob.
    const uint8_t* blobBegin = dir.dataBlob.data();
    const uint8_t* blobEnd = blobBegin + dir.dataBlob.size();
    CHECK(mip.blocks >= blobBegin);
    CHECK(mip.blocks + mip.bytesTotal <= blobEnd);
    // Data content should match what we put in.
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
    // Two textures sharing the dataBlob: tex0 at offset 0 (16 bytes),
    // tex1 at offset 16 (16 bytes).
    std::vector<uint8_t> blob(32);
    std::fill(blob.begin(), blob.begin() + 16, 0x11);
    std::fill(blob.begin() + 16, blob.end(), 0x22);

    TexDesc tex0, tex1;
    tex0.mips = {{1, 1, 16, 0}};
    tex1.width = tex1.height = tex1.paddedWidth = tex1.paddedHeight = 4;
    tex1.mips = {{1, 1, 16, 16}};

    auto bytes = buildArchive(1, {tex0, tex1}, blob);
    TextureDirectory dir;
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));

    REQUIRE(dir.dir.size() == 2);
    CHECK(dir.dir[0].mipLevels[0].blocks[0] == 0x11);
    CHECK(dir.dir[1].mipLevels[0].blocks[0] == 0x22);
}

TEST_CASE("TextureDirectory import: multiple mip levels", "[texture_archive]")
{
    // Mip 0: 2x2 blocks = 4 * 16 = 64 bytes at offset 0
    // Mip 1: 1x1 block  = 1 * 16 = 16 bytes at offset 64
    std::vector<uint8_t> blob(80);
    std::fill(blob.begin(), blob.begin() + 64, 0xAA);
    std::fill(blob.begin() + 64, blob.end(), 0xBB);

    TexDesc tex;
    tex.width = 8;
    tex.height = 8;
    tex.paddedWidth = 8;
    tex.paddedHeight = 8;
    tex.numMips = 2;
    tex.totalBytes = 80;
    tex.mips = {{2, 2, 64, 0}, {1, 1, 16, 64}};

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

    // Import again into same object — should replace, not accumulate.
    REQUIRE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
    CHECK(dir.dir.size() == 1);
}

TEST_CASE("TextureDirectory import: bad magic", "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    bytes[0] = 'X'; // corrupt magic
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: empty span", "[texture_archive]")
{
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(nullptr, 0)));
}

TEST_CASE("TextureDirectory import: truncated at magic", "[texture_archive]")
{
    std::vector<uint8_t> bytes = {'T', 'X', 'T'}; // only 3 bytes
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: truncated after magic", "[texture_archive]")
{
    // Magic present but nothing else
    std::vector<uint8_t> bytes = {'T', 'X', 'T', 'R'};
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: truncated metadata", "[texture_archive]")
{
    auto bytes = makeSimpleArchive();
    // Trim the last few bytes to corrupt the metadata section.
    bytes.resize(bytes.size() - 4);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(bytes.data(), bytes.size())));
}

TEST_CASE("TextureDirectory import: mip offset out of bounds",
          "[texture_archive]")
{
    std::vector<uint8_t> blob(16, 0);

    TexDesc tex;
    // Offset 8 + bytesTotal 16 = 24 > blob size 16 — invalid.
    tex.mips = {{1, 1, 16, 8}};

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
    tex.bytesPerBlock = 16; // ASTC is always 128-bits per block
    tex.mips = {{1, 1, 16, 0}};

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
    tex.bytesPerBlock = 8; // ETC2 RGB8 — 8 bytes per block
    tex.totalBytes = 8;
    tex.mips = {{1, 1, 8, 0}};

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

    // Blob must contain the copied pixel data.
    REQUIRE(archive.dataBlob.size() == 16);
    CHECK(archive.dataBlob[0] == 0xAB);

    // dir must have the texture.
    REQUIRE(archive.dir.size() == 1);

    // mip.blocks must point into the owned dataBlob, not the original buffer.
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

    // First texture data at the start of the blob.
    CHECK(archive.dir[0].mipLevels[0].blocks[0] == 0x11);
    // Second texture data immediately after.
    CHECK(archive.dir[1].mipLevels[0].blocks[0] == 0x22);
    // They must not overlap.
    CHECK(archive.dir[1].mipLevels[0].blocks ==
          archive.dir[0].mipLevels[0].blocks + 16);
}

TEST_CASE("TextureDirectory import: truncated before metaSize",
          "[texture_archive]")
{
    // Magic + version + textureCount only — stops before metaSize field.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1)); // version
    append(buf, uint32_t(0)); // textureCount
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: metaSize present but bytes missing",
          "[texture_archive]")
{
    // metaSize claims 32 bytes but none follow.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1));  // version
    append(buf, uint32_t(1));  // textureCount
    append(buf, uint32_t(32)); // metaSize = 32, no bytes follow
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated before data sizes",
          "[texture_archive]")
{
    // Full header with zero metaSize but missing uncompressed/compressed
    // fields.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1)); // version
    append(buf, uint32_t(0)); // textureCount
    append(buf, uint32_t(0)); // metaSize = 0
    // No data size fields.
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated texture dims in meta",
          "[texture_archive]")
{
    // textureCount=1 but metaSize only holds 4 bytes — not enough for dims (8).
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1)); // version
    append(buf, uint32_t(1)); // textureCount
    append(buf, uint32_t(4)); // metaSize = 4
    append(buf, uint32_t(0)); // 4 bytes of meta (too small for dims)
    append(buf, uint32_t(0)); // uncompressedSize
    append(buf, uint32_t(0)); // compressedSize
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated block info in meta",
          "[texture_archive]")
{
    // metaSize holds dims (8 bytes) but not block info (needs 4 more).
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1)); // version
    append(buf, uint32_t(1)); // textureCount
    append(buf, uint32_t(8)); // metaSize = 8 (dims only)
    // 8 bytes of meta: width, height, paddedWidth, paddedHeight (u16 each)
    append(buf, uint16_t(4));
    append(buf, uint16_t(4));
    append(buf, uint16_t(4));
    append(buf, uint16_t(4));
    append(buf, uint32_t(0)); // uncompressedSize
    append(buf, uint32_t(0)); // compressedSize
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated mipCount in meta",
          "[texture_archive]")
{
    // Meta has dims + block info + format + totalBytes but no mipCount.
    // That's 8+4+4+4 = 20 bytes; we supply 20 exactly.
    std::vector<uint8_t> meta;
    append(meta, uint16_t(4)); // width
    append(meta, uint16_t(4)); // height
    append(meta, uint16_t(4)); // paddedWidth
    append(meta, uint16_t(4)); // paddedHeight
    append(meta, uint8_t(4));  // blockSizeX
    append(meta, uint8_t(4));  // blockSizeY
    append(meta, uint8_t(16)); // bytesPerBlock
    append(meta, uint8_t(1));  // numMips
    append(meta, uint32_t(static_cast<uint32_t>(GPUTextureFormat::bc7))); // fmt
    append(meta, uint32_t(16)); // totalBytes
    // No mipCount field.

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1));                        // version
    append(buf, uint32_t(1));                        // textureCount
    append(buf, static_cast<uint32_t>(meta.size())); // metaSize
    buf.insert(buf.end(), meta.begin(), meta.end());
    append(buf, uint32_t(0)); // uncompressedSize
    append(buf, uint32_t(0)); // compressedSize
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}

TEST_CASE("TextureDirectory import: truncated mip entry in meta",
          "[texture_archive]")
{
    // Meta has full texture header (mipCount=1) but no mip entry bytes.
    std::vector<uint8_t> meta;
    append(meta, uint16_t(4));
    append(meta, uint16_t(4));
    append(meta, uint16_t(4));
    append(meta, uint16_t(4));
    append(meta, uint8_t(4));
    append(meta, uint8_t(4));
    append(meta, uint8_t(16));
    append(meta, uint8_t(1));
    append(meta, uint32_t(static_cast<uint32_t>(GPUTextureFormat::bc7)));
    append(meta, uint32_t(16)); // totalBytes
    append(meta, uint32_t(1));  // mipCount = 1 — but no mip entry follows

    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1));
    append(buf, uint32_t(1));
    append(buf, static_cast<uint32_t>(meta.size()));
    buf.insert(buf.end(), meta.begin(), meta.end());
    append(buf, uint32_t(0));
    append(buf, uint32_t(0));
    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
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

TEST_CASE("TextureDirectory import (file): truncated after magic",
          "[texture_archive]")
{
    std::vector<uint8_t> bytes = {'T', 'X', 'T', 'R'};
    auto path = writeTempArchive(bytes);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(path));
    std::remove(path.c_str());
}

TEST_CASE("TextureDirectory import (file): compressed blob rejected",
          "[texture_archive]")
{
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1));  // version
    append(buf, uint32_t(0));  // textureCount
    append(buf, uint32_t(0));  // metaSize
    append(buf, uint32_t(16)); // uncompressedSize
    append(buf, uint32_t(8));  // compressedSize != uncompressedSize
    buf.resize(buf.size() + 8, 0);
    auto path = writeTempArchive(buf);
    TextureDirectory dir;
    CHECK_FALSE(dir.import(path));
    std::remove(path.c_str());
}

TEST_CASE("TextureDirectory import: compressed blob rejected",
          "[texture_archive]")
{
    // Build an archive where compressedSize != uncompressedSize.
    std::vector<uint8_t> blob(16, 0);
    TexDesc tex;
    tex.mips = {{1, 1, 16, 0}};

    // Build normally, then patch compressedSize to differ.
    auto bytes = buildArchive(1, {tex}, blob);

    // Find the uncompressedSize field (comes right after the metadata).
    // Easiest: rebuild manually with mismatched sizes.
    std::vector<uint8_t> buf;
    buf.insert(buf.end(), {'T', 'X', 'T', 'R'});
    append(buf, uint32_t(1));  // version
    append(buf, uint32_t(0));  // textureCount = 0 (skip meta for simplicity)
    append(buf, uint32_t(0));  // metaSize = 0
    append(buf, uint32_t(16)); // uncompressedSize
    append(buf, uint32_t(8));  // compressedSize != uncompressedSize
    buf.resize(buf.size() + 8, 0);

    TextureDirectory dir;
    CHECK_FALSE(dir.import(Span<const uint8_t>(buf.data(), buf.size())));
}
