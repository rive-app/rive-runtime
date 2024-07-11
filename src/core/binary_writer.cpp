#include "rive/core/binary_writer.hpp"
#include "rive/core/binary_stream.hpp"
#include "rive/core/reader.h"
#include <stdio.h>

using namespace rive;

BinaryWriter::BinaryWriter(BinaryStream* stream) : m_Stream(stream) {}
BinaryWriter::~BinaryWriter() { m_Stream->flush(); }

void BinaryWriter::write(float value)
{
    auto bytes = reinterpret_cast<uint8_t*>(&value);
    if (is_big_endian())
    {
        uint8_t backwards[4] = {bytes[3], bytes[2], bytes[1], bytes[0]};
        m_Stream->write(backwards, 4);
    }
    else
    {
        m_Stream->write(bytes, 4);
    }
}

void BinaryWriter::writeFloat(float value)
{
    auto bytes = reinterpret_cast<uint8_t*>(&value);
    if (is_big_endian())
    {
        uint8_t backwards[4] = {bytes[3], bytes[2], bytes[1], bytes[0]};
        m_Stream->write(backwards, 4);
    }
    else
    {
        m_Stream->write(bytes, 4);
    }
}

void BinaryWriter::write(double value)
{
    auto bytes = reinterpret_cast<uint8_t*>(&value);
    if (is_big_endian())
    {
        uint8_t backwards[8] =
            {bytes[7], bytes[6], bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0]};
        m_Stream->write(backwards, 8);
    }
    else
    {
        m_Stream->write(bytes, 8);
    }
}

void BinaryWriter::writeVarUint(uint64_t value)
{
    uint8_t buffer[16];
    int index = 0;
    do
    {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (value != 0)
        {
            // more bytes follow
            byte |= 0x80;
        }

        buffer[index++] = byte;
    } while (value != 0);
    m_Stream->write(buffer, index);
}

void BinaryWriter::writeVarUint(uint32_t value)
{
    uint8_t buffer[16];
    int index = 0;
    do
    {
        uint8_t byte = value & 0x7f;
        value >>= 7;

        if (value != 0)
        {
            // more bytes follow
            byte |= 0x80;
        }

        buffer[index++] = byte;
    } while (value != 0);
    m_Stream->write(buffer, index);
}

void BinaryWriter::write(const uint8_t* bytes, size_t length)
{
    if (length == 0)
    {
        return;
    }
    m_Stream->write(bytes, length);
}

void BinaryWriter::writeDouble(double value)
{
    auto bytes = reinterpret_cast<uint8_t*>(&value);
    if (is_big_endian())
    {
        uint8_t backwards[8] =
            {bytes[7], bytes[6], bytes[5], bytes[4], bytes[3], bytes[2], bytes[1], bytes[0]};
        m_Stream->write(backwards, 8);
    }
    else
    {
        m_Stream->write(bytes, 8);
    }
}

void BinaryWriter::write(uint8_t value) { m_Stream->write((const uint8_t*)&value, 1); }

void BinaryWriter::write(uint16_t value) { m_Stream->write((const uint8_t*)&value, 2); }

void BinaryWriter::write(uint32_t value) { m_Stream->write((const uint8_t*)&value, 4); }

void BinaryWriter::clear() { m_Stream->clear(); }