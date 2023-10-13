#ifndef _RIVE_FILE_READER_HPP_
#define _RIVE_FILE_READER_HPP_

#include <rive/file.hpp>

#include "rive_testing.hpp"
#include "utils/no_op_factory.hpp"

static rive::NoOpFactory gNoOpFactory;

static inline std::unique_ptr<rive::File> ReadRiveFile(const char path[],
                                                       rive::Factory* factory = nullptr,
                                                       rive::FileAssetLoader* loader = nullptr,
                                                       bool loadInBandAssets = true)
{
    if (!factory)
    {
        factory = &gNoOpFactory;
    }

    FILE* fp = fopen(path, "rb");
    REQUIRE(fp != nullptr);

    fseek(fp, 0, SEEK_END);
    const size_t length = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    std::vector<uint8_t> bytes(length);
    REQUIRE(fread(bytes.data(), 1, length, fp) == length);
    fclose(fp);

    rive::ImportResult result;
    auto file = rive::File::import(bytes, factory, &result, loader);
    REQUIRE(result == rive::ImportResult::success);
    REQUIRE(file.get() != nullptr);
    REQUIRE(file->artboard() != nullptr);

    return file;
}

#endif
