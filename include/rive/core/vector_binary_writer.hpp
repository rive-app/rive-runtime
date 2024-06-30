#ifndef _RIVE_CORE_VECTOR_BINARY_WRITER_HPP_
#define _RIVE_CORE_VECTOR_BINARY_WRITER_HPP_

#include "rive/core/binary_stream.hpp"
#include "rive/core/binary_writer.hpp"
#include <cstring>

namespace rive
{
class VectorBinaryWriter : public BinaryStream, public BinaryWriter
{
private:
    std::vector<uint8_t>* m_WriteBuffer;
    std::size_t m_Start;
    size_t m_pos = 0;

public:
    VectorBinaryWriter(std::vector<uint8_t>* buffer) :
        BinaryWriter(this), m_WriteBuffer(buffer), m_Start(m_WriteBuffer->size())
    {}

    uint8_t* buffer() const { return &(*m_WriteBuffer)[m_Start]; }
    std::size_t bufferSize() const { return m_WriteBuffer->size() - m_Start; }

    std::size_t start() const { return m_Start; }
    size_t size() const { return m_pos; }

    using BinaryWriter::write;
    void write(const uint8_t* bytes, std::size_t length) override
    {
        auto end = m_pos;
        if (m_WriteBuffer->size() < end + length)
        {
            m_WriteBuffer->resize(end + length);
        }
        std::memcpy(&((*m_WriteBuffer)[end]), bytes, length);
        m_pos += length;
    }
    void flush() override {}
    void clear() override { m_pos = 0; }
};
} // namespace rive
#endif