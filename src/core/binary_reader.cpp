#include "rive/core/binary_reader.hpp"
#include "rive/core/reader.h"
#include <vector>

using namespace rive;

BinaryReader::BinaryReader(uint8_t* bytes, size_t length) :
    m_Position(bytes),
    m_End(bytes + length),
    m_Overflowed(false),
    m_Length(length)
{
}

bool BinaryReader::reachedEnd() const
{
	return m_Position == m_End || didOverflow();
}

size_t BinaryReader::lengthInBytes() const { return m_Length; }

bool BinaryReader::didOverflow() const { return m_Overflowed; }

void BinaryReader::overflow()
{
	m_Overflowed = true;
	m_Position = m_End;
}

uint64_t BinaryReader::readVarUint64()
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

std::string BinaryReader::readString()
{
	uint64_t length = readVarUint64();
	if (didOverflow())
	{
		return std::string();
	}

	std::vector<char> rawValue(length + 1);
	auto readBytes = decode_string(length, m_Position, m_End, &rawValue[0]);
	if (readBytes != length)
	{
		overflow();
		return std::string();
	}
	m_Position += readBytes;
	return std::string(rawValue.data(), length);
}

std::vector<uint8_t> BinaryReader::readBytes()
{
	uint64_t length = readVarUint64();
	if (didOverflow())
	{
		return std::vector<uint8_t>();
	}

	uint8_t* start = m_Position;
	m_Position += length;
	return std::vector<uint8_t>(start, start + length);
}

double BinaryReader::readFloat64()
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

float BinaryReader::readFloat32()
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

uint8_t BinaryReader::readByte()
{
	if (m_End - m_Position < 1)
	{
		overflow();
		return 0;
	}
	return *m_Position++;
}

uint32_t BinaryReader::readUint32()
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
