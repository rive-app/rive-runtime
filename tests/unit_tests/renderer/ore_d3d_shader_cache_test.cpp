/*
 * Copyright 2025 Rive
 */

// The cache is a header-only template over the compiled artifact type, so it
// is exercised here with a string standing in for bytecode; the D3D backends
// only add which bytes they store.

#include "ore/ore_d3d_shader_cache.hpp"

#include <catch.hpp>
#include <string>

using rive::ore::D3DShaderCache;
using rive::ore::oreD3DShaderCacheKey;

namespace
{
using StringCache = D3DShaderCache<std::string>;

void storeString(
    StringCache& cache,
    const std::string& key,
    const std::string& compiled)
{
  cache.store(key, compiled, compiled.size());
}
} // namespace

TEST_CASE("key covers target, entry point and source", "[ore_d3d_shader_cache]")
{
  const std::string a = oreD3DShaderCacheKey("ps_5_0", "main", "float4 f();");
  CHECK(a == oreD3DShaderCacheKey("ps_5_0", "main", "float4 f();"));
  CHECK(a != oreD3DShaderCacheKey("vs_5_0", "main", "float4 f();"));
  CHECK(a != oreD3DShaderCacheKey("ps_5_0", "other", "float4 f();"));
  CHECK(a != oreD3DShaderCacheKey("ps_5_0", "main", "float4 g();"));
}

TEST_CASE("stores and returns a compile by key", "[ore_d3d_shader_cache]")
{
  StringCache cache;
  std::string out;
  CHECK_FALSE(cache.lookup("k1", &out));

  storeString(cache, "k1", "bytecode-1");
  REQUIRE(cache.lookup("k1", &out));
  CHECK(out == "bytecode-1");
  CHECK(cache.size() == 1);
  CHECK(cache.totalBytes() == std::string("k1").size() + 10);

  // A second store for the same key keeps the first result.
  storeString(cache, "k1", "bytecode-other");
  REQUIRE(cache.lookup("k1", &out));
  CHECK(out == "bytecode-1");
  CHECK(cache.size() == 1);
}

TEST_CASE(
    "evicts the least recently used entry past the budget",
    "[ore_d3d_shader_cache]")
{
  // Each entry costs key (2) + compiled (8) = 10 bytes.
  StringCache cache(25);
  storeString(cache, "k1", "11111111");
  storeString(cache, "k2", "22222222");
  CHECK(cache.size() == 2);
  CHECK(cache.totalBytes() == 20);

  SECTION("oldest goes first")
  {
    storeString(cache, "k3", "33333333");
    std::string out;
    CHECK(cache.size() == 2);
    CHECK(cache.totalBytes() == 20);
    CHECK_FALSE(cache.lookup("k1", &out));
    CHECK(cache.lookup("k2", &out));
    CHECK(cache.lookup("k3", &out));
  }

  SECTION("a lookup counts as use")
  {
    std::string out;
    REQUIRE(cache.lookup("k1", &out));
    storeString(cache, "k3", "33333333");
    CHECK(cache.size() == 2);
    CHECK(cache.lookup("k1", &out));
    CHECK_FALSE(cache.lookup("k2", &out));
    CHECK(cache.lookup("k3", &out));
  }
}

TEST_CASE(
    "keeps the newest entry even when it alone exceeds the budget",
    "[ore_d3d_shader_cache]")
{
  StringCache cache(4);
  storeString(cache, "k1", "11111111");
  std::string out;
  REQUIRE(cache.lookup("k1", &out));
  CHECK(out == "11111111");
  CHECK(cache.size() == 1);

  // The next store replaces it: the budget holds one of them at most.
  storeString(cache, "k2", "22222222");
  CHECK(cache.size() == 1);
  CHECK_FALSE(cache.lookup("k1", &out));
  REQUIRE(cache.lookup("k2", &out));
  CHECK(out == "22222222");
}
