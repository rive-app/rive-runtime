#include "rive/span.hpp"

namespace assets
{
constexpr static unsigned int montserrat_ttf_data_len = 394140;
extern unsigned char montserrat_ttf_data[montserrat_ttf_data_len];
inline static rive::Span<uint8_t> montserrat_ttf()
{
    return {montserrat_ttf_data, montserrat_ttf_data_len};
}
}; // namespace assets
