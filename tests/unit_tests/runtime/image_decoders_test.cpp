#include "rive_file_reader.hpp"
#include "rive_testing.hpp"
#include "rive/decoders/bitmap_decoder.hpp"

TEST_CASE("png file decodes correctly", "[image-decoder]")
{
    auto file = ReadFile("assets/placeholder.png");
    REQUIRE(file.size() == 1096);

    auto bitmap = Bitmap::decode(file.data(), file.size());

    REQUIRE(bitmap != nullptr);

    REQUIRE(bitmap->width() == 226);
    REQUIRE(bitmap->height() == 128);
}

TEST_CASE("jpeg file decodes correctly", "[image-decoder]")
{
    auto file = ReadFile("assets/open_source.jpg");
    REQUIRE(file.size() == 8880);

    auto bitmap = Bitmap::decode(file.data(), file.size());

    REQUIRE(bitmap != nullptr);

    REQUIRE(bitmap->width() == 350);
    REQUIRE(bitmap->height() == 200);
}

#ifndef __APPLE__
// Loading this particular jpeg image in CG causes a memory leak
// CGImageSourceCreateImageAtIndex calls IIOReadPlugin::createInfoPtr which
// leaks
TEST_CASE("bad jpeg file doesn't cause an overflow", "[image-decoder]")
{
    auto file = ReadFile("assets/bad.jpg");
    REQUIRE(file.size() == 88731);

    auto bitmap = Bitmap::decode(file.data(), file.size());

    REQUIRE(bitmap != nullptr);

    REQUIRE(bitmap->width() == 24566);
    REQUIRE(bitmap->height() == 58278);
}
#endif

TEST_CASE("bad png file doesn't cause an overflow", "[image-decoder]")
{
    auto file = ReadFile("assets/bad.png");
    REQUIRE(file.size() == 534283);

    auto bitmap = Bitmap::decode(file.data(), file.size());

#ifdef __APPLE__
    // Loading this bad PNG file in CG actually works and we do get an image
    // albiet black
    REQUIRE(bitmap != nullptr);

    REQUIRE(bitmap->width() == 58278);
    REQUIRE(bitmap->height() == 24566);
#else
    // Our decoders return null as we have an invalid header with bogus
    // resolution and we want to avoid a potential attack vector
    REQUIRE(bitmap == nullptr);
#endif
}

TEST_CASE("webp file decodes correctly", "[image-decoder]")
{
    auto file = ReadFile("assets/1.webp");
    REQUIRE(file.size() == 30320);

    auto bitmap = Bitmap::decode(file.data(), file.size());

    REQUIRE(bitmap != nullptr);

    REQUIRE(bitmap->width() == 550);
    REQUIRE(bitmap->height() == 368);
}