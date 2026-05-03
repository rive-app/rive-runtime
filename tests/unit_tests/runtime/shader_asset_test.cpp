/*
 * Copyright 2026 Rive
 */

#include "rive/assets/shader_asset.hpp"
#include "rive/simple_array.hpp"
#include <catch.hpp>
#include <cstring>
#include <vector>

namespace rive
{

// Helper to build a minimal RSTB v2 binary.
static std::vector<uint8_t> makeRSTB(
    uint16_t version,
    // Each shader: {shaderId, {{target, blobData}, ...}}
    const std::vector<
        std::pair<uint32_t,
                  std::vector<std::pair<uint8_t, std::vector<uint8_t>>>>>&
        shaders,
    uint32_t magic = 0x52535442u)
{
    std::vector<uint8_t> out;

    // Header: magic(u32LE) + version(u16LE) + shader_count(u16LE)
    auto pushU32 = [&](uint32_t v) {
        out.push_back(v & 0xFF);
        out.push_back((v >> 8) & 0xFF);
        out.push_back((v >> 16) & 0xFF);
        out.push_back((v >> 24) & 0xFF);
    };
    auto pushU16 = [&](uint16_t v) {
        out.push_back(v & 0xFF);
        out.push_back((v >> 8) & 0xFF);
    };

    pushU32(magic);
    pushU16(version);
    pushU16(static_cast<uint16_t>(shaders.size()));

    // Collect all blob data and compute offsets relative to blob section start.
    std::vector<uint8_t> blobSection;
    // We need to write entries with blob offsets, so pre-compute them.
    struct VariantInfo
    {
        uint8_t target;
        uint32_t blobOffset;
        uint32_t blobSize;
    };
    struct ShaderInfo
    {
        uint32_t shaderId;
        std::vector<VariantInfo> variants;
    };
    std::vector<ShaderInfo> infos;

    for (auto& [shaderId, variants] : shaders)
    {
        ShaderInfo si;
        si.shaderId = shaderId;
        for (auto& [target, blob] : variants)
        {
            VariantInfo vi;
            vi.target = target;
            vi.blobOffset = static_cast<uint32_t>(blobSection.size());
            vi.blobSize = static_cast<uint32_t>(blob.size());
            blobSection.insert(blobSection.end(), blob.begin(), blob.end());
            si.variants.push_back(vi);
        }
        infos.push_back(std::move(si));
    }

    // Write shader entries.
    for (auto& si : infos)
    {
        pushU32(si.shaderId);
        out.push_back(static_cast<uint8_t>(si.variants.size()));
        out.push_back(0); // section_count = 0 (no tagged sections)
        for (auto& vi : si.variants)
        {
            out.push_back(vi.target);
            pushU32(vi.blobOffset);
            pushU32(vi.blobSize);
        }
    }

    // Append blob section.
    out.insert(out.end(), blobSection.begin(), blobSection.end());
    return out;
}

TEST_CASE("ShaderAsset-isTypeOf", "[ShaderAsset]")
{
    ShaderAsset asset;
    CHECK(asset.isTypeOf(970) == true); // ShaderAssetBase::typeKey
    CHECK(asset.isTypeOf(103) == true); // FileAssetBase::typeKey
    CHECK(asset.isTypeOf(99) == true);  // AssetBase::typeKey
    CHECK(asset.isTypeOf(0) == false);
    CHECK(asset.isTypeOf(999) == false);
}

TEST_CASE("ShaderAsset-coreType", "[ShaderAsset]")
{
    ShaderAsset asset;
    CHECK(asset.coreType() == 970);
}

TEST_CASE("ShaderAsset-fileExtension", "[ShaderAsset]")
{
    ShaderAsset asset;
    CHECK(asset.fileExtension() == "rstb");
}

TEST_CASE("ShaderAsset-decode-valid", "[ShaderAsset]")
{
    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    auto rstb = makeRSTB(2, {{100, {{2, blob}}}});

    // Prepend the SignedContentHeader envelope (unsigned, flags=0) so the
    // input matches ShaderAsset::decode's expected format.
    std::vector<uint8_t> enveloped;
    enveloped.push_back(0x00);
    enveloped.insert(enveloped.end(), rstb.begin(), rstb.end());
    SimpleArray<uint8_t> data(enveloped.data(), enveloped.size());
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == true);

    auto result = asset.findShader(100, 2);
    REQUIRE(result.size() == 5);
    CHECK(result[0] == 0xDE);
    CHECK(result[1] == 0xAD);
    CHECK(result[2] == 0xBE);
    CHECK(result[3] == 0xEF);
    CHECK(result[4] == 0x42);
}

TEST_CASE("ShaderAsset-decode-bad-magic", "[ShaderAsset]")
{
    auto rstb = makeRSTB(2, {}, 0xBADBAD00u);
    // Prepend the SignedContentHeader envelope (unsigned, flags=0) so the
    // input matches ShaderAsset::decode's expected format.
    std::vector<uint8_t> enveloped;
    enveloped.push_back(0x00);
    enveloped.insert(enveloped.end(), rstb.begin(), rstb.end());
    SimpleArray<uint8_t> data(enveloped.data(), enveloped.size());
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-bad-version", "[ShaderAsset]")
{
    auto rstb = makeRSTB(99, {});
    // Prepend the SignedContentHeader envelope (unsigned, flags=0) so the
    // input matches ShaderAsset::decode's expected format.
    std::vector<uint8_t> enveloped;
    enveloped.push_back(0x00);
    enveloped.insert(enveloped.end(), rstb.begin(), rstb.end());
    SimpleArray<uint8_t> data(enveloped.data(), enveloped.size());
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-truncated", "[ShaderAsset]")
{
    // Envelope byte + less than 8 bytes of RSTB = too short for header.
    uint8_t tiny[] = {0x00, 0x52, 0x53, 0x54, 0x42, 0x01, 0x00};
    SimpleArray<uint8_t> data(tiny, sizeof(tiny));
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-findShader-miss", "[ShaderAsset]")
{
    auto rstb = makeRSTB(2, {{100, {{2, {0xAA}}}}});
    // Prepend the SignedContentHeader envelope (unsigned, flags=0) so the
    // input matches ShaderAsset::decode's expected format.
    std::vector<uint8_t> enveloped;
    enveloped.push_back(0x00);
    enveloped.insert(enveloped.end(), rstb.begin(), rstb.end());
    SimpleArray<uint8_t> data(enveloped.data(), enveloped.size());
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);

    // Wrong shaderId
    CHECK(asset.findShader(999, 2).empty());
    // Wrong target
    CHECK(asset.findShader(100, 0).empty());
}

TEST_CASE("ShaderAsset-multiple-shaders", "[ShaderAsset]")
{
    std::vector<uint8_t> blob1 = {0x11, 0x22};
    std::vector<uint8_t> blob2 = {0x33, 0x44, 0x55};
    std::vector<uint8_t> blob3 = {0x66};
    std::vector<uint8_t> blob4 = {0x77, 0x88, 0x99, 0xAA};

    // Shader 1 has targets 0 and 1; shader 2 has targets 2 and 3.
    auto rstb = makeRSTB(
        2,
        {{1, {{0, blob1}, {1, blob2}}}, {2, {{2, blob3}, {3, blob4}}}});

    // Prepend the SignedContentHeader envelope (unsigned, flags=0) so the
    // input matches ShaderAsset::decode's expected format.
    std::vector<uint8_t> enveloped;
    enveloped.push_back(0x00);
    enveloped.insert(enveloped.end(), rstb.begin(), rstb.end());
    SimpleArray<uint8_t> data(enveloped.data(), enveloped.size());
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);

    auto r1 = asset.findShader(1, 0);
    REQUIRE(r1.size() == 2);
    CHECK(r1[0] == 0x11);

    auto r2 = asset.findShader(1, 1);
    REQUIRE(r2.size() == 3);
    CHECK(r2[0] == 0x33);

    auto r3 = asset.findShader(2, 2);
    REQUIRE(r3.size() == 1);
    CHECK(r3[0] == 0x66);

    auto r4 = asset.findShader(2, 3);
    REQUIRE(r4.size() == 4);
    CHECK(r4[0] == 0x77);
}

} // namespace rive
