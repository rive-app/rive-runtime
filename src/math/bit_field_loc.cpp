#include "rive/math/bit_field_loc.hpp"
#include <cassert>

using namespace rive;

BitFieldLoc::BitFieldLoc(uint32_t start, uint32_t end) : m_start(start)
{
    assert(end >= start);
    assert(end < 32);

    m_count = end - start + 1;
    m_mask = ((1 << (end - start + 1)) - 1) << start;
}

uint32_t BitFieldLoc::read(uint32_t bits) { return (bits & m_mask) >> m_start; }

uint32_t BitFieldLoc::write(uint32_t bits, uint32_t value)
{
    return (bits & ~m_mask) | ((value << m_start) & m_mask);
}