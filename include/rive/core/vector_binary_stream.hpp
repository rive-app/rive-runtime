#ifndef _RIVE_CORE_VECTOR_BINARY_STREAM_HPP_
#define _RIVE_CORE_VECTOR_BINARY_STREAM_HPP_

#include "rive/core/binary_stream.hpp"
#include "rive/span.hpp"
#include <vector>

namespace rive
{
// A BinaryStream implementation that writes to an in-memory
// std::vector<uint8_t>. Useful for building binary data that will be read back
// later.
class VectorBinaryStream : public BinaryStream
{
public:
    void write(const uint8_t* bytes, std::size_t length) override
    {
        m_memory.insert(m_memory.end(), bytes, bytes + length);
    }

    void flush() override {}

    void clear() override { m_memory.clear(); }

    Span<uint8_t> memory()
    {
        return Span<uint8_t>(m_memory.data(), m_memory.size());
    }

    const std::vector<uint8_t>& data() const { return m_memory; }

private:
    std::vector<uint8_t> m_memory;
};
} // namespace rive

#endif
