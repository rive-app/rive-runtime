#include "rive/span.hpp"

namespace assets
{
constexpr static unsigned int roboto_flex_ttf_data_len = 1654412;
extern unsigned char roboto_flex_ttf_data[roboto_flex_ttf_data_len];
inline static rive::Span<uint8_t> roboto_flex_ttf()
{
    return {roboto_flex_ttf_data, roboto_flex_ttf_data_len};
}
}; // namespace assets
