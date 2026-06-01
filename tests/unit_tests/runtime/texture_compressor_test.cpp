#include <catch.hpp>

#include "write_ktx2.hpp"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{
constexpr uint32_t VK_FORMAT_BC7_UNORM_BLOCK = 145;
constexpr uint32_t VK_FORMAT_BC7_SRGB_BLOCK = 146;

std::string tempPath(const char* suffix)
{
    static std::atomic<uint64_t> counter{0};
    auto dir = std::filesystem::temp_directory_path();
    auto name =
        "rive_ktx2_test_" + std::to_string(counter.fetch_add(1)) + suffix;
    return (dir / name).string();
}

std::vector<uint8_t> readFile(const std::string& path)
{
    std::ifstream in(path, std::ios::binary | std::ios::ate);
    REQUIRE(in.is_open());
    const auto size = static_cast<size_t>(in.tellg());
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> buf(size);
    in.read(reinterpret_cast<char*>(buf.data()), size);
    return buf;
}

uint32_t readU32(const std::vector<uint8_t>& buf, size_t offset)
{
    uint32_t v;
    std::memcpy(&v, buf.data() + offset, 4);
    return v;
}

uint64_t readU64(const std::vector<uint8_t>& buf, size_t offset)
{
    uint64_t v;
    std::memcpy(&v, buf.data() + offset, 8);
    return v;
}

Ktx2Mip dummyMip(uint32_t w, uint32_t h)
{
    Ktx2Mip mip;
    mip.pixelWidth = w;
    mip.pixelHeight = h;
    const uint32_t blocksX = (w + 3) / 4;
    const uint32_t blocksY = (h + 3) / 4;
    mip.blocks.assign(blocksX * blocksY * 16, 0xAB);
    return mip;
}
} // namespace

TEST_CASE("WriteKtx2Bc7 writes KTX2 identifier", "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    std::vector<Ktx2Mip> mips{dummyMip(4, 4)};
    REQUIRE(WriteKtx2Bc7(path.c_str(), mips, ColorMode::sRGB));

    auto buf = readFile(path);
    REQUIRE(buf.size() >= 12);
    const uint8_t expected[12] = {0xAB,
                                  0x4B,
                                  0x54,
                                  0x58,
                                  0x20,
                                  0x32,
                                  0x30,
                                  0xBB,
                                  0x0D,
                                  0x0A,
                                  0x1A,
                                  0x0A};
    CHECK(std::memcmp(buf.data(), expected, 12) == 0);
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 sRGB selects BC7_SRGB_BLOCK", "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    std::vector<Ktx2Mip> mips{dummyMip(8, 8)};
    REQUIRE(WriteKtx2Bc7(path.c_str(), mips, ColorMode::sRGB));

    auto buf = readFile(path);
    // vkFormat is the first uint32 after the 12-byte identifier.
    CHECK(readU32(buf, 12) == VK_FORMAT_BC7_SRGB_BLOCK);
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 linear selects BC7_UNORM_BLOCK", "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    std::vector<Ktx2Mip> mips{dummyMip(8, 8)};
    REQUIRE(WriteKtx2Bc7(path.c_str(), mips, ColorMode::linear));

    auto buf = readFile(path);
    CHECK(readU32(buf, 12) == VK_FORMAT_BC7_UNORM_BLOCK);
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 header records logical mip-0 dimensions",
          "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    // Logical 5x3 — encoder padded to 8x4 blocks externally; header still
    // records the un-padded extents.
    Ktx2Mip mip;
    mip.pixelWidth = 5;
    mip.pixelHeight = 3;
    mip.blocks.assign(2 * 1 * 16, 0); // (5+3)/4 * (3+3)/4 = 2*1 blocks
    REQUIRE(WriteKtx2Bc7(path.c_str(), {mip}, ColorMode::sRGB));

    auto buf = readFile(path);
    // Header byte offsets after identifier (12) + vkFormat(4) + typeSize(4):
    //   pixelWidth = 12 + 8 = 20
    //   pixelHeight = 24
    CHECK(readU32(buf, 20) == 5);
    CHECK(readU32(buf, 24) == 3);
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 levelCount matches mip count", "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    std::vector<Ktx2Mip> mips{
        dummyMip(8, 8),
        dummyMip(4, 4),
        dummyMip(2, 2),
        dummyMip(1, 1),
    };
    REQUIRE(WriteKtx2Bc7(path.c_str(), mips, ColorMode::sRGB));

    auto buf = readFile(path);
    // levelCount sits at byte 12 + 28 = 40.
    CHECK(readU32(buf, 40) == 4);
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 level data offsets are 16-byte aligned",
          "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    std::vector<Ktx2Mip> mips{
        dummyMip(8, 8),
        dummyMip(4, 4),
        dummyMip(2, 2),
        dummyMip(1, 1),
    };
    REQUIRE(WriteKtx2Bc7(path.c_str(), mips, ColorMode::sRGB));

    auto buf = readFile(path);
    // Level index begins at offset 12 (ident) + 68 (header) = 80. Each entry
    // is 24 bytes: byteOffset (u64), byteLength (u64), uncompressedByteLength
    // (u64). Spec writes them in level-0..N order.
    constexpr size_t kLevelIndex = 80;
    constexpr size_t kEntryBytes = 24;
    for (size_t i = 0; i < mips.size(); ++i)
    {
        uint64_t byteOffset = readU64(buf, kLevelIndex + i * kEntryBytes);
        CHECK((byteOffset & 0xF) == 0); // 16-byte aligned (mipPadding for BC7)
        uint64_t byteLength = readU64(buf, kLevelIndex + i * kEntryBytes + 8);
        CHECK(byteLength == mips[i].blocks.size());
    }
    std::remove(path.c_str());
}

TEST_CASE("WriteKtx2Bc7 rejects empty mip list", "[texture_compressor]")
{
    const auto path = tempPath(".ktx2");
    CHECK_FALSE(WriteKtx2Bc7(path.c_str(), {}, ColorMode::sRGB));
    std::remove(path.c_str());
}
