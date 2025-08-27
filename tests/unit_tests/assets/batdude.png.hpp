#include "rive/span.hpp"

namespace assets
{
constexpr static unsigned int batdude_png_len = 33714;
extern unsigned char batdude_png_data[batdude_png_len];
inline static rive::Span<uint8_t> batdude_png()
{
    return {batdude_png_data, batdude_png_len};
}
}; // namespace assets
