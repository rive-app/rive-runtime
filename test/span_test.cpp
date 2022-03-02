/*
 * Copyright 2022 Rive
 */

#include <rive/span.hpp>
#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("span", "[basics]") {
    Span<int> span;
    REQUIRE(span.empty());
    REQUIRE(span.size() == 0);
    REQUIRE(span.totalBytes() == 0);
    REQUIRE(span.begin() == span.end());

    int array[] = { 0, 1, 2, 3, 4, 5, 6,7 ,8, 9 };

    span = { array, 4 };
    REQUIRE(!span.empty());
    REQUIRE(span.data() == array);
    REQUIRE(span.size() == 4);
    REQUIRE(span.totalBytes() == 4 * sizeof(int));
    REQUIRE(span.begin() + span.size() == span.end());
    
    int counter = 0;
    int sum = 0;
    for (auto s : span) {
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
