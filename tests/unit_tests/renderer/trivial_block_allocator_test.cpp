#include <catch.hpp>
#include "rive/renderer/trivial_block_allocator.hpp"

using namespace rive;

TEST_CASE("AlignmentInBytes", "[trivial_block_allocator]")
{
    TrivialBlockAllocator tba(128);
    auto p = reinterpret_cast<uintptr_t>(tba.alloc<16>(1));
    CHECK(p != 0);
    CHECK(p % 16 == 0);
    p = reinterpret_cast<uintptr_t>(tba.alloc<32>(1));
    CHECK(p != 0);
    CHECK(p % 32 == 0);
    p = reinterpret_cast<uintptr_t>(tba.alloc<64>(100));
    CHECK(p != 0);
    CHECK(p % 64 == 0);
    p = reinterpret_cast<uintptr_t>(tba.alloc<128>(200));
    CHECK(p != 0);
    CHECK(p % 128 == 0);
    p = reinterpret_cast<uintptr_t>(tba.alloc<1>(200));
    CHECK(p != 0);
    CHECK(p % 1 == 0);
    p = reinterpret_cast<uintptr_t>(tba.alloc<1024>(1));
    CHECK(p != 0);
    CHECK(p % 1024 == 0);
}

TEST_CASE("rewindLastAllocation", "[trivial_block_allocator]")
{
    TrivialBlockAllocator tba(128);
    auto first = reinterpret_cast<uintptr_t>(tba.alloc<1>(1));
    CHECK(first != 0);
    tba.rewindLastAllocation(1);
    auto p = reinterpret_cast<uintptr_t>(tba.alloc<1>(100));
    CHECK(p == first);
    tba.rewindLastAllocation(50);
    p = reinterpret_cast<uintptr_t>(tba.alloc<1>(1));
    CHECK(p == first + 50);
    p = reinterpret_cast<uintptr_t>(tba.alloc<1>(1));
    CHECK(p == first + 51);
    tba.rewindLastAllocation(5);
    p = reinterpret_cast<uintptr_t>(tba.alloc<1>(1));
    CHECK(p == first + 47);
}
