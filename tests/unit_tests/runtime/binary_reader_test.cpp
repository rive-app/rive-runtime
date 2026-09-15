#include <catch.hpp>
#include "rive/core/binary_reader.hpp"
#include "rive/core/vector_binary_writer.hpp"

template <typename T> void checkFits()
{
    int64_t min = std::numeric_limits<T>::min();
    int64_t max = std::numeric_limits<T>::max();
    REQUIRE(rive::fitsIn<T>(max + 0));
    REQUIRE(rive::fitsIn<T>(min - 0));
    REQUIRE(!rive::fitsIn<T>(max + 1));
    REQUIRE(!rive::fitsIn<T>(min - 1));
}

TEST_CASE("fitsIn checks", "[type_conversions]")
{
    checkFits<int8_t>();
    checkFits<uint8_t>();

    checkFits<int16_t>();
    checkFits<uint16_t>();

    checkFits<int32_t>();
    checkFits<uint32_t>();
}

static uint8_t* packvarint(uint8_t array[], uint64_t value)
{
    while (value > 127)
    {
        *array++ = static_cast<uint8_t>(0x80 | (value & 0x7F));
        value >>= 7;
    }
    *array++ = static_cast<uint8_t>(value);
    return array;
}

template <typename T> bool checkAs(uint64_t value)
{
    uint8_t storage[16];
    uint8_t* p = storage;

    p = packvarint(storage, value);
    rive::BinaryReader reader(rive::make_span(storage, p - storage));

    auto newValue = reader.readVarUintAs<T>();

    if (reader.hasError())
    {
        REQUIRE(newValue == 0);
    }

    return !reader.hasError() && value == newValue;
}

TEST_CASE("range checks", "[binary_reader]")
{
    REQUIRE(checkAs<uint8_t>(100));
    REQUIRE(checkAs<uint16_t>(100));
    REQUIRE(checkAs<uint32_t>(100));

    REQUIRE(!checkAs<uint8_t>(1000));
    REQUIRE(checkAs<uint16_t>(1000));
    REQUIRE(checkAs<uint32_t>(1000));

    REQUIRE(!checkAs<uint8_t>(100000));
    REQUIRE(!checkAs<uint16_t>(100000));
    REQUIRE(checkAs<uint32_t>(100000));
}

TEST_CASE("all data types can be written & read", "[binary_reader]")
{
    std::vector<uint8_t> buffer;
    rive::VectorBinaryWriter writer(&buffer);
    writer.writeVarUint((uint32_t)34);
    writer.write((uint16_t)22);
    writer.write(3.14f);

    rive::BinaryReader reader(buffer);
    REQUIRE(reader.readVarUintAs<uint32_t>() == 34);
    REQUIRE(reader.readUint16() == 22);
    REQUIRE(reader.readFloat32() == 3.14f);
}

// The byte-length prefix for a readBytes() blob is decoded straight from the
// file, so a corrupt or truncated file can ask for more bytes than the buffer
// holds. readBytes() must never hand back a Span that points past the end of
// the backing buffer: callers build a sub-reader over that Span (e.g. data
// bind source paths, mesh index/vertex buffers, blob assets) and would
// otherwise read out of bounds.
TEST_CASE("readBytes overflows on a length past the end of the buffer",
          "[binary_reader]")
{
    const uint8_t storage[] = {1, 2, 3, 4};
    rive::BinaryReader reader(rive::make_span(storage, sizeof(storage)));

    auto bytes = reader.readBytes(1000);

    REQUIRE(reader.didOverflow());
    REQUIRE(bytes.size() == 0);
    // The span must stay within the source buffer.
    REQUIRE(bytes.begin() >= storage);
    REQUIRE(bytes.end() <= storage + sizeof(storage));
}

TEST_CASE("readBytes returns exactly the in-range bytes requested",
          "[binary_reader]")
{
    const uint8_t storage[] = {1, 2, 3, 4};
    rive::BinaryReader reader(rive::make_span(storage, sizeof(storage)));

    auto bytes = reader.readBytes(3);
    REQUIRE(!reader.didOverflow());
    REQUIRE(bytes.size() == 3);
    REQUIRE(bytes[0] == 1);
    REQUIRE(bytes[2] == 3);

    // Only one byte remains; asking for more must overflow, not overrun.
    auto rest = reader.readBytes(100);
    REQUIRE(reader.didOverflow());
    REQUIRE(rest.size() == 0);
    REQUIRE(rest.end() <= storage + sizeof(storage));
}
