#pragma once

#include "rive/renderer/ore/ore_rstb_entry_container.hpp"

#include <cstdint>
#include <string>
#include <vector>

// RSTB section 3: each bound global's WGSL name, for refusals to name.
// Wire: u8 n, n*{u8 group, u8 binding, u16 len + name}.

namespace rive
{
namespace ore
{

static constexpr uint8_t kRstbBindingNamesTag = 3;

struct RstbBindingName
{
  uint8_t group = 0;
  uint8_t binding = 0;
  std::string name;
};

// Empty on overflow, which readers take as no names.
inline std::vector<uint8_t> encodeBindingNames(
    const std::vector<RstbBindingName>& names)
{
  std::vector<uint8_t> out;
  if (names.empty() || names.size() > 255)
    return out;
  out.push_back(static_cast<uint8_t>(names.size()));
  for (const RstbBindingName& n : names)
  {
    if (n.name.size() > 0xFFFF)
      return {};
    out.push_back(n.group);
    out.push_back(n.binding);
    rstb_detail::putStr(out, n.name);
  }
  return out;
}

inline bool decodeBindingNames(
    const uint8_t* data,
    size_t size,
    std::vector<RstbBindingName>& out)
{
  out.clear();
  if (data == nullptr || size == 0)
    return false;
  const uint8_t* p = data;
  const uint8_t* end = data + size;
  const uint8_t count = *p++;
  for (uint8_t i = 0; i < count; i++)
  {
    if (p + 2 > end)
      return false;
    RstbBindingName n;
    n.group = *p++;
    n.binding = *p++;
    if (!rstb_detail::getStr(p, end, n.name))
      return false;
    out.push_back(std::move(n));
  }
  return true;
}

} // namespace ore
} // namespace rive
