#include "rive/span.hpp"

namespace assets
{
constexpr static unsigned int paper_riv_data_len = 2403906;
extern unsigned char paper_riv_data[paper_riv_data_len];
inline static rive::Span<uint8_t> paper_riv()
{
    return {paper_riv_data, paper_riv_data_len};
}
}; // namespace assets
