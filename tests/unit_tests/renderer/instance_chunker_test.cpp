/*
 * Copyright 2025 Rive
 */

#include <catch.hpp>
#include "instance_chunker.hpp"

using namespace rive;
using namespace rive::gpu;

TEST_CASE("maxSupportedInstancesPerDrawCommand==2, odd", "[InstanceChunker]")
{
    InstanceChunker c(5, 0, 2);
    auto it = c.begin();
    CHECK((*it).baseInstance == 0);
    CHECK((*it).instanceCount == 2);
    CHECK(it != c.end());
    ++it;
    CHECK((*it).baseInstance == 2);
    CHECK((*it).instanceCount == 2);
    CHECK(it != c.end());
    ++it;
    CHECK((*it).baseInstance == 4);
    CHECK((*it).instanceCount == 1);
    CHECK(it != c.end());
    for (size_t i = 0; i < 10; ++i)
    {
        ++it;
        CHECK((*it).baseInstance == 5);
        CHECK((*it).instanceCount == 0);
        CHECK(it == c.end());
    }
}

TEST_CASE("maxSupportedInstancesPerDrawCommand==2, even", "[InstanceChunker]")
{
    InstanceChunker c(6, 0, 2);
    auto it = c.begin();
    CHECK((*it).baseInstance == 0);
    CHECK((*it).instanceCount == 2);
    CHECK(it != c.end());
    ++it;
    CHECK((*it).baseInstance == 2);
    CHECK((*it).instanceCount == 2);
    CHECK(it != c.end());
    ++it;
    CHECK((*it).baseInstance == 4);
    CHECK((*it).instanceCount == 2);
    CHECK(it != c.end());
    for (size_t i = 0; i < 10; ++i)
    {
        ++it;
        CHECK((*it).baseInstance == 6);
        CHECK((*it).instanceCount == 0);
        CHECK(it == c.end());
    }
}

TEST_CASE("maxSupportedInstancesPerDrawCommand==0xffffffff",
          "[InstanceChunker]")
{
    InstanceChunker c(5, 0, 0xffffffff);
    auto it = c.begin();
    CHECK((*it).baseInstance == 0);
    CHECK((*it).instanceCount == 5);
    CHECK(it != c.end());
    for (size_t i = 0; i < 10; ++i)
    {
        ++it;
        CHECK((*it).baseInstance == 5);
        CHECK((*it).instanceCount == 0);
        CHECK(it == c.end());
    }
}

TEST_CASE("maxSupportedInstancesPerDrawCommand==0", "[InstanceChunker]")
{
    InstanceChunker c(5, 0, 0);
    auto it = c.begin();
    for (size_t i = 0; i < 10; ++i)
    {
        CHECK(it != c.end());
        CHECK((*it).baseInstance == 0);
        CHECK((*it).instanceCount == 0);
        ++it;
    }
}

TEST_CASE("empty", "[InstanceChunker]")
{
    InstanceChunker c(0, 0, 5);
    auto it = c.begin();
    for (size_t i = 0; i < 10; ++i)
    {
        CHECK(it == c.end());
        ++it;
    }
}

TEST_CASE("empty,maxSupportedInstancesPerDrawCommand==0", "[InstanceChunker]")
{
    InstanceChunker c(0, 0, 0);
    auto it = c.begin();
    for (size_t i = 0; i < 10; ++i)
    {
        CHECK(it == c.end());
        ++it;
    }
}
