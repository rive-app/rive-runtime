#include <catch.hpp>
#include "rive/renderer/stack_vector.hpp"

using namespace rive;

static constexpr uint32_t NUM_TEST_VALUES = 8;
static uint32_t test_values[NUM_TEST_VALUES] = {
    99999,
    12345,
    0,
    1,
    2468,
    1358,
    777777,
    std::numeric_limits<uint32_t>::max()};

TEST_CASE("Init", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(vec.size() == 0);
}

TEST_CASE("1 ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == vec.push_back(test_values[0]));
    CHECK(vec[0] == test_values[0]);
    CHECK(vec.size() == 1);
}

TEST_CASE("2 ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == vec.push_back(test_values[0]));
    CHECK(test_values[1] == vec.push_back(test_values[1]));
    CHECK(vec[0] == test_values[0]);
    CHECK(vec[1] == test_values[1]);
    CHECK(vec.size() == 2);
}

TEST_CASE("N ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    for (uint32_t val : test_values)
    {
        CHECK(val == vec.push_back(val));
    }

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec[i] == test_values[i]);
    }
    CHECK(vec.size() == NUM_TEST_VALUES);
}

TEST_CASE("0 ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    vec.push_back_n(0, nullptr);
    CHECK(vec.size() == 0);
}

TEST_CASE("1 ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == *vec.push_back_n(1, &test_values[0]));
    CHECK(vec.size() == 1);
}

TEST_CASE("2 ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == *vec.push_back_n(2, &test_values[0]));
    CHECK(vec.size() == 2);
}

TEST_CASE("N ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == *vec.push_back_n(NUM_TEST_VALUES, &test_values[0]));

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec[i] == test_values[i]);
    }
    CHECK(vec.size() == NUM_TEST_VALUES);
}

TEST_CASE("N ele - operator[] assign", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    StackVector<uint32_t, NUM_TEST_VALUES> vecAssign;
    CHECK(test_values[0] == *vec.push_back_n(NUM_TEST_VALUES, &test_values[0]));

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(0 == vecAssign.push_back(0));
    }

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(test_values[i] == (vecAssign[i] = vec[i]));
        CHECK(test_values[i] == vecAssign[i]);
    }
    CHECK(vec.size() == vecAssign.size());
}

TEST_CASE("N ele - clear", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    vec.push_back_n(NUM_TEST_VALUES, &test_values[0]);
    CHECK(vec.size() == NUM_TEST_VALUES);
    vec.clear();
    CHECK(vec.size() == 0);
}
