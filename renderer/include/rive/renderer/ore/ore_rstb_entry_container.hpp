#pragma once

#include <cstdint>
#include <string>
#include <vector>

// RSTB v4 source-variant containers. Every source variant (WGSL/GLSL/MSL/HLSL/
// SPIR-V) carries a self-describing entry table so the runtime resolves
// @vertex/@fragment by name (WebGPU-style) instead of assuming vs_main/fs_main.
//
//   logical  - the WGSL entry-point name a script matches against.
//   physical - the name the driver looks up: == logical for WGSL/SPIR-V (naga
//              writes them verbatim), the naga-emitted name for MSL (its Namer
//              sanitizes MSL keywords), "main" for GLSL, the SPIRV-Cross
//              cleansed name for HLSL.
//
// Two shapes. Whole-module targets (WGSL=0, MSL=2, SPIR-V=5) share one source:
//   u8 count, count*{u8 stage, str logical, str physical}, u32 srcLen, src
// Per-entry targets (GLSL=1, HLSL=3) carry a source per entry:
//   u8 count, count*{u8 stage, str logical, str physical, u32 srcLen, src}
// where str is u16 len + bytes (no NUL). Order is naga declaration order, so
// the first vertex/fragment entry is the WebGPU "no entryPoint" default.
//
// This header is the single source of truth shared by the producer
// (scripting_workspace, GM bake) and consumer (lua_gpu, GM helper).

namespace rive
{
namespace ore
{

// ---------------------------------------------------------------------------
// Encode (producer)
// ---------------------------------------------------------------------------
struct RstbEntry
{
    uint8_t stage = 0; // 0=vertex 1=fragment 2=compute
    std::string logical;
    std::string physical;
    std::vector<uint8_t> source; // per-entry targets only
};

namespace rstb_detail
{
inline void putU16(std::vector<uint8_t>& b, uint16_t v)
{
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
}
inline void putU32(std::vector<uint8_t>& b, uint32_t v)
{
    b.push_back(v & 0xFF);
    b.push_back((v >> 8) & 0xFF);
    b.push_back((v >> 16) & 0xFF);
    b.push_back((v >> 24) & 0xFF);
}
inline void putStr(std::vector<uint8_t>& b, const std::string& s)
{
    putU16(b, static_cast<uint16_t>(s.size()));
    b.insert(b.end(), s.begin(), s.end());
}
} // namespace rstb_detail

inline std::vector<uint8_t> buildWholeModuleContainer(
    const std::vector<RstbEntry>& entries,
    const uint8_t* src,
    uint32_t srcLen)
{
    // count is a u8. A module with >255 entry points (pathological) would
    // truncate the count and corrupt the container — fail cleanly with an empty
    // blob, which callers treat as "skip this variant".
    if (entries.size() > 255)
        return {};
    std::vector<uint8_t> b;
    b.push_back(static_cast<uint8_t>(entries.size()));
    for (const auto& e : entries)
    {
        b.push_back(e.stage);
        rstb_detail::putStr(b, e.logical);
        rstb_detail::putStr(b, e.physical);
    }
    rstb_detail::putU32(b, srcLen);
    // Guard the raw-pointer range: src may be null when srcLen == 0, and
    // `src + 0` on a null pointer is undefined behavior (trips UBSan).
    if (srcLen > 0)
        b.insert(b.end(), src, src + srcLen);
    return b;
}

inline std::vector<uint8_t> buildPerEntryContainer(
    const std::vector<RstbEntry>& entries)
{
    if (entries.size() > 255)
        return {};
    std::vector<uint8_t> b;
    b.push_back(static_cast<uint8_t>(entries.size()));
    for (const auto& e : entries)
    {
        b.push_back(e.stage);
        rstb_detail::putStr(b, e.logical);
        rstb_detail::putStr(b, e.physical);
        rstb_detail::putU32(b, static_cast<uint32_t>(e.source.size()));
        b.insert(b.end(), e.source.begin(), e.source.end());
    }
    return b;
}

// ---------------------------------------------------------------------------
// Decode (consumer). Bounded readers — an adversarial blob returns false, never
// reads out of bounds. String fields are copied; source is a span into `blob`.
// ---------------------------------------------------------------------------
struct RstbEntryView
{
    uint8_t stage = 0;
    std::string logical;
    std::string physical;
    const uint8_t* source = nullptr; // per-entry targets; null for whole-module
    uint32_t sourceSize = 0;
};

namespace rstb_detail
{
inline bool getU16(const uint8_t*& p, const uint8_t* end, uint16_t& v)
{
    if (p + 2 > end)
        return false;
    v = static_cast<uint16_t>(p[0] | (p[1] << 8));
    p += 2;
    return true;
}
inline bool getU32(const uint8_t*& p, const uint8_t* end, uint32_t& v)
{
    if (p + 4 > end)
        return false;
    v = static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
        (static_cast<uint32_t>(p[2]) << 16) |
        (static_cast<uint32_t>(p[3]) << 24);
    p += 4;
    return true;
}
inline bool getStr(const uint8_t*& p, const uint8_t* end, std::string& s)
{
    uint16_t n;
    if (!getU16(p, end, n))
        return false;
    if (p + n > end)
        return false;
    s.assign(reinterpret_cast<const char*>(p), n);
    p += n;
    return true;
}
} // namespace rstb_detail

inline bool parseWholeModuleContainer(const uint8_t* blob,
                                      uint32_t size,
                                      std::vector<RstbEntryView>& out,
                                      const uint8_t** src,
                                      uint32_t* srcLen)
{
    // Reject null/empty input before any pointer arithmetic (blob + size on a
    // null pointer is undefined behavior, even when size == 0).
    if (blob == nullptr || size == 0 || src == nullptr || srcLen == nullptr)
        return false;
    const uint8_t* p = blob;
    const uint8_t* end = blob + size;
    uint8_t count = *p++;
    out.clear();
    out.reserve(count);
    for (uint8_t i = 0; i < count; i++)
    {
        if (p >= end)
            return false;
        RstbEntryView e;
        e.stage = *p++;
        if (e.stage > 2) // only 0=vertex, 1=fragment, 2=compute are defined
            return false;
        if (!rstb_detail::getStr(p, end, e.logical) ||
            !rstb_detail::getStr(p, end, e.physical))
            return false;
        out.push_back(std::move(e));
    }
    uint32_t n;
    if (!rstb_detail::getU32(p, end, n) || p + n > end)
        return false;
    *src = p;
    *srcLen = n;
    return true;
}

inline bool parsePerEntryContainer(const uint8_t* blob,
                                   uint32_t size,
                                   std::vector<RstbEntryView>& out)
{
    if (blob == nullptr || size == 0)
        return false;
    const uint8_t* p = blob;
    const uint8_t* end = blob + size;
    uint8_t count = *p++;
    out.clear();
    out.reserve(count);
    for (uint8_t i = 0; i < count; i++)
    {
        if (p >= end)
            return false;
        RstbEntryView e;
        e.stage = *p++;
        if (e.stage > 2) // only 0=vertex, 1=fragment, 2=compute are defined
            return false;
        if (!rstb_detail::getStr(p, end, e.logical) ||
            !rstb_detail::getStr(p, end, e.physical))
            return false;
        uint32_t n;
        if (!rstb_detail::getU32(p, end, n) || p + n > end)
            return false;
        e.source = p;
        e.sourceSize = n;
        p += n;
        out.push_back(std::move(e));
    }
    return true;
}

} // namespace ore
} // namespace rive