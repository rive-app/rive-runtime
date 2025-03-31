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
