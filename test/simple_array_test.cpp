/*
 * Copyright 2022 Rive
 */

#include <rive/simple_array.hpp>
#include <catch.hpp>
#include <rive/render_text.hpp>

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
    SimpleArray<int> array({0, 1, 2, 3, 4, 5, 6, 7, 8, 9});

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
    SimpleArray<int> array(carray);
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

    SimpleArray<uint32_t> numbersA({33, 22, 44, 66});

    StructA dataA = {.numbers = std::move(numbersA)};
    // We moved the data so expect only one alloc and 0 reallocs.
    REQUIRE(SimpleArrayTesting::mallocCount == 1);
    REQUIRE(SimpleArrayTesting::reallocCount == 0);
    REQUIRE(dataA.numbers.size() == 4);
    REQUIRE(numbersA.size() == 0);

    SimpleArray<uint32_t> numbersB({1, 2, 3});

    StructA dataB = {.numbers = std::move(numbersB)};
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
        StructA data = {.numbers = std::move(numbers)};
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
