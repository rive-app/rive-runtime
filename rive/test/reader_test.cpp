#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "reader.h"

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

TEST_CASE("signed int leb decoder", "[reader]")
{
	int64_t result;

	uint8_t encoded1[] = {0x01};
	decode_int_leb(encoded1, encoded1 + 1, &result);
	REQUIRE(result == 1);

	uint8_t encoded15[] = {0x0F};
	decode_int_leb(encoded15, encoded15 + 1, &result);
	REQUIRE(result == 15);

	uint8_t encodedMultiByte[] = {0xE5, 0x8E, 0x26};
	decode_int_leb(encodedMultiByte, encodedMultiByte + 3, &result);
	REQUIRE(result == 624485);

	uint8_t encodedNegativeMultiByte[] = {0xC0, 0xBB, 0x78};
	decode_int_leb(encodedNegativeMultiByte, encodedNegativeMultiByte + 3, &result);
	REQUIRE(result == -123456);
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
}

TEST_CASE("double decoder", "[reader]")
{
	double decoded_num;
	uint64_t bytes_read;

	uint8_t num_bytes_100[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x59, 0x40};
	bytes_read = decode_double(num_bytes_100, num_bytes_100 + 8, &decoded_num);
	REQUIRE(decoded_num == 100);
	REQUIRE(bytes_read == 8);

	uint8_t num_bytes_pi[] = {0x6E, 0x86, 0x1B, 0xF0, 0xF9, 0x21, 0x09, 0x40};
	bytes_read = decode_double(num_bytes_pi, num_bytes_pi + 8, &decoded_num);
	REQUIRE(decoded_num == 3.14159);
	REQUIRE(bytes_read == 8);

	uint8_t num_bytes_neg_euler[] = {0x96, 0xB4, 0xE2, 0x1B, 0x0A, 0xBF, 0x05, 0xC0};
	bytes_read = decode_double(num_bytes_neg_euler, num_bytes_neg_euler + 8, &decoded_num);
	REQUIRE(decoded_num == -2.718281);
	REQUIRE(bytes_read == 8);

	bytes_read = decode_double(num_bytes_neg_euler, num_bytes_neg_euler + 7, &decoded_num);
	REQUIRE(bytes_read == 0);
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