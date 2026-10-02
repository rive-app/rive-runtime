#include "rive/core/binary_data_reader.hpp"
#include "rive/core/reader.h"
#include "rive/file.hpp"
#include "rive/nested_artboard.hpp"
#include "rive/viewmodel/viewmodel.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>
#include <algorithm>

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

// Scripts can now import bytes from anywhere (context:decodeFile), so crafted
// files have to fail as malformed rather than crash. The cases below each
// used to abort, overflow the stack, or loop until out of memory.

namespace
{
// kAsset with the length prefix of its first artboard's name replaced by
// `encodedLength` (a varuint as written in the file).
std::vector<uint8_t> withNameLength(const std::vector<uint8_t>& encodedLength)
{
    std::vector<uint8_t> bytes = ReadFile(kAsset);
    auto file = ReadRiveFile(kAsset);
    std::string name = file->artboardNameAt(0);
    REQUIRE(!name.empty());
    REQUIRE(name.size() < 128); // one-byte varuint length in the file
    std::vector<uint8_t> needle;
    needle.push_back((uint8_t)name.size());
    needle.insert(needle.end(), name.begin(), name.end());
    auto at =
        std::search(bytes.begin(), bytes.end(), needle.begin(), needle.end());
    REQUIRE(at != bytes.end());
    size_t offset = (size_t)(at - bytes.begin());
    std::vector<uint8_t> patched(bytes.begin(), bytes.begin() + offset);
    patched.insert(patched.end(), encodedLength.begin(), encodedLength.end());
    patched.insert(patched.end(), bytes.begin() + offset + 1, bytes.end());
    return patched;
}

rive::ImportResult importResult(const std::vector<uint8_t>& bytes)
{
    rive::ImportResult result = rive::ImportResult::success;
    auto file = rive::File::import(bytes, &gNoOpFactory, &result, nullptr);
    CHECK((file == nullptr) == (result != rive::ImportResult::success));
    return result;
}
} // namespace

TEST_CASE("string lengths past the end of the file are refused",
          "[file][malformed]")
{
    // 2^40: used to be allocated up front, which aborts without exceptions.
    CHECK(importResult(withNameLength({0x80, 0x80, 0x80, 0x80, 0x80, 0x20})) ==
          rive::ImportResult::malformed);
    // UINT64_MAX: length + 1 used to wrap to a 0-byte buffer that the whole
    // string was then copied into.
    CHECK(importResult(withNameLength(
              {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01})) ==
          rive::ImportResult::malformed);
}

TEST_CASE("an over-long varuint is refused", "[file][malformed]")
{
    // Eleven continuation bytes: the shift used to run past 63 bits, which
    // is undefined.
    CHECK(importResult(withNameLength({0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x80,
                                       0x01})) ==
          rive::ImportResult::malformed);
}

TEST_CASE("a varuint with bits past 64 is refused", "[file][malformed]")
{
    // The artboard name's own length, spread over ten groups: bit 64 in the
    // last one used to fall off the shift and leave a valid file behind.
    size_t length = ReadRiveFile(kAsset)->artboardNameAt(0).size();
    REQUIRE(length < 128);
    uint8_t first = (uint8_t)(length | 0x80);
    CHECK(importResult(withNameLength(
              {first, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x02})) ==
          rive::ImportResult::malformed);
    // The same ten groups without the stray bit still import.
    CHECK(importResult(withNameLength(
              {first, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x00})) ==
          rive::ImportResult::success);
}

TEST_CASE("a 32 bit varuint with bits past 31 is refused", "[file][malformed]")
{
    uint8_t overflowing[] = {0x80, 0x80, 0x80, 0x80, 0x10};
    rive::BinaryDataReader reader(overflowing, sizeof(overflowing));
    reader.readVarUint32();
    CHECK(reader.didOverflow());
    uint8_t largest[] = {0xFF, 0xFF, 0xFF, 0xFF, 0x0F};
    rive::BinaryDataReader max(largest, sizeof(largest));
    CHECK(max.readVarUint32() == 0xFFFFFFFFu);
    CHECK_FALSE(max.didOverflow());
}

TEST_CASE("the largest 64 bit varuint decodes", "[file][malformed]")
{
    uint8_t largest[] =
        {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01};
    uint64_t value = 0;
    CHECK(decode_uint_leb(largest, largest + sizeof(largest), &value) ==
          sizeof(largest));
    CHECK(value == UINT64_MAX);
}

TEST_CASE("a nested artboard cycle loads without the references closing it",
          "[file][malformed]")
{
    // Main and Widget nest each other. Instancing either recursed until the
    // stack overflowed.
    std::vector<uint8_t> bytes = ReadFile("assets/nested_artboard_cycle.riv");
    auto sources = [](const std::vector<uint8_t>& bytes) {
        rive::ImportResult result;
        auto file = rive::File::import(bytes, &gNoOpFactory, &result, nullptr);
        REQUIRE(result == rive::ImportResult::success);
        REQUIRE(file->artboardCount() == 2);
        std::vector<rive::Artboard*> referenced;
        for (size_t i = 0; i < file->artboardCount(); i++)
        {
            auto nested = file->artboard(i)->find<rive::NestedArtboard>();
            REQUIRE(nested.size() == 1);
            referenced.push_back(nested[0]->sourceArtboard());
            CHECK(file->artboardAt(i) != nullptr);
        }
        return std::make_pair(file, referenced);
    };
    auto mutual = sources(bytes);
    CHECK(mutual.second[0] == nullptr);
    CHECK(mutual.second[1] == nullptr);

    // Widget's NestedArtboard (type 92) with its artboardId (key 197) moved
    // from Main to Widget: Main's reference is no longer on the cycle.
    const uint8_t widgetNestsMain[] = {0x5C, 0xC5, 0x01, 0x00};
    auto at = std::search(bytes.begin(),
                          bytes.end(),
                          std::begin(widgetNestsMain),
                          std::end(widgetNestsMain));
    REQUIRE(at != bytes.end());
    at[3] = 0x01;
    auto self = sources(bytes);
    CHECK(self.second[0] == self.first->artboard(1));
    CHECK(self.second[1] == nullptr);
}

TEST_CASE("a view model that contains itself creates a finite instance",
          "[file][malformed]")
{
    // Built by rive-cli from:
    //   <ViewModel name="Node" id="0:40">
    //       <ViewModelPropertyNumber name="value" id="0:42"/>
    //       <ViewModelPropertyViewModel viewModelReferenceId="0:40"
    //                                   name="child" id="0:41"/>
    //   </ViewModel>
    // Creating a blank instance recursed until the stack overflowed.
    auto file = ReadRiveFile("assets/viewmodel_self_reference.riv");
    auto viewModel = file->viewModel("Node");
    REQUIRE(viewModel != nullptr);
    auto instance = file->createViewModelInstance(viewModel);
    REQUIRE(instance != nullptr);
    auto child = instance->propertyValue("child");
    REQUIRE(child != nullptr);
    CHECK(child->as<rive::ViewModelInstanceViewModel>()
              ->referenceViewModelInstance() == nullptr);
    CHECK(file->createViewModelInstance(file->artboard()) != nullptr);
}

namespace
{
// The instance a view model property holds, or null when it holds none.
rive::ViewModelInstance* nested(rive::ViewModelInstance* instance,
                                const std::string& name)
{
    auto value = instance->propertyValue(name);
    if (value == nullptr)
    {
        return nullptr;
    }
    return value->as<rive::ViewModelInstanceViewModel>()
        ->referenceViewModelInstance()
        .get();
}
} // namespace

TEST_CASE("a view model chain deeper than an authored file stops nesting",
          "[file][malformed]")
{
    // Built by rive-cli from 300 view models, VM0 to VM299, each one's
    // "child" holding the next. A crafted file can make such a chain long
    // enough to overflow the stack, so nesting stops at 256 instances.
    auto file = ReadRiveFile("assets/viewmodel_deep_chain.riv");
    auto instance = file->createViewModelInstance(file->viewModel("VM0"));
    REQUIRE(instance != nullptr);
    size_t depth = 1;
    for (auto* at = nested(instance.get(), "child"); at != nullptr;
         at = nested(at, "child"))
    {
        depth++;
    }
    CHECK(depth == 256);
}

TEST_CASE("a view model graph that fans out stops at an instance budget",
          "[file][malformed]")
{
    // Built by rive-cli from 24 view models, L0 to L23, each one's "a" and
    // "b" both holding the next: 2^24 - 1 instances in full, a count a
    // crafted file can push past any memory. Creation stops at the budget,
    // a small one here, since the default's 100000 instances are slow under
    // the sanitizers.
    auto file = ReadRiveFile("assets/viewmodel_fan_out.riv");
    auto instance =
        file->createBoundedViewModelInstance(file->viewModel("L0"), 1000);
    REQUIRE(instance != nullptr);
    size_t count = 0;
    std::vector<rive::ViewModelInstance*> pending = {instance.get()};
    while (!pending.empty())
    {
        auto* at = pending.back();
        pending.pop_back();
        count++;
        for (const char* name : {"a", "b"})
        {
            if (auto* next = nested(at, name))
            {
                pending.push_back(next);
            }
        }
    }
    CHECK(count == 1000);
}

TEST_CASE("copying an authored instance chain stops nesting",
          "[file][malformed]")
{
    // Built by rive-cli from a view model Node whose "child" holds a Node,
    // with 300 authored instances, each one's child the next. The artboard's
    // default instance copies that chain, which a crafted file can make deep
    // enough to overflow the stack, so the copy stops at 256.
    auto file = ReadRiveFile("assets/viewmodel_instance_chain.riv");
    auto instance = file->createDefaultViewModelInstance(file->artboard());
    REQUIRE(instance != nullptr);
    size_t depth = 1;
    for (auto* at = nested(instance.get(), "child"); at != nullptr;
         at = nested(at, "child"))
    {
        depth++;
    }
    CHECK(depth == 256);
    // Completing the file's own instances walks the same chain, and stops at
    // the same depth.
    file->completeViewModelProperties(file->viewModel("Node")->instance(0));
}
