#pragma once

#include "rive/renderer/ore/ore_rstb_entry_container.hpp"

#include <cstdint>
#include <string>
#include <vector>

// RSTB section 2: what each entry point statically binds, by name. Wire:
// u8 n, n*{u8 stage, u16 len + name, u8 m, m*{u8 group, u8 binding}}.

namespace rive
{
namespace ore
{

static constexpr uint8_t kRstbEntryBindingsTag = 2;

struct RstbEntryBindings
{
  uint8_t stage = 0; // 0=vertex 1=fragment 2=compute
  std::string name;
  // group << 8 | binding, in declaration order.
  std::vector<uint16_t> bindings;

  bool uses(uint32_t group, uint32_t binding) const
  {
    const uint16_t key = static_cast<uint16_t>(group << 8 | binding);
    for (uint16_t b : bindings)
    {
      if (b == key)
        return true;
    }
    return false;
  }
};

// Empty on overflow, which readers take as no per entry use.
inline std::vector<uint8_t> encodeEntryBindings(
    const std::vector<RstbEntryBindings>& entries)
{
  std::vector<uint8_t> out;
  if (entries.empty() || entries.size() > 255)
    return out;
  out.push_back(static_cast<uint8_t>(entries.size()));
  for (const RstbEntryBindings& e : entries)
  {
    if (e.bindings.size() > 255 || e.name.size() > 0xFFFF)
      return {};
    out.push_back(e.stage);
    rstb_detail::putStr(out, e.name);
    out.push_back(static_cast<uint8_t>(e.bindings.size()));
    for (uint16_t b : e.bindings)
    {
      out.push_back(static_cast<uint8_t>(b >> 8));
      out.push_back(static_cast<uint8_t>(b & 0xFF));
    }
  }
  return out;
}

inline bool decodeEntryBindings(
    const uint8_t* data,
    size_t size,
    std::vector<RstbEntryBindings>& out)
{
  out.clear();
  if (data == nullptr || size == 0)
    return false;
  const uint8_t* p = data;
  const uint8_t* end = data + size;
  const uint8_t count = *p++;
  out.reserve(count);
  for (uint8_t i = 0; i < count; i++)
  {
    RstbEntryBindings e;
    if (p >= end)
      break;
    e.stage = *p++;
    if (!rstb_detail::getStr(p, end, e.name) || p >= end)
      break;
    const uint8_t n = *p++;
    if (p + static_cast<size_t>(n) * 2 > end)
      break;
    e.bindings.reserve(n);
    for (uint8_t k = 0; k < n; k++)
    {
      e.bindings.push_back(static_cast<uint16_t>(p[0] << 8 | p[1]));
      p += 2;
    }
    out.push_back(std::move(e));
  }
  if (out.size() != count)
  {
    out.clear();
    return false;
  }
  return true;
}

} // namespace ore
} // namespace rive
