/*
 * Copyright 2026 Rive
 */

#include "rive/renderer/ore/ore_binding_map.hpp"

#include <algorithm>
#include <cstring>

// BindingMap serialization (toBlob/fromBlob), sort/finalize, and lookup.
// All construction at runtime goes through `fromBlob` against an RSTB
// sidecar; the editor-side toolchain produces the blob via `toBlob`.

namespace rive::ore
{

namespace
{

// On-disk blob layout:
//
//   offset 0  [u8]  blob_version        (= kBlobVersion)
//          1  [u8]  allocator_version   (= kAllocatorVersion)
//          2  [u16] entry_size (LE)     (grows append-only)
//          4  [u32] entry_count (LE)
//          8  [u16] group_size (LE)     (grows append-only)
//         10  [u16] group_count (LE)
//         12  [entry_count * entry_size] entries
//            [group_count * group_size] group layout ids
//
// Each group row (group_size = 9 bytes):
//
//          0  [u8]  group
//          1  [u64] layout_id (LE)
//
// The group table makes layout identity a build-time fact, not a runtime
// hash. Ids are backend scoped, since they cover native slots.
//
// Each entry (entry_size = 14 bytes, no trailing alignment):
//
//          0  [u8]  group
//          1  [u8]  binding
//          2  [u8]  kind (ResourceKind)
//          3  [u8]  stageMask
//          4  [u8]  backendSpace
//          5  [u16] backendSlot[0] (VS, LE)
//          7  [u16] backendSlot[1] (FS, LE)
//          9  [u16] backendSlot[2] (CS, LE)
//         11  [u8]  textureViewDim (TextureViewDim)
//         12  [u8]  textureSampleType (TextureSampleType)
//         13  [u8]  textureMultisampled (0 or 1)
//
// Forward compat: a newer writer may emit entries larger than the current
// reader knows about by bumping entry_size. The reader skips the trailing
// unknown bytes per entry. New fields are always *appended* at the tail.
// No reserved-for-future slots inside the known prefix, since entry_size
// already gives us self-describing append-only growth. Any mismatch that
// matters semantically (blob_version or allocator_version) is a loud
// error.

constexpr size_t kBlobHeaderSize = 12;
constexpr uint16_t kEntryWireSize = 14;
constexpr uint16_t kGroupWireSize = 9;

inline uint16_t readU16LE(const uint8_t* p)
{
    return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);
}

inline uint32_t readU32LE(const uint8_t* p)
{
    return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

inline uint64_t readU64LE(const uint8_t* p)
{
    uint64_t v = 0;
    for (int i = 7; i >= 0; --i)
        v = (v << 8) | static_cast<uint64_t>(p[i]);
    return v;
}

#ifdef WITH_RIVE_TOOLS
inline void writeU16LE(uint8_t* p, uint16_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
}

inline void writeU32LE(uint8_t* p, uint32_t v)
{
    p[0] = static_cast<uint8_t>(v & 0xFF);
    p[1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    p[2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    p[3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}

inline void writeU64LE(uint8_t* p, uint64_t v)
{
    for (int i = 0; i < 8; ++i)
        p[i] = static_cast<uint8_t>((v >> (i * 8)) & 0xFF);
}
#endif

} // namespace

// Runtime API: fromBlob trusts its input to be sorted (toBlob iterates
// m_entries in canonical order). No sort on the hot path.
bool BindingMap::fromBlob(const uint8_t* data, size_t size, BindingMap* out)
{
    if (out == nullptr || data == nullptr)
        return false;
    out->m_entries.clear();
    out->m_groupLayouts.clear();
#ifdef WITH_RIVE_TOOLS
    out->m_finalized = false;
#endif

    if (size < kBlobHeaderSize)
        return false;

    const uint8_t blobVer = data[0];
    const uint8_t allocVer = data[1];
    if (blobVer != kBlobVersion)
        return false; // Never silent-fallback on a malformed blob.
    if (allocVer != kAllocatorVersion)
        return false;

    const uint16_t entrySize = readU16LE(&data[2]);
    const uint32_t entryCount = readU32LE(&data[4]);
    const uint16_t groupSize = readU16LE(&data[8]);
    const uint16_t groupCount = readU16LE(&data[10]);

    // Reject writers that emit fewer fields than the reader needs.
    // Larger entry_size is fine — trailing unknown bytes are skipped.
    if (entrySize < kEntryWireSize)
        return false;
    if (groupCount != 0 && groupSize < kGroupWireSize)
        return false;

    const size_t entryBytes = static_cast<size_t>(entryCount) * entrySize;
    const size_t groupBytes = static_cast<size_t>(groupCount) * groupSize;
    const size_t needed = kBlobHeaderSize + entryBytes + groupBytes;
    if (size < needed)
        return false;

    out->m_entries.reserve(entryCount);
    const uint8_t* p = data + kBlobHeaderSize;
    for (uint32_t i = 0; i < entryCount; ++i)
    {
        Entry e{};
        e.group = p[0];
        e.binding = p[1];
        e.kind = static_cast<ResourceKind>(p[2]);
        e.stageMask = p[3];
        e.backendSpace = p[4];
        e.backendSlot[0] = readU16LE(&p[5]);
        e.backendSlot[1] = readU16LE(&p[7]);
        e.backendSlot[2] = readU16LE(&p[9]);
        e.textureViewDim = static_cast<TextureViewDim>(p[11]);
        e.textureSampleType = static_cast<TextureSampleType>(p[12]);
        e.textureMultisampled = (p[13] != 0);
        // bytes [kEntryWireSize..entrySize] are future-version fields — skip.
        out->m_entries.push_back(e);
        p += entrySize;
    }

    out->m_groupLayouts.reserve(groupCount);
    for (uint16_t i = 0; i < groupCount; ++i)
    {
        GroupLayout g{};
        g.group = p[0];
        g.layoutId = readU64LE(&p[1]);
        out->m_groupLayouts.push_back(g);
        p += groupSize;
    }
#ifdef WITH_RIVE_TOOLS
    // Flip the finalized flag so tooling-build lookups satisfy their assert.
    // The blob is already sorted by construction; no std::sort call.
    out->m_finalized = true;
#endif
    return true;
}

#ifdef WITH_RIVE_TOOLS

std::vector<uint8_t> BindingMap::toBlob() const
{
    std::vector<uint8_t> blob(kBlobHeaderSize +
                              m_entries.size() * kEntryWireSize +
                              m_groupLayouts.size() * kGroupWireSize);
    blob[0] = kBlobVersion;
    blob[1] = kAllocatorVersion;
    writeU16LE(&blob[2], kEntryWireSize);
    writeU32LE(&blob[4], static_cast<uint32_t>(m_entries.size()));
    writeU16LE(&blob[8], kGroupWireSize);
    writeU16LE(&blob[10], static_cast<uint16_t>(m_groupLayouts.size()));

    uint8_t* p = blob.data() + kBlobHeaderSize;
    for (const Entry& e : m_entries)
    {
        p[0] = e.group;
        p[1] = e.binding;
        p[2] = static_cast<uint8_t>(e.kind);
        p[3] = e.stageMask;
        p[4] = e.backendSpace;
        writeU16LE(&p[5], e.backendSlot[0]);
        writeU16LE(&p[7], e.backendSlot[1]);
        writeU16LE(&p[9], e.backendSlot[2]);
        p[11] = static_cast<uint8_t>(e.textureViewDim);
        p[12] = static_cast<uint8_t>(e.textureSampleType);
        p[13] = e.textureMultisampled ? 1u : 0u;
        p += kEntryWireSize;
    }

    for (const GroupLayout& g : m_groupLayouts)
    {
        p[0] = g.group;
        writeU64LE(&p[1], g.layoutId);
        p += kGroupWireSize;
    }
    return blob;
}

void BindingMap::computeLayoutIds()
{
    // A serialized field left out of the hash makes two different layouts
    // collide on one id, and misbind at draw time.
    static_assert(kEntryWireSize == 14,
                  "Entry grew, hash the new field below before bumping");

    assert(m_finalized && "BindingMap::computeLayoutIds before finalize");
    m_groupLayouts.clear();

    // FNV-1a 64, fed little-endian so ids match across baking hosts.
    constexpr uint64_t kOffsetBasis = 0xcbf29ce484222325ull;
    constexpr uint64_t kPrime = 0x100000001b3ull;
    auto mix = [](uint64_t h, uint8_t b) { return (h ^ b) * kPrime; };
    auto mix16 = [&mix](uint64_t h, uint16_t v) {
        h = mix(h, static_cast<uint8_t>(v & 0xFF));
        return mix(h, static_cast<uint8_t>((v >> 8) & 0xFF));
    };

    for (size_t i = 0; i < m_entries.size();)
    {
        const uint8_t group = m_entries[i].group;
        uint64_t h = mix(kOffsetBasis, group);
        size_t j = i;
        for (; j < m_entries.size() && m_entries[j].group == group; ++j)
        {
            const Entry& e = m_entries[j];
            h = mix(h, e.binding);
            h = mix(h, static_cast<uint8_t>(e.kind));
            h = mix(h, e.stageMask);
            h = mix16(h, e.backendSlot[0]);
            h = mix16(h, e.backendSlot[1]);
            h = mix16(h, e.backendSlot[2]);
            h = mix(h, static_cast<uint8_t>(e.textureViewDim));
            h = mix(h, static_cast<uint8_t>(e.textureSampleType));
            h = mix(h, e.textureMultisampled ? 1u : 0u);
        }
        // 0 is the "no baked id" sentinel, so never hand it out.
        m_groupLayouts.push_back({group, h == kNoLayoutId ? 1ull : h});
        i = j;
    }
}

void BindingMap::finalize()
{
    std::sort(m_entries.begin(),
              m_entries.end(),
              [](const Entry& a, const Entry& b) {
                  if (a.group != b.group)
                      return a.group < b.group;
                  return a.binding < b.binding;
              });
    m_finalized = true;
}

#endif // WITH_RIVE_TOOLS

} // namespace rive::ore
