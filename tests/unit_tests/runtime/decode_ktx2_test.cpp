#include "rive_testing.hpp"
#include "rive/decoders/decode_ktx2.hpp"

#include <cstdint>
#include <cstring>
#include <vector>

// Helpers for building a synthetic KTX2 byte stream so the tests can exercise
// each rejection path of the parser without committing a binary fixture.
//
// Layout:
//   [12]            identifier  «KTX 20»\r\n\x1A\n
//   [68]            header
//   [24*levelCount] level index
//   [DFD]           data format descriptor (variable, often empty here)
//   ...mip data
namespace
{
constexpr uint8_t kKtx2Identifier[12] = {
    0xAB,
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
    0x0A,
};
constexpr uint32_t VK_FORMAT_BC7_SRGB_BLOCK = 146;

template <typename T> void appendLE(std::vector<uint8_t>& buf, T value)
{
    const uint8_t* p = reinterpret_cast<const uint8_t*>(&value);
    buf.insert(buf.end(), p, p + sizeof(T));
}

std::vector<uint8_t> buildSkeletonKtx2(uint32_t vkFormat,
                                       uint32_t pixelWidth,
                                       uint32_t pixelHeight,
                                       uint32_t levelCount,
                                       uint32_t supercompressionScheme,
                                       uint32_t faceCount,
                                       uint32_t layerCount)
{
    std::vector<uint8_t> buf;
    buf.insert(buf.end(),
               kKtx2Identifier,
               kKtx2Identifier + sizeof(kKtx2Identifier));

    appendLE<uint32_t>(buf, vkFormat);
    appendLE<uint32_t>(buf, 1); // typeSize
    appendLE<uint32_t>(buf, pixelWidth);
    appendLE<uint32_t>(buf, pixelHeight);
    appendLE<uint32_t>(buf, 0); // pixelDepth
    appendLE<uint32_t>(buf, layerCount);
    appendLE<uint32_t>(buf, faceCount);
    appendLE<uint32_t>(buf, levelCount);
    appendLE<uint32_t>(buf, supercompressionScheme);
    appendLE<uint32_t>(buf, 0); // dfdByteOffset
    appendLE<uint32_t>(buf, 0); // dfdByteLength
    appendLE<uint32_t>(buf, 0); // kvdByteOffset
    appendLE<uint32_t>(buf, 0); // kvdByteLength
    appendLE<uint64_t>(buf, 0); // sgdByteOffset
    appendLE<uint64_t>(buf, 0); // sgdByteLength
    return buf;
}
} // namespace

TEST_CASE("ktx2 rejects buffer smaller than identifier+header",
          "[ktx2-decoder]")
{
    std::vector<uint8_t> buf(40, 0);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects bad magic", "[ktx2-decoder]")
{
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 4, 4, 1, 0, 1, 0);
    buf[0] = 'X';
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects unsupported vkFormat", "[ktx2-decoder]")
{
    // VK_FORMAT_R8G8B8A8_UNORM = 37 — not BC7.
    auto buf = buildSkeletonKtx2(/*vkFormat*/ 37, 4, 4, 1, 0, 1, 0);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects supercompressed payload (not yet supported)",
          "[ktx2-decoder]")
{
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK,
                                 4,
                                 4,
                                 1,
                                 /*supercompressionScheme*/ 2 /* zstd */,
                                 1,
                                 0);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects cubemaps and array layers", "[ktx2-decoder]")
{
    {
        auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK,
                                     4,
                                     4,
                                     1,
                                     0,
                                     /*faceCount*/ 6,
                                     0);
        rive::Ktx2DecodeResult out;
        REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    }
    {
        auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK,
                                     4,
                                     4,
                                     1,
                                     0,
                                     1,
                                     /*layerCount*/ 4);
        rive::Ktx2DecodeResult out;
        REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    }
}

TEST_CASE("ktx2 rejects out-of-range dimensions", "[ktx2-decoder]")
{
    {
        auto buf =
            buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 0, 4, 1, 0, 1, 0);
        rive::Ktx2DecodeResult out;
        REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    }
    {
        auto buf =
            buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 4, 999999, 1, 0, 1, 0);
        rive::Ktx2DecodeResult out;
        REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    }
}

TEST_CASE("ktx2 rejects truncated level index", "[ktx2-decoder]")
{
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK,
                                 4,
                                 4,
                                 /*levelCount*/ 1,
                                 0,
                                 1,
                                 0);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects level pointer outside buffer", "[ktx2-decoder]")
{
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 4, 4, 1, 0, 1, 0);
    appendLE<uint64_t>(buf, /*byteOffset*/ 1ull << 32);
    appendLE<uint64_t>(buf, /*byteLength*/ 16);
    appendLE<uint64_t>(buf, /*uncompressedByteLength*/ 16);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 rejects byteLength inconsistent with logical block grid",
          "[ktx2-decoder]")
{
    // 4x4 image = 1 BC7 block = 16 bytes. Claiming 32 bytes mismatches.
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 4, 4, 1, 0, 1, 0);
    const uint64_t levelOffset = buf.size() + 24;
    appendLE<uint64_t>(buf, levelOffset);
    appendLE<uint64_t>(buf, /*byteLength*/ 32);
    appendLE<uint64_t>(buf, 32);
    buf.resize(buf.size() + 32, 0);
    rive::Ktx2DecodeResult out;
    REQUIRE_FALSE(rive::DecodeKtx2(buf.data(), buf.size(), out));
}

TEST_CASE("ktx2 happy path: single 4x4 BC7 mip 0", "[ktx2-decoder]")
{
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 4, 4, 1, 0, 1, 0);
    const uint64_t levelOffset = buf.size() + 24;
    appendLE<uint64_t>(buf, levelOffset);
    appendLE<uint64_t>(buf, /*byteLength*/ 16);
    appendLE<uint64_t>(buf, /*uncompressedByteLength*/ 16);
    // 16 bytes of synthetic block payload — parser doesn't validate the
    // BC7 bitstream, just copies the bytes through.
    const uint8_t expected[16] = {
        0xDE,
        0xAD,
        0xBE,
        0xEF,
        0x01,
        0x02,
        0x03,
        0x04,
        0x05,
        0x06,
        0x07,
        0x08,
        0xCA,
        0xFE,
        0xBA,
        0xBE,
    };
    buf.insert(buf.end(), expected, expected + 16);

    rive::Ktx2DecodeResult out;
    REQUIRE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    REQUIRE(out.format == rive::GPUTextureFormat::bc7);
    REQUIRE(out.pixelWidth == 4);
    REQUIRE(out.pixelHeight == 4);
    REQUIRE(out.levelCount == 1);
    REQUIRE(out.blocks.size() == 16);
    REQUIRE(std::memcmp(out.blocks.data(), expected, 16) == 0);
}

TEST_CASE("ktx2 happy path: 8x8 with two mip levels concatenated",
          "[ktx2-decoder]")
{
    // Mip 0 = 8x8 = 4 blocks (64 bytes). Mip 1 = 4x4 = 1 block (16 bytes).
    // Level index lists level 0 first; on disk levels should sit smallest-
    // first but the parser only reads each level by its own offset, so
    // ordering within the buffer doesn't matter here.
    auto buf = buildSkeletonKtx2(VK_FORMAT_BC7_SRGB_BLOCK, 8, 8, 2, 0, 1, 0);

    const uint64_t headerEnd = buf.size();
    const uint64_t levelIndexBytes = 24 * 2;
    const uint64_t mip0Offset = headerEnd + levelIndexBytes;
    const uint64_t mip0Bytes = 64;
    const uint64_t mip1Offset = mip0Offset + mip0Bytes;
    const uint64_t mip1Bytes = 16;

    // Level index entry 0 (mip 0, 8x8).
    appendLE<uint64_t>(buf, mip0Offset);
    appendLE<uint64_t>(buf, mip0Bytes);
    appendLE<uint64_t>(buf, mip0Bytes);
    // Level index entry 1 (mip 1, 4x4).
    appendLE<uint64_t>(buf, mip1Offset);
    appendLE<uint64_t>(buf, mip1Bytes);
    appendLE<uint64_t>(buf, mip1Bytes);

    // Block payloads. Distinct fill bytes so the test can verify ordering.
    buf.resize(buf.size() + mip0Bytes, 0xAA);
    buf.resize(buf.size() + mip1Bytes, 0xBB);

    rive::Ktx2DecodeResult out;
    REQUIRE(rive::DecodeKtx2(buf.data(), buf.size(), out));
    REQUIRE(out.pixelWidth == 8);
    REQUIRE(out.pixelHeight == 8);
    REQUIRE(out.levelCount == 2);
    REQUIRE(out.blocks.size() == mip0Bytes + mip1Bytes);
    // Output buffer is concatenated level 0 (largest) first, then level 1.
    REQUIRE(out.blocks[0] == 0xAA);
    REQUIRE(out.blocks[mip0Bytes - 1] == 0xAA);
    REQUIRE(out.blocks[mip0Bytes] == 0xBB);
    REQUIRE(out.blocks[mip0Bytes + mip1Bytes - 1] == 0xBB);
}
