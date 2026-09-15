/*
 * Copyright 2026 Rive
 */

// Tests RiveRenderPath's cached inner-fan triangulation: geometry earns a
// persistent triangulation on its second request (the first is a throwaway in
// the caller's allocator), after which identical requests reuse the same
// instance, and every kind of raw-path mutation invalidates it so the next
// request rebuilds with the new geometry.
//
// NOTE: rebuild is detected by the triangulation reflecting the new geometry,
// NOT by pointer identity -- the allocator's reset() reuses the same memory, so
// a rebuilt triangulator often lands at the same address.

#include "rive_render_path.hpp"
#include "gr_inner_fan_triangulator.hpp"
#include "rive/math/simd.hpp"
#include <catch.hpp>

DISABLE_CLANG_SIMD_ABI_WARNING()

namespace rive
{
namespace
{
// Mirrors what PathDraw::Make does: reuse the path's cached triangulation when
// it has one, and only build otherwise. createTriangulator() asserts it isn't
// called on a path that already holds a current triangulation.
GrInnerFanTriangulator* obtainTriangulator(const RiveRenderPath& path,
                                           TrivialBlockAllocator& alloc)
{
    if (GrInnerFanTriangulator* cached = path.cachedTriangulator())
    {
        return cached;
    }
    return path.createTriangulator(alloc);
}

TEST_CASE("triangulation cache reuses an unmutated path", "[TriangulatorCache]")
{
    TrivialBlockAllocator perFrame(GrTriangulator::kArenaDefaultChunkSize);
    RiveRenderPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.lineTo(0, 100);
    path.close();

    // First sighting: a correct triangulation, but built in the caller's
    // allocator and not retained by the path.
    GrInnerFanTriangulator* first = obtainTriangulator(path, perFrame);
    REQUIRE(first != nullptr);
    const size_t vertexCount = first->maxVertexCount(FillRule::nonZero);
    CHECK(vertexCount > 0);

    // Second sighting promotes it to storage of the path's own, so it is a
    // distinct instance from the throwaway (which is still alive here).
    GrInnerFanTriangulator* second = obtainTriangulator(path, perFrame);
    REQUIRE(second != nullptr);
    CHECK(second != first);
    CHECK(second->maxVertexCount(FillRule::nonZero) == vertexCount);

    // From here on it's cached, and comes back for free without rebuilding.
    CHECK(path.cachedTriangulator() == second);
    CHECK(path.cachedTriangulator() == second);
}

TEST_CASE("triangulation cache doesn't retain single-use geometry",
          "[TriangulatorCache]")
{
    TrivialBlockAllocator perFrame(GrTriangulator::kArenaDefaultChunkSize);
    RiveRenderPath path;

    // A path that mutates between every request never reaches a second
    // sighting, so it is served entirely from the caller's allocator.
    for (int i = 0; i < 4; ++i)
    {
        path.moveTo(0, static_cast<float>(i));
        path.lineTo(100, static_cast<float>(i));
        path.lineTo(0, 100 + static_cast<float>(i));
        path.close();

        GrInnerFanTriangulator* triangulator =
            obtainTriangulator(path, perFrame);
        REQUIRE(triangulator != nullptr);
        CHECK(triangulator->maxVertexCount(FillRule::nonZero) > 0);
        // Never reaches a second sighting, so nothing is ever retained.
        CHECK(path.cachedTriangulator() == nullptr);
    }
}

TEST_CASE("triangulation cache re-promotes after a mutation",
          "[TriangulatorCache]")
{
    TrivialBlockAllocator perFrame(GrTriangulator::kArenaDefaultChunkSize);
    RiveRenderPath path;

    // Two requests promote the geometry to a cached triangulation.
    auto promote = [&]() {
        obtainTriangulator(path, perFrame);
        return obtainTriangulator(path, perFrame);
    };
    auto freshCount = [&]() {
        TrivialBlockAllocator alloc(GrTriangulator::kArenaDefaultChunkSize);
        GrInnerFanTriangulator fresh(path.getRawPath(),
                                     path.getBounds(),
                                     &alloc);
        return fresh.maxVertexCount(FillRule::nonZero);
    };

    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.lineTo(0, 100);
    path.close();
    GrInnerFanTriangulator* cached = promote();
    REQUIRE(cached != nullptr);
    const size_t firstCount = cached->maxVertexCount(FillRule::nonZero);
    CHECK(firstCount == freshCount());
    CHECK(path.cachedTriangulator() == cached);

    // Mutating drops the cache, and the geometry has to earn a new one from
    // scratch. This is the cycle a path goes through when it settles, changes,
    // and settles again -- the one place a stale cache could survive being
    // invalidated once.
    path.moveTo(200, 0);
    path.cubicTo(250, 50, 350, 150, 400, 300);
    path.lineTo(200, 300);
    path.close();

    GrInnerFanTriangulator* rePromoted = promote();
    REQUIRE(rePromoted != nullptr);
    const size_t secondCount = rePromoted->maxVertexCount(FillRule::nonZero);
    CHECK(secondCount == freshCount());
    CHECK(secondCount != firstCount);
    // ...and the re-promoted instance is the one that sticks.
    CHECK(path.cachedTriangulator() == rePromoted);
}

TEST_CASE("triangulation cache invalidates on every mutation kind",
          "[TriangulatorCache]")
{
    TrivialBlockAllocator perFrame(GrTriangulator::kArenaDefaultChunkSize);
    RiveRenderPath path;
    path.moveTo(0, 0);
    path.lineTo(100, 0);
    path.lineTo(0, 100);
    path.close();

    // The cached count, and a fresh count built independently from the current
    // raw path. Both derive the sweep axis from the path's bounds, so their
    // monotone decompositions match.
    auto cachedCount = [&]() {
        return obtainTriangulator(path, perFrame)
            ->maxVertexCount(FillRule::nonZero);
    };
    auto freshCount = [&]() {
        TrivialBlockAllocator alloc(GrTriangulator::kArenaDefaultChunkSize);
        GrInnerFanTriangulator fresh(path.getRawPath(),
                                     path.getBounds(),
                                     &alloc);
        return fresh.maxVertexCount(FillRule::nonZero);
    };

    uint64_t lastMutationID = path.getRawPathMutationID();
    REQUIRE(cachedCount() > 0);
    CHECK(cachedCount() == freshCount());

    // A mutation must advance the cache key (invalidating it) and the rebuilt
    // triangulation must match a fresh one -- not stale, not garbage. We assert
    // key advance rather than a vertex-count change because some mutations (a
    // congruent extra contour) don't alter the count yet still must invalidate.
    auto expectInvalidatedAndCorrect = [&]() {
        uint64_t id = path.getRawPathMutationID();
        CHECK(id != lastMutationID);
        lastMutationID = id;
        CHECK(cachedCount() == freshCount());
    };

    // moveTo / lineTo / close.
    path.moveTo(200, 0);
    path.lineTo(300, 0);
    path.lineTo(200, 100);
    path.close();
    expectInvalidatedAndCorrect();

    // cubicTo.
    path.moveTo(0, 200);
    path.cubicTo(50, 150, 150, 250, 300, 200);
    path.lineTo(0, 300);
    path.close();
    expectInvalidatedAndCorrect();

    // addRawPath.
    {
        RiveRenderPath extra;
        extra.moveTo(400, 0);
        extra.lineTo(500, 0);
        extra.lineTo(400, 100);
        extra.close();
        path.addRawPath(extra.getRawPath());
    }
    expectInvalidatedAndCorrect();

    // addRenderPath.
    {
        RiveRenderPath other;
        other.moveTo(600, 0);
        other.lineTo(700, 0);
        other.lineTo(600, 100);
        other.close();
        path.addRenderPath(&other, Mat2D());
    }
    expectInvalidatedAndCorrect();

    // rewind clears the path, so there's nothing left to triangulate.
    path.rewind();
    CHECK(path.getRawPathMutationID() != lastMutationID);
    CHECK(cachedCount() == 0);
    CHECK(freshCount() == 0);
}
} // namespace
} // namespace rive
