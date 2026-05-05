/*
 * Copyright 2022 Rive
 */

#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <rive/text_engine.hpp>
#include <cstdint>
#include <limits>

using namespace rive;

TEST_CASE("array initializes as expected", "[simple array]")
{
    SimpleArray<int> array;
    REQUIRE(array.empty());
    REQUIRE(array.size() == 0);
    REQUIRE(array.size_bytes() == 0);
    REQUIRE(array.begin() == array.end());
}

TEST_CASE("simple array can be created", "[simple array]")
{
    std::vector<int> v{0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    SimpleArray<int> array(v);

    REQUIRE(!array.empty());
    REQUIRE(array.size() == 10);
    REQUIRE(array.size_bytes() == 10 * sizeof(int));
    REQUIRE(array.begin() + array.size() == array.end());

    int counter = 0;
    int sum = 0;
    for (auto s : array)
    {
        counter += 1;
        sum += s;
    }
    REQUIRE(counter == 10);
    REQUIRE(sum == 0 + 1 + 2 + 3 + 4 + 5 + 6 + 7 + 8 + 9);
}

TEST_CASE("can iterate simple array", "[simple array]")
{
    const int carray[] = {2, 4, 8, 16};
    SimpleArray<int> array(carray, 4);
    int expect = 2;
    for (auto value : array)
    {
        REQUIRE(value == expect);
        expect *= 2;
    }
}

TEST_CASE("can build up a simple array", "[simple array]")
{
    SimpleArrayBuilder<int> builder;
    builder.add(1);
    builder.add(2);
    REQUIRE(builder.size() == 2);
    REQUIRE(builder.capacity() == 2);

    builder.add(3);
    REQUIRE(builder.size() == 3);
    REQUIRE(builder.capacity() == 4);

    int iterationCount = 0;
    int expect = 1;
    for (auto value : builder)
    {
        REQUIRE(value == expect++);
        iterationCount++;
    }
    // Should only iterate what's been written so far.
    REQUIRE(iterationCount == 3);

    int reallocCountBeforeMove = SimpleArrayTesting::reallocCount;
    SimpleArray<int> array = std::move(builder);
    REQUIRE(array[0] == 1);
    REQUIRE(array[1] == 2);
    REQUIRE(array[2] == 3);
    REQUIRE(array.size() == 3);
    REQUIRE(SimpleArrayTesting::reallocCount == reallocCountBeforeMove + 1);
}

struct StructA
{
    rive::SimpleArray<uint32_t> numbers;
};

static SimpleArray<StructA> buildStructs()
{
    SimpleArrayTesting::resetCounters();

    std::vector<uint32_t> vA{33, 22, 44, 66};
    SimpleArray<uint32_t> numbersA(vA);

    StructA dataA = {std::move(numbersA)};
    // We moved the data so expect only one alloc and 0 reallocs.
    REQUIRE(SimpleArrayTesting::mallocCount == 1);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    REQUIRE(dataA.numbers.size() == 4);
    REQUIRE(numbersA.size() == 0);

    std::vector<uint32_t> vB{1, 2, 3};
    SimpleArray<uint32_t> numbersB(vB);

    StructA dataB = {std::move(numbersB)};
    REQUIRE(SimpleArrayTesting::mallocCount == 2);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    REQUIRE(dataB.numbers.size() == 3);
    REQUIRE(numbersB.size() == 0);

    SimpleArray<StructA> structs(2);
    structs[0] = std::move(dataA);
    structs[1] = std::move(dataB);
    // Should've alloc one more time to create the structs object.
    REQUIRE(SimpleArrayTesting::mallocCount == 3);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    REQUIRE(structs.size() == 2);
    REQUIRE(structs[0].numbers.size() == 4);
    REQUIRE(structs[1].numbers.size() == 3);
    return structs;
}

TEST_CASE("arrays of arrays work", "[simple array]")
{
    auto structs = buildStructs();
    REQUIRE(SimpleArrayTesting::mallocCount == 3);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    REQUIRE(structs.size() == 2);
    REQUIRE(structs[0].numbers.size() == 4);
    REQUIRE(structs[1].numbers.size() == 3);
}

static SimpleArray<StructA> buildStructsWithBuilder()
{
    SimpleArrayTesting::resetCounters();
    SimpleArrayBuilder<StructA> structs(2);
    REQUIRE(SimpleArrayTesting::mallocCount == 1);
    for (int i = 0; i < 3; i++)
    {
        SimpleArray<uint32_t> numbers({33, 22, 44, 66});
        StructA data = {std::move(numbers)};
        structs.add(std::move(data));
    }
    REQUIRE(SimpleArrayTesting::mallocCount == 4);
    // Realloc once because we'd reserved 2 and actually added 3.
    REQUIRE(SimpleArrayTesting::reallocCount == 1);
    return std::move(structs);
}

TEST_CASE("builder arrays of arrays work", "[simple array]")
{
    auto structs = buildStructsWithBuilder();
    // alloc counters should still be the same
    REQUIRE(SimpleArrayTesting::mallocCount == 4);
    // Realloc one more time as we sized down.
    REQUIRE(SimpleArrayTesting::reallocCount == 2);
}

TEST_CASE("builders can be reset", "[simple array]")
{
    SimpleArrayTesting::resetCounters();
    SimpleArrayBuilder<uint32_t> builder(3);
    builder.add(1);
    builder.add(2);
    builder.add(3);
    REQUIRE(SimpleArrayTesting::mallocCount == 1);
    REQUIRE(SimpleArrayTesting::freeCount == 0);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);

    builder = SimpleArrayBuilder<uint32_t>(4);
    // Previous builder got freed.
    REQUIRE(SimpleArrayTesting::freeCount == 1);
    // We allocated more memory.
    REQUIRE(SimpleArrayTesting::mallocCount == 2);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    builder.add(3);
    builder.add(2);

    SimpleArrayTesting::resetCounters();
    SimpleArray<uint32_t> array = std::move(builder);
    // Realloc'd down
    REQUIRE(SimpleArrayTesting::reallocCount == 1);
    // Free and malloc counts didn't move.
    REQUIRE(SimpleArrayTesting::freeCount == 0);
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
    REQUIRE(array.size() == 2);
}

TEST_CASE("ctor returns empty array on size*sizeof(T) overflow",
          "[simple array]")
{
    SimpleArrayTesting::resetCounters();

    // (SIZE_MAX/4) * sizeof(uint64_t) = (SIZE_MAX/4) * 8 wraps SIZE_MAX.
    // checkedMul rejects before malloc is called, so this is safe to
    // construct in a unit test — no gigantic allocation is attempted.
    constexpr size_t huge = std::numeric_limits<size_t>::max() / 4;
    SimpleArray<uint64_t> array(huge);

    REQUIRE(array.empty());
    REQUIRE(array.size() == 0);
    REQUIRE(array.size_bytes() == 0);
    REQUIRE(array.data() == nullptr);
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
}

// Skip under AddressSanitizer: ASAN aborts on requested sizes above its
// internal cap (~1 TiB) before the system allocator can return null, so the
// production null-check this test exists to exercise never runs. The defense
// itself (m_ptr null-check in the ctor) remains in the code; ASAN catches
// the same class of misuse via its allocation-size-too-big diagnostic.
// __SANITIZE_ADDRESS__ covers GCC, MSVC, and Clang 13+; older Apple Clang
// only sets __has_feature(address_sanitizer), so check both. The
// __has_feature shim keeps MSVC's preprocessor (which doesn't know
// __has_feature) from choking on the second condition.
#ifndef __has_feature
#define __has_feature(x) 0
#endif
#if !defined(__SANITIZE_ADDRESS__) && !__has_feature(address_sanitizer)
TEST_CASE("ctor returns empty array when malloc fails (OOM)", "[simple array]")
{
    SimpleArrayTesting::resetCounters();

    // SIZE_MAX * 1 does not overflow checkedMul, but no real allocator can
    // satisfy SIZE_MAX bytes — malloc returns nullptr and the ctor must
    // tolerate it without writing through the null pointer.
    SimpleArray<uint8_t> array(std::numeric_limits<size_t>::max());

    REQUIRE(array.empty());
    REQUIRE(array.size() == 0);
    REQUIRE(array.data() == nullptr);
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
}
#endif

TEST_CASE("delegating ctor stays safe on overflow", "[simple array]")
{
    SimpleArrayTesting::resetCounters();

    constexpr size_t huge = std::numeric_limits<size_t>::max() / 4;
    const uint64_t dummy = 0;
    SimpleArray<uint64_t> array(&dummy, huge);

    REQUIRE(array.empty());
    REQUIRE(array.size() == 0);
    REQUIRE(array.data() == nullptr);
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
}

TEST_CASE("ctor still works for normal sizes after overflow guard",
          "[simple array]")
{
    SimpleArrayTesting::resetCounters();

    SimpleArray<uint32_t> array(8);
    REQUIRE(!array.empty());
    REQUIRE(array.size() == 8);
    REQUIRE(array.data() != nullptr);
    REQUIRE(SimpleArrayTesting::mallocCount == 1);
}

TEST_CASE("overflow-failed array stays composable", "[simple array]")
{
    // Construct an array that fails the size*sizeof(T) overflow check, then
    // exercise the rest of the API on the empty state. Locks the
    // m_ptr=nullptr/m_size=0 invariant against future refactors of either
    // the ctor or the move/copy paths.
    constexpr size_t huge = std::numeric_limits<size_t>::max() / 4;

    SimpleArrayTesting::resetCounters();

    SimpleArray<uint64_t> failed(huge);
    REQUIRE(failed.empty());
    REQUIRE(failed.begin() == failed.end());

    // range-for should iterate zero times.
    int iterations = 0;
    for (auto& v : failed)
    {
        (void)v;
        iterations++;
    }
    REQUIRE(iterations == 0);

    // move-construct from the failed array — moved should still be empty,
    // failed is left valid-but-empty per move semantics.
    SimpleArray<uint64_t> moved = std::move(failed);
    REQUIRE(moved.empty());
    REQUIRE(moved.data() == nullptr);

    // copy-construct from another freshly-failed array — delegates through
    // SimpleArray(const T*, size_t) which short-circuits in the size==0 ctor.
    SimpleArray<uint64_t> another(huge);
    SimpleArray<uint64_t> copy(another);
    REQUIRE(copy.empty());
    REQUIRE(copy.data() == nullptr);

    // No real allocations should have happened along any of these paths.
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
    // Destruction at scope end must not crash on free(nullptr).
}

TEST_CASE("delegating ctor accepts (nullptr, 0) without UB", "[simple array]")
{
    // The delegating SimpleArray(const T*, size_t) ctor must early-return
    // before touching `ptr` when size is zero; otherwise nullptr arithmetic
    // and memcpy(nullptr, ...) would be UB even with size == 0.
    SimpleArrayTesting::resetCounters();

    SimpleArray<int> array(static_cast<const int*>(nullptr), 0);
    REQUIRE(array.empty());
    REQUIRE(array.data() == nullptr);
    REQUIRE(SimpleArrayTesting::mallocCount == 0);
}
