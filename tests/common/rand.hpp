#include <cassert>
#include <cstdint>
#include <random>

class Rand
{
public:
    void seed(uint64_t x) { m_impl.seed(x); }

    uint64_t u64() { return m_impl(); }
    uint32_t u32() { return static_cast<uint32_t>(m_impl()); }

    float f32(float start, float end)
    {
        // 24-bit mantissa drawn from the top bits of u64. Not strictly
        // unbiased: the affine map onto [start, end] introduces tiny
        // rounding non-uniformity when (end - start) isn't a dyadic
        // fraction, and for very wide intervals the result can round up
        // to exactly `end`. Acceptable for tests — the bias is below
        // float epsilon, and std::uniform_real_distribution had the same
        // boundary quirk anyway.
        float u = (m_impl() >> 40) * (1.0f / (1 << 24));
        return start + u * (end - start);
    }

    float f32(float end = 1) { return f32(0, end); }

    bool boolean() { return m_impl() & 1; }

    // This is based on Lemire's nearly-divisonless unbiased bounded random
    // integer algorithm:
    // https://lemire.me/blog/2019/06/06/nearly-divisionless-random-integer-generation-on-various-systems/
    uint32_t u32(uint32_t start, uint32_t end)
    {
        assert(start <= end);
        if (start == 0 && end == UINT32_MAX)
        {
            return u32();
        }
        uint32_t range = end - start + 1;
        uint64_t m = uint64_t(u32()) * range;
        uint32_t l = uint32_t(m);
        if (l < range)
        {
            uint32_t threshold =
                (0u - range) % range; // == (1ull << 32) % range
            while (l < threshold)
            {
                m = uint64_t(u32()) * range;
                l = uint32_t(m);
            }
        }
        return start + uint32_t(m >> 32);
    }

private:
    std::mt19937_64 m_impl;
};
