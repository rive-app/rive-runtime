/*
 * Copyright 2026 Rive
 */

// Deferred resources answer descriptor questions on the recording thread,
// long before replay builds the real object. A recorder that forgets the
// descriptor cannot validate what it is about to record, so the answers have
// to match what a backend would say. GPU free.

#include "rive/renderer/ore/cmd/ore_deferred_context.hpp"
#include "rive/renderer/ore/cmd/ore_deferred_resource.hpp"

#include <catch.hpp>

using namespace rive;
using namespace rive::ore;
using namespace rive::ore::cmd;

TEST_CASE("a deferred layout answers for its entries", "[ore][cmd]")
{
    DeferredOreContext ctx(nullptr);

    BindGroupLayoutEntry entries[2]{};
    entries[0].binding = 0;
    entries[0].kind = BindingKind::uniformBuffer;
    entries[0].hasDynamicOffset = true;
    entries[1].binding = 1;
    entries[1].kind = BindingKind::uniformBuffer;

    BindGroupLayoutDesc ld{};
    ld.groupIndex = 2;
    ld.entries = entries;
    ld.entryCount = 2;

    auto layout = ctx.makeBindGroupLayout(ld);
    REQUIRE(layout != nullptr);
    CHECK(layout->groupIndex() == 2u);
    CHECK(layout->entries().size() == 2u);
    CHECK(layout->hasDynamicOffset(0));
    CHECK_FALSE(layout->hasDynamicOffset(1));
    CHECK_FALSE(layout->hasDynamicOffset(7)); // no such binding
}

TEST_CASE("a deferred bind group counts its dynamic offsets", "[ore][cmd]")
{
    DeferredOreContext ctx(nullptr);

    BindGroupLayoutEntry entries[2]{};
    entries[0].binding = 0;
    entries[0].kind = BindingKind::uniformBuffer;
    entries[0].hasDynamicOffset = true;
    entries[1].binding = 1;
    entries[1].kind = BindingKind::uniformBuffer;

    BindGroupLayoutDesc ld{};
    ld.groupIndex = 3;
    ld.entries = entries;
    ld.entryCount = 2;
    auto layout = ctx.makeBindGroupLayout(ld);
    REQUIRE(layout != nullptr);

    BindGroupDesc::UBOEntry ubos[2]{};
    ubos[0].slot = 0; // dynamic
    ubos[1].slot = 1; // static
    BindGroupDesc bd{};
    bd.layout = layout.get();
    bd.ubos = ubos;
    bd.uboCount = 2;

    auto group = ctx.makeBindGroup(bd);
    REQUIRE(group != nullptr);
    // Only the dynamic entry counts, and the slot comes from the layout.
    CHECK(group->dynamicOffsetCount() == 1u);
    CHECK(group->groupIndex() == 3u);
    CHECK(group->layout() == layout.get());
}

TEST_CASE("an empty deferred layout keeps no entries", "[ore][cmd]")
{
    DeferredOreContext ctx(nullptr);

    // A layout with no bindings leaves entries null, which the copy has to
    // survive.
    BindGroupLayoutDesc ld{};
    ld.groupIndex = 1;
    auto layout = ctx.makeBindGroupLayout(ld);
    REQUIRE(layout != nullptr);
    CHECK(layout->entries().empty());
    CHECK(layout->groupIndex() == 1u);

    BindGroupDesc bd{};
    bd.layout = layout.get();
    auto group = ctx.makeBindGroup(bd);
    REQUIRE(group != nullptr);
    CHECK(group->dynamicOffsetCount() == 0u);
}
