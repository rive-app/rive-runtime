#define CATCH_CONFIG_MAIN // This tells Catch to provide a main() - only do this in one cpp file
#include "catch.hpp"
#include "file.hpp"
#include "core/binary_reader.hpp"

TEST_CASE("file can be read", "[file]")
{
    FILE* fp = fopen("../../assets/copy_paste_test.riv", "r");
    REQUIRE(fp != nullptr);

   fseek(fp, 0, SEEK_END);
    auto length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    uint8_t* bytes = new uint8_t[length];
    REQUIRE(fread(bytes, 1, length, fp) == length);
    auto reader = rive::BinaryReader(bytes, length);
	rive::File* file = rive::File::import(reader);

	REQUIRE(file != nullptr);

    delete [] bytes;
}