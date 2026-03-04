#include "rive/renderer.hpp"
#include "rive/renderer/gpu.hpp"

#include <catch.hpp>

#undef assert

// This is what is thrown from TESTING_ASSERT so we can ensure we're catching
// the correct type.
struct TestAssertFailure
{};

thread_local bool tl_expects_assert = false;
// Define a special assert macro that the key builder will use in place of
// "assert" to check that the validation fires when it should.
#define TESTING_ASSERT(cond, message)                                          \
    if (!(cond))                                                               \
    {                                                                          \
        if (!tl_expects_assert)                                                \
        {                                                                      \
            /* add info about which assert failed to the output */             \
            UNSCOPED_INFO("" << message);                                      \
        }                                                                      \
        throw TestAssertFailure{};                                             \
    }

// Always enable sort key validation in sort_key_builder.hpp so that unit tests
// in release mode can still test the validation.
#define RIVE_ENABLE_SORT_KEY_VALIDATION 1

#include "../src/sort_key_builder.hpp"
#include <tuple>

namespace rive::gpu
{
// We can do a fair amount of validation at compile time!

// Static test the standard layout masks and shifts
constexpr auto BUILDER_1 = SortKeyBuilder{
    {.entry = SortEntry::drawGroup, .bitCount = 15},
    {.entry = SortEntry::drawType, .bitCount = 3},
    {.entry = SortEntry::textureHash, .bitCount = 14},
    {.entry = SortEntry::blendMode, .bitCount = 4},
    {.entry = SortEntry::drawContents, .bitCount = 9},
    {.entry = SortEntry::drawIndex, .bitCount = 15},
    {.entry = SortEntry::subpassIndex, .bitCount = 3},
};

static_assert(BUILDER_1.mask(SortEntry::subpassIndex) == 0x0000000000000007);
static_assert(BUILDER_1.shift(SortEntry::subpassIndex) == 0);
static_assert(BUILDER_1.shift(SortEntry::drawIndex) == 3);
static_assert(BUILDER_1.mask(SortEntry::drawIndex) == 0x000000000003FFF8);
static_assert(BUILDER_1.mask(SortEntry::drawContents) == 0x0000000007FC0000);
static_assert(BUILDER_1.shift(SortEntry::drawContents) == 18);
static_assert(BUILDER_1.mask(SortEntry::blendMode) == 0x0000000078000000);
static_assert(BUILDER_1.shift(SortEntry::blendMode) == 27);
static_assert(BUILDER_1.mask(SortEntry::textureHash) == 0x00001FFF80000000);
static_assert(BUILDER_1.shift(SortEntry::textureHash) == 31);
static_assert(BUILDER_1.mask(SortEntry::drawType) == 0x0000E00000000000);
static_assert(BUILDER_1.shift(SortEntry::drawType) == 45);
static_assert(BUILDER_1.mask(SortEntry::drawGroup) == 0x7fff000000000000);
static_assert(BUILDER_1.shift(SortEntry::drawGroup) == 48);

static_assert(BUILDER_1.buildPartialKey({{SortEntry::drawIndex, 1},
                                         {SortEntry::subpassIndex, 1}}) ==
              0x0000000000000009);
static_assert(BUILDER_1.buildKey({
                  {SortEntry::blendMode, 0},
                  {SortEntry::drawContents, 0},
                  {SortEntry::drawIndex, 0},
                  {SortEntry::drawGroup, 0},
                  {SortEntry::drawType, 0},
                  {SortEntry::subpassIndex, 0},
                  {SortEntry::textureHash, 0},
              }) == 0);

static_assert(BUILDER_1.buildKey({
                  {SortEntry::blendMode, (1 << 4) - 1},
                  {SortEntry::drawContents, (1 << 9) - 1},
                  {SortEntry::drawIndex, (1 << 15) - 1},
                  {SortEntry::drawGroup, (1 << 15) - 1},
                  {SortEntry::drawType, (1 << 3) - 1},
                  {SortEntry::subpassIndex, (1 << 3) - 1},
                  {SortEntry::textureHash, (1 << 14) - 1},
              }) == 0x7FFFFFFFFFFFFFFF);

static_assert(
    BUILDER_1.buildKey({
        {SortEntry::blendMode, 0},
        {SortEntry::drawContents, 0},
        {SortEntry::drawIndex, 0},
        {SortEntry::drawGroup, 0},
        {SortEntry::drawType, 0},
        {SortEntry::subpassIndex, 0},
        {SortEntry::textureHash, 0xFFFFFFFFFFFFFFFF, ValidateKeyEntry::no},
    }) == BUILDER_1.mask(SortEntry::textureHash));

// A somewhat more nonsensical builder
constexpr auto BUILDER_2 = SortKeyBuilder{
    {.entry = SortEntry::drawType, .bitCount = 4},
    {.entry = SortEntry::drawContents, .bitCount = 10},
    {.entry = SortEntry::drawIndex, .bitCount = 0},
    {.entry = SortEntry::blendMode, .bitCount = 6},
    {.entry = SortEntry::textureHash, .bitCount = 20},
    {.entry = SortEntry::drawGroup, .bitCount = 20},
    {.entry = SortEntry::subpassIndex, .bitCount = 0},
};

static_assert(BUILDER_2.mask(SortEntry::subpassIndex) == 0x0000000000000000);
static_assert(BUILDER_2.shift(SortEntry::subpassIndex) == 0);

static_assert(BUILDER_2.mask(SortEntry::drawGroup) == 0x00000000000FFFFF);
static_assert(BUILDER_2.shift(SortEntry::drawGroup) == 0);

static_assert(BUILDER_2.mask(SortEntry::textureHash) == 0x000000FFFFF00000);
static_assert(BUILDER_2.shift(SortEntry::textureHash) == 20);

static_assert(BUILDER_2.mask(SortEntry::blendMode) == 0x00003F0000000000);
static_assert(BUILDER_2.shift(SortEntry::blendMode) == 40);

// No matter the positioning in the list, a zero-bit-count entry should have no
// mask *and* no shift
static_assert(BUILDER_2.mask(SortEntry::drawIndex) == 0x0000000000000000);
static_assert(BUILDER_2.shift(SortEntry::drawIndex) == 0);

static_assert(BUILDER_2.mask(SortEntry::drawContents) == 0x00FFC00000000000);
static_assert(BUILDER_2.shift(SortEntry::drawContents) == 46);

static_assert(BUILDER_2.mask(SortEntry::drawType) == 0x0F00000000000000);
static_assert(BUILDER_2.shift(SortEntry::drawType) == 56);

// Macro to handle a TestAssertFailure happening when one is unexpected. This
// could be done with CHECK_NOTHROW but doing it this way results in a clearer
// message in the unit test output.
#define EXPECT_NO_ASSERT(...)                                                  \
    do                                                                         \
    {                                                                          \
        try                                                                    \
        {                                                                      \
            [&]() __VA_ARGS__();                                               \
        }                                                                      \
        catch (TestAssertFailure)                                              \
        {                                                                      \
            FAIL_CHECK("Unexpected assert");                                   \
        }                                                                      \
    } while (false)

// Macro to handle a TestAssertFailure *not* happening when it should. Like
// above, this could be done with CHECK_THROWS_AS(..., TestAssertFailure), but
// this results in a clearer message on failure.
#define EXPECT_ASSERT(...)                                                     \
    do                                                                         \
    {                                                                          \
        try                                                                    \
        {                                                                      \
            tl_expects_assert = true;                                          \
            [&]() __VA_ARGS__();                                               \
            FAIL_CHECK("Expected assert");                                     \
            tl_expects_assert = false;                                         \
        }                                                                      \
        catch (TestAssertFailure)                                              \
        {                                                                      \
            tl_expects_assert = false;                                         \
        }                                                                      \
    } while (false)

TEST_CASE("SortKeyBuilderInitializationValidation",
          "[RenderContext][SortKeyBuilder]")
{
    // This should initialize without issue
    EXPECT_NO_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 15},
            {.entry = SortEntry::drawType, .bitCount = 3},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 15},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // This should assert due to a duplicate entry
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 10},
            {.entry = SortEntry::drawGroup, .bitCount = 5},
            {.entry = SortEntry::drawType, .bitCount = 3},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 15},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // This should assert due to a missing entry
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 15},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 15},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // This should assert due to more than 63 bits specified
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 15},
            {.entry = SortEntry::drawType, .bitCount = 3},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 16},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // This should also assert due to more than 63 bits specified
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 15},
            {.entry = SortEntry::drawType, .bitCount = 4},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 15},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // Not that this is likely to happen in practice, but this specifies bits in
    // a way that will *sum* to less than 64 bits but there's an individual
    // element that's too large, ensure that this is also handled.
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 0xFFFFFFFF},
            {.entry = SortEntry::drawType, .bitCount = 4},
            {.entry = SortEntry::textureHash, .bitCount = 14},
            {.entry = SortEntry::blendMode, .bitCount = 4},
            {.entry = SortEntry::drawContents, .bitCount = 9},
            {.entry = SortEntry::drawIndex, .bitCount = 15},
            {.entry = SortEntry::subpassIndex, .bitCount = 3},
        };
    });

    // This should fail because literally no bits are specified
    EXPECT_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 0},
            {.entry = SortEntry::drawType, .bitCount = 0},
            {.entry = SortEntry::textureHash, .bitCount = 0},
            {.entry = SortEntry::blendMode, .bitCount = 0},
            {.entry = SortEntry::drawContents, .bitCount = 0},
            {.entry = SortEntry::drawIndex, .bitCount = 0},
            {.entry = SortEntry::subpassIndex, .bitCount = 0},
        };
    });

    // Technically this is fine to have a single bit from a single element.
    EXPECT_NO_ASSERT({
        std::ignore = SortKeyBuilder{
            {.entry = SortEntry::drawGroup, .bitCount = 0},
            {.entry = SortEntry::drawType, .bitCount = 0},
            {.entry = SortEntry::textureHash, .bitCount = 0},
            {.entry = SortEntry::blendMode, .bitCount = 1},
            {.entry = SortEntry::drawContents, .bitCount = 0},
            {.entry = SortEntry::drawIndex, .bitCount = 0},
            {.entry = SortEntry::subpassIndex, .bitCount = 0},
        };
    });
}

TEST_CASE("BuildPartialKeyValidation", "[RenderContext][SortKeyBuilder]")
{
    auto builder = SortKeyBuilder{
        {.entry = SortEntry::drawGroup, .bitCount = 15},
        {.entry = SortEntry::drawType, .bitCount = 3},
        {.entry = SortEntry::textureHash, .bitCount = 14},
        {.entry = SortEntry::blendMode, .bitCount = 4},
        {.entry = SortEntry::drawContents, .bitCount = 9},
        {.entry = SortEntry::drawIndex, .bitCount = 0},
        {.entry = SortEntry::subpassIndex, .bitCount = 3},
    };

    // This is fine
    EXPECT_NO_ASSERT({
        CHECK(builder.buildPartialKey({
                  {SortEntry::drawGroup, 1},
                  {SortEntry::blendMode, 3},
              }) == 0x0000000200003000);
    });

    // Assert for no values specified
    EXPECT_ASSERT({ builder.buildPartialKey({}); });

    // This should fail for a duplicate entry
    EXPECT_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawGroup, 1},
            {SortEntry::drawGroup, 3},
        });
    });

    // These should succeed - it's fine to only specify a single value that's
    // in-range
    EXPECT_NO_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawContents, 0x100},
        });
    });
    EXPECT_NO_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawContents, 0x1FF},
        });
    });

    // This should fail due to too many bits in the value being assigned
    EXPECT_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawContents, 0x200},
        });
    });

    // These should succeed despite too many bits in the value being
    // assigned, because validation was disabled
    EXPECT_NO_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawContents, 0x200, ValidateKeyEntry::no},
        });
    });
    EXPECT_NO_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawContents, 0xFFFFFFFFFFFFFFFF, ValidateKeyEntry::no},
        });
    });

    // It is also fine to add a key with set bits to an entry that has no bits
    // assigned to it (it should just be ignored)
    EXPECT_NO_ASSERT({
        builder.buildPartialKey({
            {SortEntry::drawIndex, 0xFFFFFFFF},
        });
    });
}

TEST_CASE("BuildKeyValidation", "[RenderContext][SortKeyBuilder]")
{
    auto builder = SortKeyBuilder{
        {.entry = SortEntry::drawType, .bitCount = 3},
        {.entry = SortEntry::textureHash, .bitCount = 14},
        {.entry = SortEntry::drawContents, .bitCount = 9},
        {.entry = SortEntry::subpassIndex, .bitCount = 3},
        {.entry = SortEntry::blendMode, .bitCount = 4},
        {.entry = SortEntry::drawIndex, .bitCount = 0},
        {.entry = SortEntry::drawGroup, .bitCount = 15},
    };

    // This is fine (all values are specified and in range, *or* the bit count
    // is zero)
    EXPECT_NO_ASSERT({
        CHECK(builder.buildKey({
                  {SortEntry::drawType, 1},
                  {SortEntry::textureHash, 1},
                  {SortEntry::drawContents, 1},
                  {SortEntry::subpassIndex, 1},
                  {SortEntry::blendMode, 1},
                  {SortEntry::drawIndex, 3},
                  {SortEntry::drawGroup, 3},
              }) == 0x0000200080488003);
    });

    // This is also fine (ordering of entries specified is unimportant)
    EXPECT_NO_ASSERT({
        CHECK(builder.buildKey({
                  {SortEntry::drawContents, 1},
                  {SortEntry::drawGroup, 2},
                  {SortEntry::textureHash, 1},
                  {SortEntry::drawType, 1},
                  {SortEntry::blendMode, 1},
                  {SortEntry::subpassIndex, 1},
                  {SortEntry::drawIndex, 3},
              }) == 0x0000200080488002);
    });

    // Assert for no values specified
    EXPECT_ASSERT({ builder.buildKey({}); });

    // These should fail due to a missing entry
    EXPECT_ASSERT({
        builder.buildKey({
            {SortEntry::drawContents, 1},
            {SortEntry::textureHash, 1},
            {SortEntry::drawType, 1},
            {SortEntry::blendMode, 1},
            {SortEntry::subpassIndex, 1},
            {SortEntry::drawIndex, 3},
        });
    });
    EXPECT_ASSERT({
        builder.buildKey({
            {SortEntry::drawContents, 1},
            {SortEntry::drawGroup, 2},
            {SortEntry::textureHash, 1},
            {SortEntry::drawType, 1},
            {SortEntry::blendMode, 1},
            {SortEntry::drawIndex, 3},
        });
    });

    // This should fail for a duplicate entry
    EXPECT_ASSERT({
        builder.buildKey({
            {SortEntry::drawContents, 1},
            {SortEntry::drawContents, 1},
            {SortEntry::drawGroup, 3},
            {SortEntry::textureHash, 1},
            {SortEntry::drawType, 1},
            {SortEntry::blendMode, 1},
            {SortEntry::subpassIndex, 1},
            {SortEntry::drawIndex, 3},
        });
    });

    // This should fail for an out-of-range value
    EXPECT_ASSERT({
        builder.buildKey({
            {SortEntry::drawType, 1},
            {SortEntry::textureHash, 1},
            {SortEntry::drawContents, 1},
            {SortEntry::subpassIndex, 8}, // out of range
            {SortEntry::blendMode, 1},
            {SortEntry::drawIndex, 3},
            {SortEntry::drawGroup, 3},
        });
    });

    // These should succeed despite too many bits in the value being
    // assigned, because validation was disabled (And the value applied should
    // have only the in-range bits set
    EXPECT_NO_ASSERT({
        CHECK(builder.buildKey({
                  {SortEntry::drawType, 1},
                  {SortEntry::textureHash, 1},
                  {SortEntry::drawContents, 1},
                  {
                      SortEntry::subpassIndex,
                      0x8484881, // wildly out of range
                      ValidateKeyEntry::no,
                  },
                  {SortEntry::blendMode, 1},
                  {SortEntry::drawIndex, 3},
                  {SortEntry::drawGroup, 3},
              }) == 0x0000200080488003);
    });
}

TEST_CASE("ExtractFromKey", "[RenderContext][SortKeyBuilder]")
{
    auto builder = SortKeyBuilder{
        {.entry = SortEntry::drawType, .bitCount = 3},
        {.entry = SortEntry::textureHash, .bitCount = 14},
        {.entry = SortEntry::drawContents, .bitCount = 9},
        {.entry = SortEntry::subpassIndex, .bitCount = 3},
        {.entry = SortEntry::blendMode, .bitCount = 4},
        {.entry = SortEntry::drawIndex, .bitCount = 0},
        {.entry = SortEntry::drawGroup, .bitCount = 15},
    };

    uint64_t key = 0;

    EXPECT_NO_ASSERT({
        key = builder.buildKey({
            {SortEntry::drawType, 1},
            {SortEntry::textureHash, 0xFFF0F6, ValidateKeyEntry::no},
            {SortEntry::drawContents, 0xf3},
            {SortEntry::subpassIndex, 2},
            {SortEntry::blendMode, 0xC},
            {SortEntry::drawIndex, 300},
            {SortEntry::drawGroup, 0x7fff},
        });
    });

    CHECK(builder.extract(SortEntry::drawType, key) == 1);

    CHECK(builder.extract(SortEntry::drawContents, key) == 0xf3);
    CHECK(builder.extract(SortEntry::subpassIndex, key) == 2);
    CHECK(builder.extract(SortEntry::blendMode, key) == 0xC);
    CHECK(builder.extract(SortEntry::drawGroup, key) == 0x7fff);

    // This one should be trimmed down to the available bits
    CHECK(builder.extract(SortEntry::textureHash, key) == 0x30F6);

    // This one has no bits assigned to it (and should be zero)
    CHECK(builder.extract(SortEntry::drawIndex, key) == 0);
}

} // namespace rive::gpu