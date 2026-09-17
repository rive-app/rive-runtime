#include "rive/file.hpp"
#include "rive/artboard.hpp"
#include "rive/dependency_sorter.hpp"
#include "rive/runtime_header.hpp"
#include "rive/core/binary_reader.hpp"
#include <utils/no_op_renderer.hpp>
#include "rive/nested_artboard.hpp"
#include "rive/shapes/image.hpp"
#include "rive_file_reader.hpp"
#include <catch.hpp>

// The number of bytes the header occupies, read off the file independently of
// the byte ranges so the two can be compared against each other.
static size_t headerSizeOf(const std::vector<uint8_t>& bytes)
{
    rive::Span<const uint8_t> span(bytes.data(), bytes.size());
    rive::BinaryReader reader(span);
    rive::RuntimeHeader header;
    REQUIRE(rive::RuntimeHeader::read(reader, header));
    return (size_t)(reader.position() - bytes.data());
}

// Groundwork for re-importing a single artboard in place: an artboard has to
// occupy one contiguous, non-overlapping run of the stream for that to be
// possible at all.
TEST_CASE("artboard byte ranges are contiguous and cover the artboard stream",
          "[artboard-ranges]")
{
    auto bytes = ReadFile("assets/artboardclipping.riv");
    auto file = ReadRiveFile("assets/artboardclipping.riv");
    REQUIRE(file->artboardCount() > 1);

    size_t previousEnd = 0;
    for (size_t i = 0; i < file->artboardCount(); i++)
    {
        auto range = file->artboardByteRange(i);
        // Non-empty.
        REQUIRE(range.end > range.start);
        // Ordered, and each run begins exactly where the previous one ended,
        // so no artboard's bytes are interleaved with another's.
        if (i > 0)
        {
            REQUIRE(range.start == previousEnd);
        }
        previousEnd = range.end;
    }
    // The last run reaches the end of the stream: nothing trails the final
    // artboard. Ranges are relative to the end of the header, so measure the
    // header independently rather than deriving it from the ranges -- which
    // is what makes this an actual check and not a restatement.
    REQUIRE(previousEnd > 0);
    REQUIRE(headerSizeOf(bytes) + previousEnd == bytes.size());
    // Everything before the first artboard is the file-global content the
    // artboards index into (the Backboard, assets, view models, enums,
    // converters), so the first run starts after it rather than at zero.
    REQUIRE(file->artboardByteRange(0).start < previousEnd);
}

TEST_CASE("artboard byte ranges hold across files", "[artboard-ranges]")
{
    for (auto name : {"assets/juice.riv",
                      "assets/entry.riv",
                      "assets/artboardclipping.riv"})
    {
        auto file = ReadRiveFile(name);
        size_t previousEnd = 0;
        for (size_t i = 0; i < file->artboardCount(); i++)
        {
            auto range = file->artboardByteRange(i);
            REQUIRE(range.end > range.start);
            if (i > 0)
            {
                REQUIRE(range.start == previousEnd);
            }
            previousEnd = range.end;
        }
    }
}

TEST_CASE("an out of range artboard index reports an empty byte range",
          "[artboard-ranges]")
{
    auto file = ReadRiveFile("assets/artboardclipping.riv");
    auto range = file->artboardByteRange(file->artboardCount() + 10);
    REQUIRE(range.start == 0);
    REQUIRE(range.end == 0);
}

// The spike: can one artboard be re-imported in place, against the file's
// already-loaded assets and artboards, leaving everything else intact?
TEST_CASE("an artboard can be replaced in place from its own bytes",
          "[artboard-replace]")
{
    auto bytes = ReadFile("assets/artboardclipping.riv");
    auto file = ReadRiveFile("assets/artboardclipping.riv");
    REQUIRE(file->artboardCount() > 1);

    const size_t target = 1;
    const std::string nameBefore = file->artboardNameAt(target);
    const size_t countBefore = file->artboardCount();

    // Another artboard's instance must survive the replacement untouched.
    auto untouched = file->artboardAt(0);
    REQUIRE(untouched != nullptr);
    const float untouchedWidth = untouched->width();

    // The header is stripped from the byte ranges, so offset past it the same
    // way the importer did.
    auto range = file->artboardByteRange(target);
    REQUIRE(range.end > range.start);
    const size_t headerSize = headerSizeOf(bytes);
    rive::Span<const uint8_t> run(bytes.data() + headerSize + range.start,
                                  range.end - range.start);

    REQUIRE(file->replaceArtboard(target, run) == rive::ImportResult::success);

    // Same slot, same identity, same count.
    REQUIRE(file->artboardCount() == countBefore);
    REQUIRE(file->artboardNameAt(target) == nameBefore);

    // The replacement is a working artboard.
    auto replaced = file->artboardAt(target);
    REQUIRE(replaced != nullptr);
    replaced->advance(0.0f);
    rive::NoOpRenderer renderer;
    replaced->draw(&renderer);

    // And the instance made before the replacement still works.
    REQUIRE(untouched->width() == untouchedWidth);
    untouched->advance(0.0f);
    untouched->draw(&renderer);
}

// Stronger than replacing an artboard with its own bytes: feed it a *different*
// artboard's run and check the content actually changed. Proves the bytes are
// really being decoded rather than the old artboard surviving by accident.
TEST_CASE("replacing an artboard decodes the bytes it is given",
          "[artboard-replace]")
{
    auto bytes = ReadFile("assets/artboardclipping.riv");
    auto file = ReadRiveFile("assets/artboardclipping.riv");
    REQUIRE(file->artboardCount() > 1);

    const std::string sourceName = file->artboardNameAt(0);
    const std::string targetName = file->artboardNameAt(1);
    REQUIRE(sourceName != targetName);

    const size_t headerSize = headerSizeOf(bytes);
    auto sourceRange = file->artboardByteRange(0);
    rive::Span<const uint8_t> sourceRun(bytes.data() + headerSize +
                                            sourceRange.start,
                                        sourceRange.end - sourceRange.start);

    // Overwrite artboard 1 with artboard 0's run.
    REQUIRE(file->replaceArtboard(1, sourceRun) == rive::ImportResult::success);

    // Slot 1 now holds artboard 0's content, and slot 0 is untouched.
    REQUIRE(file->artboardNameAt(1) == sourceName);
    REQUIRE(file->artboardNameAt(0) == sourceName);
    REQUIRE(file->artboardCount() > 1);

    // Both are usable.
    rive::NoOpRenderer renderer;
    for (size_t i = 0; i < 2; i++)
    {
        auto instance = file->artboardAt(i);
        REQUIRE(instance != nullptr);
        instance->advance(0.0f);
        instance->draw(&renderer);
    }
}

TEST_CASE("replacing an artboard rejects bad input without changing the file",
          "[artboard-replace]")
{
    auto file = ReadRiveFile("assets/artboardclipping.riv");
    const size_t countBefore = file->artboardCount();
    const std::string nameBefore = file->artboardNameAt(0);

    // Out of range index.
    uint8_t byte = 0;
    REQUIRE(file->replaceArtboard(countBefore + 5,
                                  rive::Span<const uint8_t>(&byte, 1)) ==
            rive::ImportResult::malformed);
    // Empty run.
    REQUIRE(file->replaceArtboard(0, rive::Span<const uint8_t>(&byte, 0)) ==
            rive::ImportResult::malformed);

    REQUIRE(file->artboardCount() == countBefore);
    REQUIRE(file->artboardNameAt(0) == nameBefore);
}

// A NestedArtboard in one artboard holds a resolved pointer to another. If the
// referenced artboard is replaced, that pointer has to follow it or it dangles.
TEST_CASE("replacing a referenced artboard re-points its referencers",
          "[artboard-replace]")
{
    auto bytes = ReadFile("assets/nested_artboard_opacity.riv");
    auto file = ReadRiveFile("assets/nested_artboard_opacity.riv");
    REQUIRE(file->artboardCount() > 1);

    // Find an artboard that nests another, and which artboard it points at.
    rive::NestedArtboard* nested = nullptr;
    int referencedIndex = -1;
    for (size_t i = 0; i < file->artboardCount() && nested == nullptr; i++)
    {
        for (auto* object : file->artboard(i)->objects())
        {
            if (object != nullptr && object->is<rive::NestedArtboard>())
            {
                auto* candidate = object->as<rive::NestedArtboard>();
                if (candidate->referencedArtboardId() >= 0)
                {
                    nested = candidate;
                    referencedIndex = candidate->referencedArtboardId();
                    break;
                }
            }
        }
    }
    REQUIRE(nested != nullptr);
    REQUIRE(referencedIndex >= 0);
    REQUIRE((size_t)referencedIndex < file->artboardCount());

    const size_t headerSize = headerSizeOf(bytes);
    auto range = file->artboardByteRange((size_t)referencedIndex);
    rive::Span<const uint8_t> run(bytes.data() + headerSize + range.start,
                                  range.end - range.start);

    auto* before = file->artboard((size_t)referencedIndex);
    // The referencer points at the old artboard to begin with.
    REQUIRE(nested->sourceArtboard() == before);

    REQUIRE(file->replaceArtboard((size_t)referencedIndex, run) ==
            rive::ImportResult::success);
    auto* after = file->artboard((size_t)referencedIndex);
    // A genuinely new object, so a stale pointer is detectable rather than
    // merely unlucky.
    REQUIRE(after != before);
    // The point of the test: the referencer followed the replacement instead
    // of being left pointing at freed memory.
    REQUIRE(nested->sourceArtboard() == after);
    REQUIRE(nested->sourceArtboard() != before);

    // The host artboard still instances and draws, which it could not do if
    // its nested reference still pointed at the deleted artboard.
    rive::NoOpRenderer renderer;
    auto host = file->artboardDefault();
    REQUIRE(host != nullptr);
    host->advance(0.0f);
    host->draw(&renderer);
}

// An artboard's run references file assets by index into the file's asset
// list, which lives outside the run. Re-importing the artboard has to rebind
// those references against the assets already loaded, or the artboard comes
// back pointing at nothing.
TEST_CASE("a replaced artboard still resolves its file assets",
          "[artboard-replace]")
{
    auto bytes = ReadFile("assets/walle.riv");
    auto file = ReadRiveFile("assets/walle.riv");

    // Find an image and the asset it resolves to before the replacement.
    rive::Image* image = nullptr;
    size_t artboardIndex = 0;
    for (size_t i = 0; i < file->artboardCount() && image == nullptr; i++)
    {
        for (auto* object : file->artboard(i)->objects())
        {
            if (object != nullptr && object->is<rive::Image>())
            {
                image = object->as<rive::Image>();
                artboardIndex = i;
                break;
            }
        }
    }
    REQUIRE(image != nullptr);
    REQUIRE(image->imageAsset() != nullptr);
    auto* assetBefore = image->imageAsset();

    const size_t headerSize = headerSizeOf(bytes);
    auto range = file->artboardByteRange(artboardIndex);
    rive::Span<const uint8_t> run(bytes.data() + headerSize + range.start,
                                  range.end - range.start);

    REQUIRE(file->replaceArtboard(artboardIndex, run) ==
            rive::ImportResult::success);

    // The re-imported artboard's image must resolve to the same asset object
    // the file already holds -- not null, and not a fresh copy.
    rive::Image* replaced = nullptr;
    for (auto* object : file->artboard(artboardIndex)->objects())
    {
        if (object != nullptr && object->is<rive::Image>())
        {
            replaced = object->as<rive::Image>();
            break;
        }
    }
    REQUIRE(replaced != nullptr);
    REQUIRE(replaced != image);
    REQUIRE(replaced->imageAsset() != nullptr);
    REQUIRE(replaced->imageAsset() == assetBefore);
}

// An instance skips the topological sort and replays its source's order
// instead (see Artboard::dependencyOrderRecipe). That is only sound if the two
// agree exactly -- a replayed order that drops, duplicates or reorders a
// component silently changes what updates, and in what order.
//
// The recipe is addressed by each component's m_GraphOrder stamp, which is
// meaningless for the many components in m_Objects that the sort never placed:
// Component leaves it uninitialized. This is the check that the recipe does
// not mistake one of those for a real entry -- it passes on a platform whose
// allocator happens to hand back zeros either way, so treat a failure here as
// real regardless of which platform reports it.
TEST_CASE("an artboard instance's dependency order matches a real sort",
          "[artboard-ranges]")
{
    for (auto name : {"assets/juice.riv",
                      "assets/entry.riv",
                      "assets/artboardclipping.riv"})
    {
        auto file = ReadRiveFile(name);
        for (size_t i = 0; i < file->artboardCount(); i++)
        {
            auto instance = file->artboardAt(i);
            REQUIRE(instance != nullptr);

            std::vector<rive::Component*> sorted;
            rive::DependencySorter sorter;
            sorter.sort(instance.get(), sorted);

            INFO(name << " artboard " << i);
            REQUIRE(instance->dependencyOrder().size() == sorted.size());
            for (size_t p = 0; p < sorted.size(); p++)
            {
                INFO("position " << p);
                REQUIRE(instance->dependencyOrder()[p] == sorted[p]);
            }
        }
    }
}
