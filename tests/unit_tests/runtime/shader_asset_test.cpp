/*
 * Copyright 2026 Rive
 */

#include "rive/assets/shader_asset.hpp"
#include "rive/simple_array.hpp"
#include <array>
#include <catch.hpp>
#include <cstring>
#include <vector>

namespace rive
{

// Helper to build a minimal RSTB v4 binary.
//   variants: vector of (target, blobData) pairs.
//   sections: vector of (tag, sectionData) pairs written verbatim after the
//             variant descriptors with a u16 length prefix.
static std::vector<uint8_t> makeRSTB(
    uint16_t version,
    const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& variants,
    const std::vector<std::pair<uint8_t, std::vector<uint8_t>>>& sections = {},
    uint32_t magic = 0x52535442u)
{
    std::vector<uint8_t> out;

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

    // Header: magic + version + variant_count + section_count.
    pushU32(magic);
    pushU16(version);
    out.push_back(static_cast<uint8_t>(variants.size()));
    out.push_back(static_cast<uint8_t>(sections.size()));

    // Variant descriptors with offsets relative to blob section.
    std::vector<uint8_t> blobSection;
    for (auto& [target, blob] : variants)
    {
        out.push_back(target);
        pushU32(static_cast<uint32_t>(blobSection.size()));
        pushU32(static_cast<uint32_t>(blob.size()));
        blobSection.insert(blobSection.end(), blob.begin(), blob.end());
    }

    // Tagged sections: tag(u8) + length(u16) + data[length].
    for (auto& [tag, data] : sections)
    {
        out.push_back(tag);
        pushU16(static_cast<uint16_t>(data.size()));
        out.insert(out.end(), data.begin(), data.end());
    }

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

// Wrap raw RSTB bytes in an unsigned SignedContentHeader envelope (flags=0)
// to match ShaderAsset::decode's expected input.
static SimpleArray<uint8_t> envelope(const std::vector<uint8_t>& rstb)
{
    std::vector<uint8_t> out;
    out.push_back(0x00);
    out.insert(out.end(), rstb.begin(), rstb.end());
    return SimpleArray<uint8_t>(out.data(), out.size());
}

TEST_CASE("ShaderAsset-decode-valid", "[ShaderAsset]")
{
    std::vector<uint8_t> blob = {0xDE, 0xAD, 0xBE, 0xEF, 0x42};
    auto data = envelope(makeRSTB(4, {{2, blob}}));
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == true);

    auto result = asset.findShader(2);
    REQUIRE(result.size() == 5);
    CHECK(result[0] == 0xDE);
    CHECK(result[1] == 0xAD);
    CHECK(result[2] == 0xBE);
    CHECK(result[3] == 0xEF);
    CHECK(result[4] == 0x42);
}

TEST_CASE("ShaderAsset-decode-bad-magic", "[ShaderAsset]")
{
    auto data = envelope(makeRSTB(4, {}, {}, 0xBADBAD00u));
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-bad-version", "[ShaderAsset]")
{
    // v3 (and earlier) is no longer accepted after the v4 entry-container bump.
    auto data = envelope(makeRSTB(3, {}));
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
    auto data = envelope(makeRSTB(4, {{2, {0xAA}}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);

    CHECK(asset.findShader(0).empty());
    CHECK(asset.findShader(99).empty());
    REQUIRE(asset.findShader(2).size() == 1);
}

TEST_CASE("ShaderAsset-multiple-targets", "[ShaderAsset]")
{
    std::vector<uint8_t> blob0 = {0x11, 0x22};
    std::vector<uint8_t> blob1 = {0x33, 0x44, 0x55};
    std::vector<uint8_t> blob2 = {0x66};
    std::vector<uint8_t> blob3 = {0x77, 0x88, 0x99, 0xAA};

    auto data =
        envelope(makeRSTB(4, {{0, blob0}, {1, blob1}, {2, blob2}, {3, blob3}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);

    auto r0 = asset.findShader(0);
    REQUIRE(r0.size() == 2);
    CHECK(r0[0] == 0x11);

    auto r1 = asset.findShader(1);
    REQUIRE(r1.size() == 3);
    CHECK(r1[0] == 0x33);

    auto r2 = asset.findShader(2);
    REQUIRE(r2.size() == 1);
    CHECK(r2[0] == 0x66);

    auto r3 = asset.findShader(3);
    REQUIRE(r3.size() == 4);
    CHECK(r3[0] == 0x77);
}

// Tag 1 section data: pair_count(u8) + N * (texGroup, texBinding,
// sampGroup, sampBinding).
static std::vector<uint8_t> makeTexSamplerPairsTag(
    const std::vector<std::array<uint8_t, 4>>& pairs)
{
    std::vector<uint8_t> out;
    out.push_back(static_cast<uint8_t>(pairs.size()));
    for (auto& p : pairs)
    {
        out.push_back(p[0]);
        out.push_back(p[1]);
        out.push_back(p[2]);
        out.push_back(p[3]);
    }
    return out;
}

TEST_CASE("ShaderAsset-decode-texture-sampler-pairs", "[ShaderAsset]")
{
    auto pairs =
        makeTexSamplerPairsTag({{0, 1, 0, 2}, {1, 3, 1, 4}, {2, 5, 2, 6}});
    auto data = envelope(makeRSTB(4, {{2, {0xAA}}}, {{1, pairs}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);

    auto out = asset.textureSamplerPairs();
    REQUIRE(out.size() == 3);
    CHECK(out[0].texGroup == 0);
    CHECK(out[0].texBinding == 1);
    CHECK(out[0].sampGroup == 0);
    CHECK(out[0].sampBinding == 2);
    CHECK(out[1].texBinding == 3);
    CHECK(out[2].sampBinding == 6);

    // The variant blob still resolves correctly after the tagged section.
    auto blob = asset.findShader(2);
    REQUIRE(blob.size() == 1);
    CHECK(blob[0] == 0xAA);
}

TEST_CASE("ShaderAsset-decode-no-sections-empty-pairs", "[ShaderAsset]")
{
    auto data = envelope(makeRSTB(4, {{2, {0x42}}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);
    CHECK(asset.textureSamplerPairs().size() == 0);
}

TEST_CASE("ShaderAsset-decode-unknown-tag-skipped", "[ShaderAsset]")
{
    // Tag 99 (unknown). Reader must skip the section without failing,
    // and pairs must remain empty.
    std::vector<uint8_t> bogusSection = {0xDE, 0xAD, 0xBE, 0xEF};
    auto data = envelope(makeRSTB(4, {{2, {0x42}}}, {{99, bogusSection}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);
    CHECK(asset.textureSamplerPairs().size() == 0);
    auto blob = asset.findShader(2);
    REQUIRE(blob.size() == 1);
    CHECK(blob[0] == 0x42);
}

TEST_CASE("ShaderAsset-decode-truncated-section-header", "[ShaderAsset]")
{
    // Build a valid RSTB then chop bytes so the section header (3 bytes)
    // doesn't fit. Header(8) + Variant(9) = 17 bytes before the section,
    // and we claim section_count=1 so the reader will look for a section.
    auto rstb = makeRSTB(4, {{2, {0x42}}}, {{1, {0}}});
    // Truncate to leave only 1 byte where the 3-byte section header
    // should be. Section header starts at offset 17.
    rstb.resize(17 + 1);
    auto data = envelope(rstb);
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-truncated-section-data", "[ShaderAsset]")
{
    // Section claims length=10 but only 2 data bytes follow.
    auto rstb = makeRSTB(4, {{2, {0x42}}});
    // Bump section_count from 0 to 1.
    rstb[7] = 1;
    // Append a section header claiming 10 data bytes but supply only 2.
    rstb.push_back(1);    // tag
    rstb.push_back(10);   // length lo
    rstb.push_back(0);    // length hi
    rstb.push_back(0xAA); // data byte 1
    rstb.push_back(0xBB); // data byte 2
    auto data = envelope(rstb);
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-rejects-out-of-range-variant", "[ShaderAsset]")
{
    // Build a valid 1-variant RSTB then patch the variant's blob_size to
    // a value that runs past m_bytes. Decode must fail rather than store
    // an entry that findShader would later return as a Span past the buf.
    auto rstb = makeRSTB(4, {{2, {0xAA}}});
    // Variant descriptor starts at byte 8: target(u8) + blob_offset(u32)
    // + blob_size(u32). blob_size lives at bytes 13..16.
    rstb[13] = 0xFF;
    rstb[14] = 0xFF;
    rstb[15] = 0xFF;
    rstb[16] = 0x7F;
    auto data = envelope(rstb);
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
    CHECK(asset.findShader(2).empty());
}

TEST_CASE("ShaderAsset-decode-rejects-overflowing-variant", "[ShaderAsset]")
{
    // blob_offset = 0xFFFFFFF0, blob_size = 0x20. Naive uint32_t addition
    // wraps to 0x10 and a non-overflow-aware bounds check would pass.
    auto rstb = makeRSTB(4, {{2, {0xAA}}});
    rstb[9] = 0xF0;
    rstb[10] = 0xFF;
    rstb[11] = 0xFF;
    rstb[12] = 0xFF;
    rstb[13] = 0x20;
    rstb[14] = 0x00;
    rstb[15] = 0x00;
    rstb[16] = 0x00;
    auto data = envelope(rstb);
    ShaderAsset asset;
    CHECK(asset.decode(data, nullptr) == false);
}

TEST_CASE("ShaderAsset-decode-pair-count-mismatch-ignored", "[ShaderAsset]")
{
    // Section claims pair_count=5 but only carries enough bytes for 2.
    // Reader should skip the pair payload (length insufficient) but not
    // fail the overall decode; subsequent variants still resolve.
    std::vector<uint8_t> badPairs = {5, 0, 1, 0, 2, 1, 3, 1, 4};
    auto data = envelope(makeRSTB(4, {{2, {0xAA}}}, {{1, badPairs}}));
    ShaderAsset asset;
    REQUIRE(asset.decode(data, nullptr) == true);
    CHECK(asset.textureSamplerPairs().size() == 0);
    auto blob = asset.findShader(2);
    REQUIRE(blob.size() == 1);
    CHECK(blob[0] == 0xAA);
}

} // namespace rive
