/*
 * Copyright 2026 Rive
 */

#include <rive/assets/manifest_asset.hpp>
#include <rive/manifest_sections.hpp>
#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <cstdint>
#include <vector>

using namespace rive;

namespace
{
// Every value these tests write is below 128, so a varuint is a single byte.
// Keeping the encoding trivial keeps the expected bytes readable.
void writeVarUint(std::vector<uint8_t>& out, uint64_t value)
{
    REQUIRE(value < 128);
    out.push_back(static_cast<uint8_t>(value));
}

void writeSection(std::vector<uint8_t>& out,
                  ManifestSections section,
                  const std::vector<uint8_t>& payload)
{
    writeVarUint(out, static_cast<uint64_t>(section));
    writeVarUint(out, payload.size());
    out.insert(out.end(), payload.begin(), payload.end());
}

// Matches the payload the Dart exporter writes in
// ManifestAsset._writeWatermark: version, flags, artboard index.
std::vector<uint8_t> watermarkPayload(uint64_t version,
                                      uint64_t flags,
                                      uint64_t artboardIndex)
{
    std::vector<uint8_t> payload;
    writeVarUint(payload, version);
    writeVarUint(payload, flags);
    writeVarUint(payload, artboardIndex);
    return payload;
}

bool decode(ManifestAsset& asset, const std::vector<uint8_t>& bytes)
{
    SimpleArray<uint8_t> data(bytes.data(), bytes.size());
    return asset.decode(data, nullptr);
}
} // namespace

TEST_CASE("a manifest with no sections decodes with no watermark", "[manifest]")
{
    ManifestAsset asset;
    REQUIRE(decode(asset, {}));
    REQUIRE(!asset.hasWatermark());
}

TEST_CASE("the watermark section decodes", "[manifest]")
{
    std::vector<uint8_t> bytes;
    writeSection(bytes, ManifestSections::watermark, watermarkPayload(1, 1, 7));

    ManifestAsset asset;
    REQUIRE(decode(asset, bytes));
    REQUIRE(asset.hasWatermark());
    REQUIRE(asset.watermarkArtboardIndex() == 7);
}

TEST_CASE("a cleared enabled flag leaves the watermark off", "[manifest]")
{
    std::vector<uint8_t> bytes;
    writeSection(bytes, ManifestSections::watermark, watermarkPayload(1, 0, 7));

    ManifestAsset asset;
    REQUIRE(decode(asset, bytes));
    REQUIRE(!asset.hasWatermark());
}

TEST_CASE("trailing bytes in the watermark section are skipped", "[manifest]")
{
    // A newer exporter appending fields must not break this runtime: the
    // section carries its own size, and decodeWatermark swallows the rest.
    auto payload = watermarkPayload(1, 1, 3);
    payload.push_back(0x2a);
    payload.push_back(0x2b);

    std::vector<uint8_t> bytes;
    writeSection(bytes, ManifestSections::watermark, payload);

    ManifestAsset asset;
    REQUIRE(decode(asset, bytes));
    REQUIRE(asset.hasWatermark());
    REQUIRE(asset.watermarkArtboardIndex() == 3);
}

TEST_CASE("an unrecognized watermark version is ignored, not fatal",
          "[manifest]")
{
    std::vector<uint8_t> bytes;
    writeSection(bytes,
                 ManifestSections::watermark,
                 watermarkPayload(99, 1, 3));
    // A section after it must still parse, proving the unknown version was
    // consumed rather than desyncing the reader.
    std::vector<uint8_t> names;
    writeVarUint(names, 1); // one entry
    writeVarUint(names, 4); // id
    writeVarUint(names, 2); // string length
    names.push_back('h');
    names.push_back('i');
    writeSection(bytes, ManifestSections::names, names);

    ManifestAsset asset;
    REQUIRE(decode(asset, bytes));
    REQUIRE(!asset.hasWatermark());
    REQUIRE(asset.resolveName(4) == "hi");
}

TEST_CASE("an unknown section is skipped and later sections still decode",
          "[manifest]")
{
    std::vector<uint8_t> bytes;
    writeSection(bytes, static_cast<ManifestSections>(99), {0x01, 0x02, 0x03});
    writeSection(bytes, ManifestSections::watermark, watermarkPayload(1, 1, 5));

    ManifestAsset asset;
    REQUIRE(decode(asset, bytes));
    REQUIRE(asset.hasWatermark());
    REQUIRE(asset.watermarkArtboardIndex() == 5);
}

// A watermark index that does not fit in 32 bits is a malformed manifest. It
// used to be truncated, so an encoded 2^32 became index 0 and would attach
// whatever artboard sits there as the watermark; now the section is rejected.
TEST_CASE("an out-of-range watermark artboard index is rejected", "[manifest]")
{
    // manifest_detail::appendVarUint writes real multi-byte LEB128, which the
    // single-byte helper above deliberately cannot.
    std::vector<uint8_t> payload;
    manifest_detail::appendVarUint(payload, watermarkSectionVersion);
    manifest_detail::appendVarUint(payload, watermarkFlagEnabled);
    manifest_detail::appendVarUint(payload, uint64_t(1) << 32);

    std::vector<uint8_t> bytes;
    writeSection(bytes, ManifestSections::watermark, payload);

    ManifestAsset asset;
    CHECK(!decode(asset, bytes));
    CHECK(!asset.hasWatermark());

    // The largest index that does fit still decodes.
    std::vector<uint8_t> okPayload;
    manifest_detail::appendVarUint(okPayload, watermarkSectionVersion);
    manifest_detail::appendVarUint(okPayload, watermarkFlagEnabled);
    manifest_detail::appendVarUint(okPayload, 0xFFFFFFFFu);

    std::vector<uint8_t> okBytes;
    writeSection(okBytes, ManifestSections::watermark, okPayload);

    ManifestAsset ok;
    REQUIRE(decode(ok, okBytes));
    CHECK(ok.hasWatermark());
    CHECK(ok.watermarkArtboardIndex() == 0xFFFFFFFFu);
}

// ManifestSections has an unsigned char base, so casting a section id down to
// it aliases every id mod 256 onto a known section. 258 landed on watermark,
// so a section the runtime should have skipped attached one instead -- and the
// skip-unknown branch that lets a newer exporter add sections never ran.
TEST_CASE("a section id that aliases a known one is skipped", "[manifest]")
{
    // A watermark payload carried under ids the decoder must not recognize.
    std::vector<uint8_t> payload =
        watermarkPayload(watermarkSectionVersion, watermarkFlagEnabled, 7);

    for (uint64_t aliased : {uint64_t(256), uint64_t(257), uint64_t(258)})
    {
        std::vector<uint8_t> bytes;
        manifest_detail::appendVarUint(bytes, aliased);
        manifest_detail::appendVarUint(bytes, payload.size());
        bytes.insert(bytes.end(), payload.begin(), payload.end());

        ManifestAsset asset;
        INFO("section id " << aliased);
        // Unknown sections are skipped, so the manifest still decodes...
        REQUIRE(decode(asset, bytes));
        // ...and nothing was read out of them.
        CHECK(!asset.hasWatermark());
    }

    // The real id still works, so this is not just rejecting everything.
    std::vector<uint8_t> real;
    writeSection(real, ManifestSections::watermark, payload);
    ManifestAsset asset;
    REQUIRE(decode(asset, real));
    CHECK(asset.hasWatermark());
    CHECK(asset.watermarkArtboardIndex() == 7);
}
