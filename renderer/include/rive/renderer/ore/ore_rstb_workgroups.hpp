#pragma once

#include "rive/renderer/ore/ore_rstb_entry_container.hpp"

#include <cstdint>
#include <string>
#include <vector>

// RSTB section 4: each compute entry point's workgroup, by name. Wire:
// u8 n, n*{u16 len + name, 3*u32 size, u8 m, m*u32 memory}.

namespace rive
{
namespace ore
{

static constexpr uint8_t kRstbWorkgroupsTag = 4;

struct RstbWorkgroup
{
  std::string name;
  uint32_t size[3] = {1, 1, 1};
  // Bytes of each var<workgroup> the entry uses, in the order Metal numbers
  // their threadgroup memory, which takes each rounded up to 16.
  std::vector<uint32_t> memory;
};

// Empty on overflow.
inline std::vector<uint8_t> encodeWorkgroups(
    const std::vector<RstbWorkgroup>& workgroups)
{
  std::vector<uint8_t> out;
  if (workgroups.empty() || workgroups.size() > 255)
    return out;
  out.push_back(static_cast<uint8_t>(workgroups.size()));
  for (const RstbWorkgroup& w : workgroups)
  {
    if (w.memory.size() > 255 || w.name.size() > 0xFFFF)
      return {};
    rstb_detail::putStr(out, w.name);
    for (uint32_t dimension : w.size)
      rstb_detail::putU32(out, dimension);
    out.push_back(static_cast<uint8_t>(w.memory.size()));
    for (uint32_t bytes : w.memory)
      rstb_detail::putU32(out, bytes);
  }
  return out;
}

inline bool decodeWorkgroups(
    const uint8_t* data,
    size_t size,
    std::vector<RstbWorkgroup>& out)
{
  out.clear();
  if (data == nullptr || size == 0)
    return false;
  const uint8_t* p = data;
  const uint8_t* end = data + size;
  const uint8_t count = *p++;
  // Whole or nothing, so a cut section never passes as a shorter one.
  std::vector<RstbWorkgroup> decoded;
  for (uint8_t i = 0; i < count; i++)
  {
    RstbWorkgroup w;
    if (!rstb_detail::getStr(p, end, w.name))
      return false;
    for (uint32_t& dimension : w.size)
    {
      if (!rstb_detail::getU32(p, end, dimension))
        return false;
    }
    if (p == end)
      return false;
    w.memory.resize(*p++);
    for (uint32_t& bytes : w.memory)
    {
      if (!rstb_detail::getU32(p, end, bytes))
        return false;
    }
    decoded.push_back(std::move(w));
  }
  out = std::move(decoded);
  return true;
}

} // namespace ore
} // namespace rive
