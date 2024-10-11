#include "rive/span.hpp"

namespace assets
{
constexpr static unsigned int nomoon_png_len = 71766;
extern unsigned char nomoon_png_data[nomoon_png_len];
inline static rive::Span<uint8_t> nomoon_png()
{
    return {nomoon_png_data, nomoon_png_len};
}
} // namespace assets
