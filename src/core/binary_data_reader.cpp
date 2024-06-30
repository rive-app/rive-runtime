#include "rive/core/binary_data_reader.hpp"
#include "rive/core/reader.h"

using namespace rive;

BinaryDataReader::BinaryDataReader(uint8_t* bytes, size_t length) :
    m_Position(bytes), m_End(bytes + length), m_Overflowed(false), m_Length(length)
{}

size_t BinaryDataReader::lengthInBytes() const { return m_Length; }

bool BinaryDataReader::didOverflow() const { return m_Overflowed; }

void BinaryDataReader::overflow()
{
    m_Overflowed = true;
    m_Position = m_End;
}

uint64_t BinaryDataReader::readVarUint()
{
    uint64_t value;
    auto readBytes = decode_uint_leb(m_Position, m_End, &value);
    if (readBytes == 0)
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}

uint32_t BinaryDataReader::readVarUint32()
{
    uint32_t value;
    auto readBytes = decode_uint_leb32(m_Position, m_End, &value);
    if (readBytes == 0)
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}

double BinaryDataReader::readFloat64()
{
    double value;
    auto readBytes = decode_double(m_Position, m_End, &value);
    if (readBytes == 0)
    {
        overflow();
        return 0.0;
    }
    m_Position += readBytes;
    return value;
}

float BinaryDataReader::readFloat32()
{
    float value;
    auto readBytes = decode_float(m_Position, m_End, &value);
    if (readBytes == 0)
    {
        overflow();
        return 0.0f;
    }
    m_Position += readBytes;
    return value;
}

uint8_t BinaryDataReader::readByte()
{
    if (m_End - m_Position < 1)
    {
        overflow();
        return 0;
    }
    return *m_Position++;
}

uint32_t BinaryDataReader::readUint32()
{
    uint32_t value;
    auto readBytes = decode_uint_32(m_Position, m_End, &value);
    if (readBytes == 0)
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}

void BinaryDataReader::complete(uint8_t* bytes, size_t length)
{
    m_Position = bytes;
    m_End = bytes + length;
    m_Length = length;
}

void BinaryDataReader::reset(uint8_t* bytes) { m_Position = bytes; }
