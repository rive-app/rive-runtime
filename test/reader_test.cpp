#include <catch.hpp>
#include <rive/core/reader.h>

TEST_CASE("uint leb decoder", "[reader]")
{
    uint64_t result;

    uint8_t encoded1[] = {0x01};
    decode_uint_leb(encoded1, encoded1 + 1, &result);
    REQUIRE(result == 1);

    uint8_t encoded15[] = {0x0F};
    decode_uint_leb(encoded15, encoded15 + 1, &result);
    REQUIRE(result == 15);

    uint8_t encodedMultiByte[] = {0xE5, 0x8E, 0x26};
    decode_uint_leb(encodedMultiByte, encodedMultiByte + 3, &result);
    REQUIRE(result == 624485);
}

TEST_CASE("string decoder", "[reader]")
{
    char* str = strdup("New Artboard");
    uint8_t str_bytes[] = {0x4E, 0x65, 0x77, 0x20, 0x41, 0x72, 0x74, 0x62, 0x6F, 0x61, 0x72, 0x64};
    // Length of string + 1 is needed to add the null terminator
    char* decoded_str = (char*)malloc((13) * sizeof(char));
    uint64_t bytes_read = decode_string(12, str_bytes, str_bytes + 12, decoded_str);
    REQUIRE(strcmp(str, decoded_str) == 0);
    REQUIRE(bytes_read == 12);

    bytes_read = decode_string(12, str_bytes, str_bytes + 11, decoded_str);
    REQUIRE(bytes_read == 0);
    delete str;
    delete decoded_str;
}

TEST_CASE("float decoder", "[reader]")
{
    float decoded_num;
    uint64_t bytes_read;

    uint8_t num_bytes_100[] = {0x00, 0x00, 0xC8, 0x42};
    bytes_read = decode_float(num_bytes_100, num_bytes_100 + 4, &decoded_num);
    REQUIRE(decoded_num == 100.0f);
    REQUIRE(bytes_read == 4);

    uint8_t num_bytes_pi[] = {0xD0, 0x0F, 0x49, 0x40};
    bytes_read = decode_float(num_bytes_pi, num_bytes_pi + 4, &decoded_num);
    REQUIRE(decoded_num == 3.14159f);
    REQUIRE(bytes_read == 4);

    uint8_t num_bytes_neg_euler[] = {0x51, 0xF8, 0x2D, 0xC0};
    bytes_read = decode_float(num_bytes_neg_euler, num_bytes_neg_euler + 4, &decoded_num);
    REQUIRE(decoded_num == -2.718281f);
    REQUIRE(bytes_read == 4);

    bytes_read = decode_float(num_bytes_neg_euler, num_bytes_neg_euler + 3, &decoded_num);
    REQUIRE(bytes_read == 0);
}

TEST_CASE("byte decoder", "[reader")
{
    uint8_t decoded_byte;
    // luigi: commented this out as it was giving a warning (I added -Wall to
    // the compiler flags). Warning was:
    //
    // ../../rive/test/reader_test.cpp:108:11: warning: unused variable
    // 'bytes_read' [-Wunused-variable]
    //
    // uint64_t bytes_read;
    uint8_t pos = 0;
    uint8_t bytes[] = {0x00, 0x00, 0xC8, 0x42};

    pos += decode_uint_8(bytes + pos, bytes + 4, &decoded_byte);
    REQUIRE(decoded_byte == bytes[pos - 1]);
    pos += decode_uint_8(bytes + pos, bytes + 4, &decoded_byte);
    REQUIRE(decoded_byte == bytes[pos - 1]);
    pos += decode_uint_8(bytes + pos, bytes + 4, &decoded_byte);
    REQUIRE(decoded_byte == bytes[pos - 1]);
    pos += decode_uint_8(bytes + pos, bytes + 4, &decoded_byte);
    REQUIRE(decoded_byte == bytes[pos - 1]);
    // overflow
    REQUIRE(decode_uint_8(bytes + pos, bytes + 4, &decoded_byte) == 0);
}
