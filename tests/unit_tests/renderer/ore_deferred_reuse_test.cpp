/*
 * Copyright 2026 Rive
 */

// Producer side id reuse: the generational free list and a destroy then
// recreate lifecycle in one ordered stream, which the GMs never exercise.
// GPU free, asserts the recorded bytes.

#include "rive/renderer/cmd/id_allocator.hpp"
#include "rive/renderer/ore/cmd/ore_command_buffer.hpp"
#include "rive/renderer/ore/cmd/ore_commands.hpp"
#include "rive/renderer/ore/cmd/ore_make_recording.hpp"

#include <catch.hpp>
#include <cstring>

using namespace rive::ore;
using namespace rive::ore::cmd;
using rive::IdAllocator;
using rive::Span;

TEST_CASE("IdAllocator recycles ids with a bumped generation",
          "[ore][cmd][reuse]")
{
    IdAllocator<uint32_t> ids;

    SECTION("a freed id returns at generation+1")
    {
        auto a = ids.alloc();
        auto b = ids.alloc();
        REQUIRE(a.id == 0u);
        REQUIRE(b.id == 1u);

        ids.release(a.id, a.generation);
        auto c = ids.alloc();
        CHECK(c.id == 0u);
        CHECK(c.generation == 1u);

        // Free list is empty again so the alloc is fresh.
        auto d = ids.alloc();
        CHECK(d.id == 2u);
        CHECK(d.generation == 0u);

        // Generation keeps climbing on repeated recycling.
        ids.release(c.id, c.generation);
        auto e = ids.alloc();
        CHECK(e.id == 0u);
        CHECK(e.generation == 2u);
    }

    SECTION("an id whose generation would overflow is retired, never recycled")
    {
        auto a = ids.alloc();
        // Bumping the max generation would wrap, so the id is dropped.
        ids.release(a.id, 0xffffffffu);
        auto b = ids.alloc();
        CHECK(b.id == 1u);
        CHECK(b.generation == 0u);
    }
}

TEST_CASE("ordered stream records a create/write/destroy/recreate lifecycle",
          "[ore][cmd][reuse]")
{
    // Mirrors DeferredOreContext, one allocator and one ordered stream.
    IdAllocator<rive::ore::cmd::ResourceHandle> ids;
    OreCommandBuffer cb;

    auto a = ids.alloc();
    BufferDesc bd{};
    bd.size = 16;
    bd.usage = BufferUsage::vertex;
    recordMakeBuffer(cb, a.id, a.generation, bd);

    const uint32_t data[4] = {10, 20, 30, 40};
    recordBufferUpdate(cb, a.id, data, sizeof(data), 0);

    // The destroy records into the same stream after the write.
    recordDestroyResource(cb, a.id, a.generation);
    ids.release(a.id, a.generation);

    auto b = ids.alloc();
    REQUIRE(b.id == a.id);
    REQUIRE(b.generation == 1u);
    BufferDesc bd2{};
    bd2.size = 32;
    bd2.usage = BufferUsage::index;
    recordMakeBuffer(cb, b.id, b.generation, bd2);

    OreCommandReader r(cb.commandBytes(), cb.blobBytes());
    CommandType type;

    REQUIRE(r.next(type));
    REQUIRE(type == CommandType::makeBuffer);
    auto m0 = r.read<MakeResourcePOD>();
    auto d0 = r.read<BufferDescPOD>();
    CHECK(m0.id == a.id);
    CHECK(m0.generation == 0u);
    CHECK(d0.size == 16u);
    CHECK(d0.usage == BufferUsage::vertex);

    REQUIRE(r.next(type));
    REQUIRE(type == CommandType::bufferUpdate);
    auto up = r.read<BufferUpdatePOD>();
    CHECK(up.handle == a.id);
    CHECK(up.offset == 0u);
    Span<const uint8_t> bytes = r.blobAt(up.bytes.offset, up.bytes.size);
    REQUIRE(bytes.size() == sizeof(data));
    CHECK(std::memcmp(bytes.data(), data, sizeof(data)) == 0);

    REQUIRE(r.next(type));
    REQUIRE(type == CommandType::destroyResource);
    auto ds = r.read<DestroyResourcePOD>();
    CHECK(ds.handle == a.id);
    CHECK(ds.generation == 0u);

    REQUIRE(r.next(type));
    REQUIRE(type == CommandType::makeBuffer);
    auto m1 = r.read<MakeResourcePOD>();
    auto d1 = r.read<BufferDescPOD>();
    CHECK(m1.id == a.id);
    CHECK(m1.generation == 1u);
    CHECK(d1.size == 32u);
    CHECK(d1.usage == BufferUsage::index);

    CHECK_FALSE(r.next(type));
}
