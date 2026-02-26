#include <catch.hpp>
#include "rive/renderer/stack_vector.hpp"
#include <numeric>

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

template <typename T> const T& constify(const T& t) { return t; }

TEST_CASE("1 ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == vec.push_back(test_values[0]));
    CHECK(vec[0] == test_values[0]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[0]);
    CHECK(&constify(vec).back() == &vec[0]);
    CHECK(vec.size() == 1);
    vec.front() = test_values[1];
    CHECK(vec[0] == test_values[1]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[0]);
    CHECK(&constify(vec).back() == &vec[0]);
}

TEST_CASE("2 ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == vec.push_back(test_values[0]));
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[0]);
    CHECK(&constify(vec).back() == &vec[0]);
    CHECK(test_values[1] == vec.push_back(test_values[1]));
    CHECK(vec[0] == test_values[0]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(vec[1] == test_values[1]);
    CHECK(&constify(vec)[1] == &vec[1]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[1]);
    CHECK(&constify(vec).back() == &vec[1]);
    CHECK(vec.size() == 2);
    vec.front() = test_values[2];
    vec.back() = test_values[3];
    CHECK(vec[0] == test_values[2]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(vec[1] == test_values[3]);
    CHECK(&constify(vec)[1] == &vec[1]);
}

TEST_CASE("N ele - push_back", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    for (uint32_t val : test_values)
    {
        CHECK(val == vec.push_back(val));
        CHECK(vec.back() == val);
        CHECK(&vec.front() == &vec[0]);
        CHECK(&constify(vec).front() == &vec[0]);
    }

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec[i] == test_values[i]);
        CHECK(&constify(vec)[i] == &vec[i]);
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
    CHECK(vec[0] == test_values[0]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[0]);
    CHECK(&constify(vec).back() == &vec[0]);
}

TEST_CASE("2 ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == *vec.push_back_n(2, &test_values[0]));
    CHECK(vec.size() == 2);
    CHECK(vec[0] == test_values[0]);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(vec[1] == test_values[1]);
    CHECK(&constify(vec)[1] == &vec[1]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[1]);
    CHECK(&constify(vec).back() == &vec[1]);
}

TEST_CASE("N ele - push_back_n", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(test_values[0] == *vec.push_back_n(NUM_TEST_VALUES, &test_values[0]));

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec[i] == test_values[i]);
        CHECK(&constify(vec)[i] == &vec[i]);
    }
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[NUM_TEST_VALUES - 1]);
    CHECK(&constify(vec).back() == &vec[NUM_TEST_VALUES - 1]);
    CHECK(vec.size() == NUM_TEST_VALUES);
}

TEST_CASE("0 ele - push_back_n repeat", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    vec.push_back_n(0, nullptr);
    CHECK(vec.size() == 0);
}

TEST_CASE("1 ele - push_back_n repeat", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(*vec.push_back_n(1, 48u) == 48u);
    CHECK(vec.size() == 1);
    CHECK(vec[0] == 48u);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(vec.back() == 48u);
    CHECK(constify(vec).back() == 48u);
}

TEST_CASE("2 ele - push_back_n repeat", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(*vec.push_back_n(2, 101u) == 101u);
    CHECK(vec.size() == 2);
    CHECK(vec[0] == 101u);
    CHECK(&constify(vec)[0] == &vec[0]);
    CHECK(vec[1] == 101u);
    CHECK(&constify(vec)[1] == &vec[1]);
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(vec.back() == 101u);
    CHECK(constify(vec).back() == 101u);
}

TEST_CASE("N ele - push_back_n repeat", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    CHECK(999u == *vec.push_back_n(NUM_TEST_VALUES, 999u));

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec[i] == 999u);
        CHECK(&constify(vec)[i] == &vec[i]);
    }
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(vec.back() == 999u);
    CHECK(vec.size() == NUM_TEST_VALUES);
}

TEST_CASE("N ele - operator[] assign", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    StackVector<uint32_t, NUM_TEST_VALUES> vecAssign;
    CHECK(test_values[0] == *vec.push_back_n(NUM_TEST_VALUES, &test_values[0]));
    CHECK(&vec.front() == &vec[0]);
    CHECK(&constify(vec).front() == &vec[0]);
    CHECK(&vec.back() == &vec[NUM_TEST_VALUES - 1]);
    CHECK(&constify(vec).back() == &vec[NUM_TEST_VALUES - 1]);

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(0 == vecAssign.push_back(0));
    }

    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(test_values[i] == (vecAssign[i] = vec[i]));
        CHECK(test_values[i] == vecAssign[i]);
    }
    CHECK(&vecAssign.front() == &vecAssign[0]);
    CHECK(&constify(vecAssign).front() == &vecAssign[0]);
    CHECK(&vecAssign.back() == &vecAssign[NUM_TEST_VALUES - 1]);
    CHECK(&constify(vecAssign).back() == &vecAssign[NUM_TEST_VALUES - 1]);
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

TEST_CASE("data()", "[stack_vector]")
{
    StackVector<uint32_t, NUM_TEST_VALUES> vec;
    const auto& constVec = vec;
    CHECK(vec.dataOrNull() == nullptr);
    CHECK(constVec.dataOrNull() == nullptr);

    vec.push_back_n(NUM_TEST_VALUES, nullptr);
    CHECK(vec.dataOrNull() != nullptr);
    CHECK(constVec.dataOrNull() != nullptr);
    CHECK(vec.size() == NUM_TEST_VALUES);
    CHECK(constVec.size() == NUM_TEST_VALUES);

    std::iota(vec.dataOrNull(), vec.dataOrNull() + NUM_TEST_VALUES, 0);
    for (uint32_t i = 0; i < NUM_TEST_VALUES; ++i)
    {
        CHECK(vec.dataOrNull()[i] == i);
        CHECK(constVec.dataOrNull()[i] == i);
        CHECK(vec.data()[i] == i);
        CHECK(constVec.data()[i] == i);
    }
}
