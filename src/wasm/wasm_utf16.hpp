#ifndef _RIVE_WASM_UTF16_HPP_
#define _RIVE_WASM_UTF16_HPP_

#include <cstdint>

namespace rive
{
/// The code point at byte i of a module's UTF-16, a surrogate pair as one,
/// moving i past it; i + 1 must be below bytes. A lone surrogate stands for
/// itself, as the module's own encoder wrote it.
inline uint32_t nextUtf16(const uint8_t* data, uint32_t bytes, uint32_t& i)
{
  uint32_t code = data[i] | (data[i + 1] << 8);
  i += 2;
  if (code >= 0xd800 && code < 0xdc00 && i + 1 < bytes)
  {
    uint32_t low = data[i] | (data[i + 1] << 8);
    if (low >= 0xdc00 && low < 0xe000)
    {
      code = 0x10000 + ((code - 0xd800) << 10) + (low - 0xdc00);
      i += 2;
    }
  }
  return code;
}

/// Writes a code point's UTF-8, at most 4 bytes, answering how many.
inline uint32_t utf8Of(uint32_t code, uint8_t* out)
{
  if (code < 0x80)
  {
    out[0] = (uint8_t)code;
    return 1;
  }
  if (code < 0x800)
  {
    out[0] = (uint8_t)(0xc0 | (code >> 6));
    out[1] = (uint8_t)(0x80 | (code & 0x3f));
    return 2;
  }
  if (code < 0x10000)
  {
    out[0] = (uint8_t)(0xe0 | (code >> 12));
    out[1] = (uint8_t)(0x80 | ((code >> 6) & 0x3f));
    out[2] = (uint8_t)(0x80 | (code & 0x3f));
    return 3;
  }
  out[0] = (uint8_t)(0xf0 | (code >> 18));
  out[1] = (uint8_t)(0x80 | ((code >> 12) & 0x3f));
  out[2] = (uint8_t)(0x80 | ((code >> 6) & 0x3f));
  out[3] = (uint8_t)(0x80 | (code & 0x3f));
  return 4;
}
} // namespace rive

#endif
