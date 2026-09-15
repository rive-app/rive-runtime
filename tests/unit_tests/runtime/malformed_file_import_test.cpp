#include "rive/file.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// Importing a damaged Rive file (one that was truncated in transit, cached
// partially, or corrupted on disk) must never crash or corrupt the heap. The
// importer is expected to either succeed or return ImportResult::malformed and
// a null File -- nothing in between.
//
// This guards two memory-safety fixes in the import path:
//
//   * File::m_backboard is now initialized to null. If a read fails before a
//     Backboard object has been parsed, ~File() runs `delete m_backboard`; an
//     uninitialized pointer there is a free() of a garbage address, which
//     corrupts the allocator and later crashes at an unrelated malloc/new.
//
//   * BinaryReader::readBytes() now treats an over-long file-controlled length
//     as overflow and returns an empty Span anchored at the end of the buffer,
//     so a sub-reader built over the returned Span cannot read past the end.
//
// Run under AddressSanitizer to catch the heap violations these fixes prevent:
//   ./test.sh clean asan -m "[malformed]"

// A representative valid file. A data-binding file is used on purpose: its
// data binds carry length-prefixed source-path blobs, so truncating through
// them also exercises BinaryReader::readBytes().
static const char* kAsset = "assets/data_binding_test_2.riv";

TEST_CASE("truncated file import never crashes", "[file][malformed]")
{
    std::vector<uint8_t> bytes = ReadFile(kAsset);

    // Import every prefix of the file, including the empty prefix and the full
    // file. Each must resolve cleanly and the returned File (when the import
    // fails) must destruct without touching uninitialized state.
    for (size_t length = 0; length <= bytes.size(); length++)
    {
        std::vector<uint8_t> truncated(bytes.begin(), bytes.begin() + length);
        rive::ImportResult result;
        auto file =
            rive::File::import(truncated, &gNoOpFactory, &result, nullptr);
        if (result == rive::ImportResult::success)
        {
            REQUIRE(file != nullptr);
        }
        else
        {
            REQUIRE(file == nullptr);
        }
    }
}

TEST_CASE("full file still imports after the guards", "[file][malformed]")
{
    auto file = ReadRiveFile(kAsset);
    REQUIRE(file->artboard() != nullptr);
}
