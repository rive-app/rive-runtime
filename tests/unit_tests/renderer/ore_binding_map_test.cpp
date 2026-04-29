/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_binding_map.hpp"
#include <catch.hpp>

namespace rive::ore
{

static BindingMap::Entry makeEntry(
    uint8_t group,
    uint8_t binding,
    ResourceKind kind,
    uint16_t slotVs,
    uint16_t slotFs = BindingMap::kAbsent,
    uint16_t slotCs = BindingMap::kAbsent,
    uint8_t stageMask = BindingMap::kStageVertex | BindingMap::kStageFragment,
    uint8_t backendSpace = 0)
{
    BindingMap::Entry e{};
    e.group = group;
    e.binding = binding;
    e.kind = kind;
    e.stageMask = stageMask;
    e.backendSpace = backendSpace;
    e.backendSlot[0] = slotVs;
    e.backendSlot[1] = slotFs;
    e.backendSlot[2] = slotCs;
    return e;
}

TEST_CASE("BindingMap lookup after finalize", "[ore_binding_map]")
{
    BindingMap m;
    // Push out-of-order to verify finalize sorts.
    m.push(makeEntry(1, 2, ResourceKind::SampledTexture, 5, 5));
    m.push(makeEntry(0, 1, ResourceKind::UniformBuffer, 1, 1));
    m.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 0, 0));
    m.finalize();

    REQUIRE(m.size() == 3);
    CHECK(m.at(0).group == 0);
    CHECK(m.at(0).binding == 0);
    CHECK(m.at(1).group == 0);
    CHECK(m.at(1).binding == 1);
    CHECK(m.at(2).group == 1);
    CHECK(m.at(2).binding == 2);

    CHECK(m.lookup(0, 0, ResourceKind::UniformBuffer, BindingMap::Stage::VS) ==
          0);
    CHECK(m.lookup(0, 1, ResourceKind::UniformBuffer, BindingMap::Stage::VS) ==
          1);
    CHECK(m.lookup(1, 2, ResourceKind::SampledTexture, BindingMap::Stage::VS) ==
          5);

    // Missing pair returns kAbsent.
    CHECK(m.lookup(9, 9, ResourceKind::UniformBuffer, BindingMap::Stage::VS) ==
          BindingMap::kAbsent);
    // Wrong kind returns kAbsent.
    CHECK(m.lookup(0, 0, ResourceKind::Sampler, BindingMap::Stage::VS) ==
          BindingMap::kAbsent);
}

TEST_CASE("BindingMap stage visibility", "[ore_binding_map]")
{
    BindingMap m;
    // VS-only: FS lookup must return kAbsent even though an FS slot is set.
    m.push(makeEntry(0,
                     0,
                     ResourceKind::UniformBuffer,
                     /*slotVs*/ 3,
                     /*slotFs*/ BindingMap::kAbsent,
                     /*slotCs*/ BindingMap::kAbsent,
                     /*stageMask*/ BindingMap::kStageVertex));
    m.finalize();

    CHECK(m.lookup(0, 0, ResourceKind::UniformBuffer, BindingMap::Stage::VS) ==
          3);
    CHECK(m.lookup(0, 0, ResourceKind::UniformBuffer, BindingMap::Stage::FS) ==
          BindingMap::kAbsent);
    CHECK(m.lookup(0, 0, ResourceKind::UniformBuffer, BindingMap::Stage::CS) ==
          BindingMap::kAbsent);
}

TEST_CASE("BindingMap toBlob / fromBlob round-trip", "[ore_binding_map]")
{
    BindingMap original;
    original.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 0, 0));
    original.push(makeEntry(0,
                            7,
                            ResourceKind::UniformBuffer,
                            1,
                            1,
                            /*slotCs*/ BindingMap::kAbsent,
                            /*stageMask*/ BindingMap::kStageVertex |
                                BindingMap::kStageFragment,
                            /*backendSpace*/ 0));
    original.push(makeEntry(2,
                            0,
                            ResourceKind::SampledTexture,
                            5,
                            5,
                            /*slotCs*/ BindingMap::kAbsent,
                            /*stageMask*/ BindingMap::kStageFragment,
                            /*backendSpace*/ 2));
    original.finalize();

    std::vector<uint8_t> blob = original.toBlob();
    // Header: 8 bytes. Each entry: 14 bytes. Total: 8 + 3*14 = 50.
    REQUIRE(blob.size() == 50);
    CHECK(blob[0] == BindingMap::kBlobVersion);
    CHECK(blob[1] == BindingMap::kAllocatorVersion);
    // entry_size = 14 (little-endian).
    CHECK(blob[2] == 14);
    CHECK(blob[3] == 0);
    // entry_count = 3.
    CHECK(blob[4] == 3);
    CHECK(blob[5] == 0);
    CHECK(blob[6] == 0);
    CHECK(blob[7] == 0);

    BindingMap restored;
    REQUIRE(BindingMap::fromBlob(blob.data(), blob.size(), &restored));
    REQUIRE(restored.size() == 3);

    // Round-trip preserves every field.
    for (size_t i = 0; i < original.size(); ++i)
    {
        const auto& a = original.at(i);
        const auto& b = restored.at(i);
        CHECK(a.group == b.group);
        CHECK(a.binding == b.binding);
        CHECK(a.kind == b.kind);
        CHECK(a.stageMask == b.stageMask);
        CHECK(a.backendSpace == b.backendSpace);
        CHECK(a.backendSlot[0] == b.backendSlot[0]);
        CHECK(a.backendSlot[1] == b.backendSlot[1]);
        CHECK(a.backendSlot[2] == b.backendSlot[2]);
    }
}

TEST_CASE("BindingMap rejects bad blob version", "[ore_binding_map]")
{
    BindingMap source;
    source.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 0, 0));
    source.finalize();
    std::vector<uint8_t> blob = source.toBlob();

    BindingMap out;

    // Null / too small.
    CHECK_FALSE(BindingMap::fromBlob(nullptr, 0, &out));
    CHECK_FALSE(BindingMap::fromBlob(blob.data(), 4, &out));

    // Bad blob version → fail loud, per RFC §14.4.
    std::vector<uint8_t> badBlobVer = blob;
    badBlobVer[0] = BindingMap::kBlobVersion + 1;
    CHECK_FALSE(
        BindingMap::fromBlob(badBlobVer.data(), badBlobVer.size(), &out));

    // Bad allocator version → fail loud.
    std::vector<uint8_t> badAllocVer = blob;
    badAllocVer[1] = BindingMap::kAllocatorVersion + 1;
    CHECK_FALSE(
        BindingMap::fromBlob(badAllocVer.data(), badAllocVer.size(), &out));
}

TEST_CASE("BindingMap rejects truncated blob", "[ore_binding_map]")
{
    BindingMap source;
    source.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 0, 0));
    source.push(makeEntry(1, 0, ResourceKind::SampledTexture, 0, 0));
    source.finalize();
    std::vector<uint8_t> blob = source.toBlob();

    BindingMap out;
    REQUIRE(BindingMap::fromBlob(blob.data(), blob.size(), &out));

    // Drop the last byte → size mismatch → fail.
    CHECK_FALSE(BindingMap::fromBlob(blob.data(), blob.size() - 1, &out));
}

TEST_CASE("BindingMap forward-compat: larger entry_size parses OK",
          "[ore_binding_map]")
{
    BindingMap source;
    source.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 0, 0));
    source.finalize();
    std::vector<uint8_t> blob = source.toBlob();

    // Read the writer's entry_size out of the real blob so the test stays
    // meaningful as the wire format grows (RFC §14.6 append-only fields).
    // Synthesize a "future" blob that's `kExtraTrailing` bytes longer per
    // entry; the reader should accept it by skipping the unknown tail.
    const uint16_t currentEntrySize =
        static_cast<uint16_t>(blob[2]) | (static_cast<uint16_t>(blob[3]) << 8);
    constexpr uint16_t kExtraTrailing = 4;
    const uint16_t futureEntrySize = currentEntrySize + kExtraTrailing;

    std::vector<uint8_t> futureBlob(8 + 1 * futureEntrySize);
    futureBlob[0] = BindingMap::kBlobVersion;
    futureBlob[1] = BindingMap::kAllocatorVersion;
    futureBlob[2] = static_cast<uint8_t>(futureEntrySize & 0xFF);
    futureBlob[3] = static_cast<uint8_t>((futureEntrySize >> 8) & 0xFF);
    futureBlob[4] = 1; // entry_count LE
    // Copy the current entry verbatim, then append unknown trailing bytes.
    std::memcpy(futureBlob.data() + 8, blob.data() + 8, currentEntrySize);
    for (uint16_t i = 0; i < kExtraTrailing; ++i)
        futureBlob[8 + currentEntrySize + i] = 0xFF;

    BindingMap out;
    REQUIRE(BindingMap::fromBlob(futureBlob.data(), futureBlob.size(), &out));
    REQUIRE(out.size() == 1);
    CHECK(out.at(0).group == 0);
    CHECK(out.at(0).binding == 0);
    CHECK(out.at(0).kind == ResourceKind::UniformBuffer);
}

TEST_CASE("BindingMap forward-compat: smaller entry_size rejected",
          "[ore_binding_map]")
{
    // A blob claiming entry_size below the reader's known prefix would
    // mean the writer omitted a field the reader needs — reject loudly
    // rather than misinterpret.
    std::vector<uint8_t> blob(8);
    blob[0] = BindingMap::kBlobVersion;
    blob[1] = BindingMap::kAllocatorVersion;
    blob[2] = 10; // entry_size = 10, below kEntryWireSize (14)
    blob[3] = 0;
    blob[4] = 0; // entry_count = 0 (so no actual payload needed)
    blob[5] = 0;
    blob[6] = 0;
    blob[7] = 0;

    BindingMap out;
    CHECK_FALSE(BindingMap::fromBlob(blob.data(), blob.size(), &out));
}

TEST_CASE("ResourceKind numeric values are frozen", "[ore_binding_map]")
{
    // RFC §14.5: never renumber. Locks the on-disk RSTB schema.
    CHECK(static_cast<uint8_t>(ResourceKind::UniformBuffer) == 0);
    CHECK(static_cast<uint8_t>(ResourceKind::StorageBufferRO) == 1);
    CHECK(static_cast<uint8_t>(ResourceKind::StorageBufferRW) == 2);
    CHECK(static_cast<uint8_t>(ResourceKind::SampledTexture) == 3);
    CHECK(static_cast<uint8_t>(ResourceKind::StorageTexture) == 4);
    CHECK(static_cast<uint8_t>(ResourceKind::Sampler) == 5);
    CHECK(static_cast<uint8_t>(ResourceKind::ComparisonSampler) == 6);
}

TEST_CASE("lookupBackendSlot helper", "[ore_binding_map]")
{
    BindingMap m;
    m.push(makeEntry(0, 0, ResourceKind::UniformBuffer, 7, 7));
    m.finalize();
    CHECK(lookupBackendSlot(m,
                            0,
                            0,
                            ResourceKind::UniformBuffer,
                            BindingMap::Stage::VS) == 7);
    CHECK(lookupBackendSlot(m,
                            0,
                            0,
                            ResourceKind::UniformBuffer,
                            BindingMap::Stage::FS) == 7);
}

} // namespace rive::ore
