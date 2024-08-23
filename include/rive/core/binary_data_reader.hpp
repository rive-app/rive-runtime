#ifndef _RIVE_CORE_BINARY_DATA_READER_HPP_
#define _RIVE_CORE_BINARY_DATA_READER_HPP_

#include <string>
#include <stdint.h>

namespace rive
{
class BinaryDataReader
{
private:
    uint8_t* m_Position;
    uint8_t* m_End;
    bool m_Overflowed;
    size_t m_Length;

    void overflow();

public:
    BinaryDataReader(uint8_t* bytes, size_t length);
    bool didOverflow() const;
    bool isEOF() const { return m_Position >= m_End; }
    const uint8_t* position() const { return m_Position; }

    size_t lengthInBytes() const;

    uint64_t readVarUint();
    uint32_t readVarUint32();
    double readFloat64();
    float readFloat32();
    uint8_t readByte();
    uint32_t readUint32();
    void complete(uint8_t* bytes, size_t length);
    void reset(uint8_t* bytes);
};
} // namespace rive

#endif