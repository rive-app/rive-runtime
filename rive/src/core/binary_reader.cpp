#include "core/binary_reader.hpp"
#include "core/reader.h"

using namespace rive;

BinaryReader::BinaryReader(uint8_t* bytes, size_t length) : m_Position(bytes), m_End(bytes+length), m_Overflowed(false), m_Length(length)
{

}

size_t BinaryReader::lengthInBytes() const 
{
    return m_Length;
}

bool BinaryReader::didOverflow() const 
{
    return m_Overflowed;
}

void BinaryReader::overflow() 
{
    m_Overflowed = true;
    m_Position = m_End;
}

uint64_t BinaryReader::readVarUint() 
{
    uint64_t value;
    auto readBytes = decode_uint_leb(m_Position, m_End, &value);
    if(readBytes == 0) 
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}


int64_t BinaryReader::readVarInt() 
{
    int64_t value;
    auto readBytes = decode_int_leb(m_Position, m_End, &value);
    if(readBytes == 0) 
    {
        overflow();
        return 0;
    }
    m_Position += readBytes;
    return value;
}

std::string BinaryReader::readString() 
{
    uint64_t length = readVarUint();
    if(didOverflow())
    {
        return std::string();
    }

    char rawValue[length];
    auto readBytes = decode_string(length, m_Position, m_End, &rawValue[0]);
    if(readBytes != length) 
    {
        overflow();
        return std::string();
    }
    m_Position += readBytes;
    return std::string(rawValue);
}

double BinaryReader::readFloat64()
{
    double value;
    auto readBytes = decode_double(m_Position, m_End, &value);
    if(readBytes == 0) 
    {
        overflow();
        return 0.0;
    }
    m_Position += readBytes;
    return value;
}

float BinaryReader::readFloat32()
{
    float value;
    auto readBytes = decode_float(m_Position, m_End, &value);
    if(readBytes == 0) 
    {
        overflow();
        return 0.0f;
    }
    m_Position += readBytes;
    return value;
}

uint8_t BinaryReader::readByte()
{
    if (m_End - m_Position < 1)
    {
        overflow();
		return 0;
    }
    return *m_Position++;
}

BinaryReader BinaryReader::read(size_t length)
{
    if (m_End - m_Position < length)
    {
        overflow();
		return BinaryReader(m_Position, 0);
    }
    auto readerPosition = m_Position;
    m_Position += length;
    return BinaryReader(readerPosition, length);
}