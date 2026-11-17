/*
 * Copyright 2022 Rive
 */

#include <rive/span.hpp>
#include <catch.hpp>
#include <cstdio>
#include <vector>

using namespace rive;

TEST_CASE("basics", "[span]")
{
  Span<int> span;
  REQUIRE(span.empty());
  REQUIRE(span.size() == 0);
  REQUIRE(span.size_bytes() == 0);
  REQUIRE(span.begin() == span.end());

  int array[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

  span = {array, 4};
  REQUIRE(!span.empty());
  REQUIRE(span.data() == array);
  REQUIRE(span.size() == 4);
  REQUIRE(span.size_bytes() == 4 * sizeof(int));
  REQUIRE(span.begin() + span.size() == span.end());

  int counter = 0;
  int sum = 0;
  for (auto s : span)
  {
    counter += 1;
    sum += s;
  }
  REQUIRE(counter == 4);
  REQUIRE(sum == 0 + 1 + 2 + 3);

  auto sub = span.subset(1, 2);
  REQUIRE(!sub.empty());
  REQUIRE(sub.data() == array + 1);
  REQUIRE(sub.size() == 2);

  sub = sub.subset(1, 0);
  REQUIRE(sub.empty());
  REQUIRE(sub.size() == 0);
}

static void funca(Span<int> span) {}
static void funcb(Span<const int> span) {}

TEST_CASE("const-and-containers", "[span]")
{
  const int carray[] = {1, 2, 3, 4};
  funcb({carray, 4});

  int array[] = {1, 2, 3, 4};
  funca({array, 4});
  funcb({array, 4});

  std::vector<int> v;
  funca(v);
  funcb(v);
}

TEST_CASE("can iterate span", "[span]")
{
  const int carray[] = {2, 4, 8, 16};

  auto span = make_span(carray, 4);
  int expect = 2;
  for (auto value : span)
  {
    REQUIRE(value == expect);
    expect *= 2;
  }
}

TEST_CASE("subspans", "[span]")
{
  int carray[] = {1, 2, 6, 24, 120, 720, 5040, 40320};

  static_assert(internal::IsSpannable<decltype(carray)>);
  static_assert(
      std::is_same_v<internal::SpannableElementType<decltype(carray)>, int>);

  auto vec = std::vector<int>{std::begin(carray), std::end(carray)};
  auto span = Span{carray};

  // valid start positions include "size()", which would just make a 0-length
  // span.
  for (auto start = 0u; start <= std::size(carray); start++)
  {
    for (auto end = start; end <= std::size(carray); end++)
    {
      auto runTest1 = [start, end](auto& src) {
        auto subSpan = Span{src, start, end - start};
        CHECK(subSpan.size() == end - start);
        for (auto i = 0u; i < subSpan.size(); i++)
        {
          CHECK(subSpan[i] == src[i + start]);
        }
      };

      // test 2 is the same as test 1, but making a const span instead.
      auto runTest2 = [start, end](auto& src) {
        auto subSpan = Span<const int>{src, start, end - start};
        CHECK(subSpan.size() == end - start);
        for (auto i = 0u; i < subSpan.size(); i++)
        {
          CHECK(subSpan[i] == src[i + start]);
        }
      };

      runTest1(carray);
      runTest1(vec);
      runTest1(span);

      runTest2(carray);
      runTest2(vec);
      runTest2(span);
    }

    // Test3 tests the toEnd version of subspans
    auto runTest3 = [start](auto& src) {
      auto subSpan = Span{src, start, ToEnd};
      CHECK(subSpan.size() == std::size(src) - start);
      for (auto i = 0u; i < subSpan.size(); i++)
      {
        CHECK(subSpan[i] == src[i + start]);
      }
    };

    // Test4 tests the toEnd version of const subspans
    auto runTest4 = [start](auto& src) {
      auto subSpan = Span<const int>{src, start, ToEnd};
      CHECK(subSpan.size() == std::size(src) - start);
      for (auto i = 0u; i < subSpan.size(); i++)
      {
        CHECK(subSpan[i] == src[i + start]);
      }
    };

    runTest3(carray);
    runTest3(vec);
    runTest3(span);

    runTest4(carray);
    runTest4(vec);
    runTest4(span);
  }
}