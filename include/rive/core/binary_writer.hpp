#ifndef _RIVE_CORE_BINARY_WRITER_HPP_
#define _RIVE_CORE_BINARY_WRITER_HPP_

#include <cstddef>
#include <cstdint>

namespace rive
{
class BinaryStream;
class BinaryWriter
{
private:
    BinaryStream* m_Stream;

public:
    BinaryWriter(BinaryStream* stream);
    ~BinaryWriter();
    void write(float value);
    void writeFloat(float value);
    void write(double value);
    void writeVarUint(uint64_t value);
    void writeVarUint(uint32_t value);
    void write(const uint8_t* bytes, std::size_t length);
    void write(uint8_t value);
    void writeDouble(double value);
    void write(uint16_t value);
    void write(uint32_t value);
    void clear();
};
} // namespace rive

#endif