#ifndef _RIVE_STABLE_ID_HPP_
#define _RIVE_STABLE_ID_HPP_
#include <cstdint>

namespace rive
{
/// A number minted on the first ask and kept for the holder's life, which no
/// other holder in the process gets short of four billion asks. Never 0.
class StableId
{
public:
  uint32_t get();

private:
  uint32_t m_value = 0;
};
} // namespace rive

#endif
