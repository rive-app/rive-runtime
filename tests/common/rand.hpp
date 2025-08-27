#include <random>

class Rand : public std::mt19937_64
{
public:
    void seed(uint64_t x) { m_impl.seed(x); }

    uint64_t u64() { return m_impl(); }
    uint32_t u32() { return static_cast<uint32_t>(m_impl()); }

    float f32(float start, float end)
    {
        return std::uniform_real_distribution<float>(start, end)(m_impl);
    }

    float f32(float end = 1) { return f32(0, end); }

    bool boolean() { return m_impl() & 1; }

    uint32_t u32(uint32_t start, uint32_t end)
    {
        return static_cast<uint32_t>(
            std::uniform_int_distribution<uint32_t>(start, end)(m_impl));
    }

private:
    std::mt19937_64 m_impl;
};
