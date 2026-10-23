#pragma once

#include "rive/renderer/ore/ore_binding_map.hpp"
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
  // What the entry samples; older bakes leave it unknown.
  std::vector<TextureSamplerPair> pairs;
  bool pairsKnown = false;

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
  for (const RstbEntryBindings& e : entries)
  {
    if (!e.pairsKnown || e.pairs.size() > 255)
      return out;
  }
  // Then each entry's pairs, u8 n, n*{u8 p, p*{4 x u8}}, which older readers
  // never reach; left out where they would overflow the section's u16 length.
  const size_t bindingsEnd = out.size();
  out.push_back(static_cast<uint8_t>(entries.size()));
  for (const RstbEntryBindings& e : entries)
  {
    out.push_back(static_cast<uint8_t>(e.pairs.size()));
    for (const TextureSamplerPair& p : e.pairs)
    {
      out.insert(
          out.end(),
          {p.textureGroup, p.textureBinding, p.samplerGroup, p.samplerBinding});
    }
  }
  if (out.size() > 0xFFFF)
    out.resize(bindingsEnd);
  return out;
}

// Each entry's pairs from the block after the bindings; all or none.
inline void decodeEntryPairs(
    const uint8_t* p,
    const uint8_t* end,
    std::vector<RstbEntryBindings>& entries)
{
  if (p >= end || static_cast<size_t>(*p++) != entries.size())
    return;
  std::vector<std::vector<TextureSamplerPair>> pairs(entries.size());
  for (std::vector<TextureSamplerPair>& entryPairs : pairs)
  {
    if (p >= end)
      return;
    const uint8_t n = *p++;
    if (p + static_cast<size_t>(n) * 4 > end)
      return;
    for (uint8_t k = 0; k < n; k++, p += 4)
      entryPairs.push_back({p[0], p[1], p[2], p[3]});
  }
  for (size_t i = 0; i < entries.size(); i++)
  {
    entries[i].pairs = std::move(pairs[i]);
    entries[i].pairsKnown = true;
  }
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
  decodeEntryPairs(p, end, out);
  return true;
}

} // namespace ore
} // namespace rive
