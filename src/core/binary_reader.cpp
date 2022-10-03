#include "rive/core/binary_reader.hpp"
#include "rive/core/reader.h"
#include "rive/span.hpp"
#include <vector>

using namespace rive;

BinaryReader::BinaryReader(Span<const uint8_t> bytes) :
    m_Bytes(bytes), m_Position(bytes.begin()), m_Overflowed(false), m_IntRangeError(false)
{}

bool BinaryReader::reachedEnd() const
{
    return m_Position == m_Bytes.end() || didOverflow() || didIntRangeError();
}

size_t BinaryReader::lengthInBytes() const { return m_Bytes.size(); }
const uint8_t* BinaryReader::position() const { return m_Position; }

bool BinaryReader::didOverflow() const { return m_Overflowed; }

bool BinaryReader::didIntRangeError() const { return m_IntRangeError; }

void BinaryReader::overflow()
{
    m_Overflowed = true;
    m_Position = m_Bytes.end();
}

void BinaryReader::intRangeError()
{
    m_IntRangeError = true;
    m_Position = m_Bytes.end();
}

uint64_t BinaryReader::readVarUint64()
{
    uint64_t value;
    auto readBytes = decode_uint_leb(m_Position, m_Bytes.end(), &value);
    if (readBytes == 0)
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}

std::string BinaryReader::readString()
{
    uint64_t length = readVarUint64();
    if (didOverflow())
    {
        return std::string();
    }

    std::vector<char> rawValue((size_t)length + 1);
    auto readBytes = decode_string(length, m_Position, m_Bytes.end(), &rawValue[0]);
    if (readBytes != length)
    {
        overflow();
        return std::string();
    }
    m_Position += readBytes;
    return std::string(rawValue.data(), (size_t)length);
}

Span<const uint8_t> BinaryReader::readBytes()
{
    uint64_t length = readVarUint64();
    if (didOverflow())
    {
        return Span<const uint8_t>(m_Position, 0);
    }

    const uint8_t* start = m_Position;
    m_Position += length;
    return {start, (size_t)length};
}

float BinaryReader::readFloat32()
{
    float value;
    auto readBytes = decode_float(m_Position, m_Bytes.end(), &value);
    if (readBytes == 0)
    {
        overflow();
        return 0.0f;
    }
    m_Position += readBytes;
    return value;
}

uint8_t BinaryReader::readByte()
{
    if (m_Bytes.end() - m_Position < 1)
    {
        overflow();
        return 0;
    }
    return *m_Position++;
}

uint32_t BinaryReader::readUint32()
{
    uint32_t value;
    auto readBytes = decode_uint_32(m_Position, m_Bytes.end(), &value);
    if (readBytes == 0)
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}
