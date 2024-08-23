#ifndef _RIVE_BIT_FIELD_LOC_HPP_
#define _RIVE_BIT_FIELD_LOC_HPP_

#include <cmath>
#include <stdio.h>
#include <cstdint>
#include <tuple>
#include <vector>

namespace rive
{

class BitFieldLoc
{
public:
    BitFieldLoc(uint32_t start, uint32_t end);

    uint32_t read(uint32_t bits);
    uint32_t write(uint32_t bits, uint32_t value);

private:
    uint32_t m_start;
    uint32_t m_count;
    uint32_t m_mask;
};
} // namespace rive

#endif