/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/math/raw_path.hpp>

#include <catch.hpp>
#include <cstdio>
#include <limits>
#include <tuple>

using namespace rive;

TEST_CASE("rawpath-basics", "[rawpath]")
{
    RawPath path;

    REQUIRE(path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 0, 0});

    path.move({1, 2});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{1, 2, 1, 2});

    path = RawPath();
    REQUIRE(path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 0, 0});

    path.move({1, -2});
    path.line({3, 4});
    path.line({-1, 5});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{-1, -2, 3, 5});
}

TEST_CASE("rawpath-add-helpers", "[rawpath]")
{
    RawPath path;

    path.addRect({1, 1, 5, 6});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{1, 1, 5, 6});
    REQUIRE(path.points().size() == 4);
    REQUIRE(path.verbs().size() == 5); // move, line, line, line, close

    path = RawPath();
    path.addOval({0, 0, 3, 6});
    REQUIRE(!path.empty());
    REQUIRE(path.bounds() == AABB{0, 0, 3, 6});
    REQUIRE(path.points().size() == 13);
    REQUIRE(path.verbs().size() == 6); // move, cubic, cubic, cubic, cubic, close

    const Vec2D pts[] = {
        {1, 2},
        {4, 5},
        {3, 2},
        {100, -100},
    };
    constexpr auto size = sizeof(pts) / sizeof(pts[0]);

    for (auto isClosed : {false, true})
    {
        path = RawPath();
        path.addPoly({pts, size}, isClosed);
        REQUIRE(path.bounds() == AABB{1, -100, 100, 5});
        REQUIRE(path.points().size() == size);
        REQUIRE(path.verbs().size() == size + isClosed);

        for (size_t i = 0; i < size; ++i)
        {
            REQUIRE(path.points()[i] == pts[i]);
        }
        REQUIRE(path.verbs()[0] == PathVerb::move);
        for (size_t i = 1; i < size; ++i)
        {
            REQUIRE(path.verbs()[i] == PathVerb::line);
        }
        if (isClosed)
        {
            REQUIRE(path.verbs()[size] == PathVerb::close);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

static void check_iter(RawPath::Iter& iter,
                       const RawPath::Iter& end,
                       PathVerb expectedVerb,
                       std::vector<Vec2D> expectedPts)
{
    REQUIRE(iter != end);
    PathVerb verb = iter.verb();
    const Vec2D* pts = iter.pts();
    REQUIRE(verb == expectedVerb);
    switch (verb)
    {
        case PathVerb::move:
            CHECK(expectedPts.size() == 1);
            CHECK(pts[0] == iter.movePt());
            break;
        case PathVerb::line:
            CHECK(expectedPts.size() == 2);
            CHECK(pts == iter.linePts());
            break;
        case PathVerb::quad:
            CHECK(expectedPts.size() == 3);
            CHECK(pts == iter.quadPts());
            break;
        case PathVerb::cubic:
            CHECK(expectedPts.size() == 4);
            CHECK(pts == iter.cubicPts());
            break;
        case PathVerb::close:
            CHECK(expectedPts.size() == 0);
            CHECK(pts == iter.rawPtsPtr() - 1);
            break;
    }
    for (size_t i = 0; i < expectedPts.size(); ++i)
    {
        CHECK(pts[i] == expectedPts[i]);
    }
    ++iter;
}

TEST_CASE("rawpath-iter", "[rawpath]")
{
    {
        RawPath rp;
        REQUIRE(rp.begin() == rp.end());
    }
    {
        RawPath rp;
        rp.moveTo(1, 2);
        rp.lineTo(3, 4);
        rp.quadTo(5, 6, 7, 8);
        rp.cubicTo(9, 10, 11, 12, 13, 14);
        rp.close();
        auto iter = rp.begin();
        auto end = rp.end();
        check_iter(iter, end, PathVerb::move, {{1, 2}});
        check_iter(iter, end, PathVerb::line, {{1, 2}, {3, 4}});
        check_iter(iter, end, PathVerb::quad, {{3, 4}, {5, 6}, {7, 8}});
        check_iter(iter, end, PathVerb::cubic, {{7, 8}, {9, 10}, {11, 12}, {13, 14}});
        check_iter(iter, end, PathVerb::close, {});
        REQUIRE(iter == end);

        // Moves are never discarded.
        rp.reset();
        rp.moveTo(1, 2);
        rp.moveTo(3, 4);
        rp.moveTo(5, 6);
        rp.close();
        std::tie(iter, end) = std::make_tuple(rp.begin(), rp.end());
        check_iter(iter, end, PathVerb::move, {{1, 2}});
        check_iter(iter, end, PathVerb::move, {{3, 4}});
        check_iter(iter, end, PathVerb::move, {{5, 6}});
        check_iter(iter, end, PathVerb::close, {});
        REQUIRE(iter == end);

        // lineTo, quadTo, and cubicTo can inject implicit moveTos.
        rp.rewind();
        rp.close();                   // discarded
        rp.close();                   // discarded
        rp.close();                   // discarded
        rp.close();                   // discarded
        rp.lineTo(1, 2);              // injects moveTo(0, 0)
        rp.close();                   // kept
        rp.close();                   // discarded
        rp.cubicTo(3, 4, 5, 6, 7, 8); // injects moveTo(0, 0)
        rp.moveTo(9, 10);
        rp.moveTo(11, 12);
        rp.quadTo(13, 14, 15, 16);
        rp.close();        // kept
        rp.lineTo(17, 18); // injects moveTo(11, 12)
        std::tie(iter, end) = std::make_tuple(rp.begin(), rp.end());
        check_iter(iter, end, PathVerb::move, {{0, 0}});
        check_iter(iter, end, PathVerb::line, {{0, 0}, {1, 2}});
        check_iter(iter, end, PathVerb::close, {});
        check_iter(iter, end, PathVerb::move, {{0, 0}});
        check_iter(iter, end, PathVerb::cubic, {{0, 0}, {3, 4}, {5, 6}, {7, 8}});
        check_iter(iter, end, PathVerb::move, {{9, 10}});
        check_iter(iter, end, PathVerb::move, {{11, 12}});
        check_iter(iter, end, PathVerb::quad, {{11, 12}, {13, 14}, {15, 16}});
        check_iter(iter, end, PathVerb::close, {});
        check_iter(iter, end, PathVerb::move, {{11, 12}});
        check_iter(iter, end, PathVerb::line, {{11, 12}, {17, 18}});
        REQUIRE(iter == end);
    }
}

TEST_CASE("addPath", "[rawpath]")
{
    using PathMaker = void (*)(RawPath * sink);

    const PathMaker makers[] = {
        [](RawPath* sink) {},
        [](RawPath* sink) {
            sink->moveTo(1, 2);
            sink->lineTo(3, 4);
        },
        [](RawPath* sink) {
            sink->moveTo(1, 2);
            sink->lineTo(3, 4);
            sink->close();
        },
        [](RawPath* sink) {
            sink->moveTo(1, 2);
            sink->lineTo(3, 4);
            sink->quadTo(5, 6, 7, 8);
            sink->cubicTo(9, 10, 11, 12, 13, 14);
            sink->close();
        },
    };
    constexpr size_t N = sizeof(makers) / sizeof(makers[0]);

    auto direct = [](PathMaker m0, PathMaker m1, const Mat2D* mx) {
        RawPath p;
        m0(&p);
        m1(&p);
        if (mx)
        {
            p.transformInPlace(*mx);
        }
        return p;
    };
    auto useadd = [](PathMaker m0, PathMaker m1, const Mat2D* mx) {
        RawPath p;

        RawPath tmp;
        m0(&tmp);
        p.addPath(tmp, mx);

        tmp.reset();
        m1(&tmp);
        p.addPath(tmp, mx);
        return p;
    };

    for (auto i = 0; i < N; ++i)
    {
        for (auto j = 0; j < N; ++j)
        {
            RawPath p0, p1;

            p0 = direct(makers[i], makers[j], nullptr);
            p1 = useadd(makers[i], makers[j], nullptr);
            REQUIRE(p0 == p1);

            auto mx = Mat2D::fromScale(2, 3);
            p0 = direct(makers[i], makers[j], &mx);
            p1 = useadd(makers[i], makers[j], &mx);
            REQUIRE(p0 == p1);
        }
    }
}

TEST_CASE("bounds", "[rawpath]")
{
    RawPath path;
    AABB bounds;
    srand(0);
    const auto randPt = [&] {
        Vec2D pt = Vec2D(float(rand()), float(rand())) / (float(RAND_MAX) * .5f) - Vec2D(1, 1);
        bounds.minX = std::min(bounds.minX, pt.x);
        bounds.minY = std::min(bounds.minY, pt.y);
        bounds.maxX = std::max(bounds.maxX, pt.x);
        bounds.maxY = std::max(bounds.maxY, pt.y);
        return pt;
    };
    for (int numVerbs = 1; numVerbs < 1 << 16; numVerbs <<= 1)
    {
        path.rewind();
        bounds.minX = bounds.minY = std::numeric_limits<float>::infinity();
        bounds.maxX = bounds.maxY = -std::numeric_limits<float>::infinity();
        for (int i = 0; i < numVerbs; ++i)
        {
            switch (rand() % 5)
            {
                case 0:
                    path.move(randPt());
                    break;
                case 1:
                    if (path.empty())
                    { // Account for the implicit moveTo(0).
                        bounds = {};
                    }
                    path.line(randPt());
                    break;
                case 2:
                    if (path.empty())
                    { // Account for the implicit moveTo(0).
                        bounds = {};
                    }
                    path.quad(randPt(), randPt());
                    break;
                case 3:
                    if (path.empty())
                    { // Account for the implicit moveTo(0).
                        bounds = {};
                    }
                    path.cubic(randPt(), randPt(), randPt());
                    break;
                case 4:
                    path.close();
                    break;
            }
        }
        AABB pathBounds = path.bounds();
        REQUIRE(pathBounds.minX == bounds.minX);
        REQUIRE(pathBounds.minY == bounds.minY);
        REQUIRE(pathBounds.maxX == bounds.maxX);
        REQUIRE(pathBounds.maxY == bounds.maxY);
    }
}

TEST_CASE("prune-empty-segments", "[rawpath]")
{
    {
        RawPath p;
        p.pruneEmptySegments();
        CHECK(p.begin() == p.end());
    }

    {
        RawPath p;
        p.lineTo(0, 0);
        p.pruneEmptySegments();
        auto iter = p.begin();
        auto end = p.end();
        check_iter(iter, end, PathVerb::move, {{0, 0}});
        CHECK(iter == end);
    }

    {
        RawPath p;
        p.quadTo(0, 0, 0, 0);
        p.pruneEmptySegments();
        auto iter = p.begin();
        auto end = p.end();
        check_iter(iter, end, PathVerb::move, {{0, 0}});
        CHECK(iter == end);
    }

    {
        RawPath p;
        p.cubicTo(0, 0, 0, 0, 0, 0);
        p.pruneEmptySegments();
        auto iter = p.begin();
        auto end = p.end();
        check_iter(iter, end, PathVerb::move, {{0, 0}});
        CHECK(iter == end);
    }

    {
        RawPath p;
        p.moveTo(1, 2);
        p.lineTo(3, 4);
        p.lineTo(3, 4);
        p.quadTo(5, 6, 7, 8);
        p.quadTo(7, 8, 7, 8);
        p.quadTo(7, 8, 7, 9);
        p.quadTo(7, 9, 7, 9);
        p.quadTo(7, 9, 7, 8);
        p.quadTo(7, 8, 7, 8);
        p.cubicTo(9, 10, 11, 12, 13, 14);
        p.cubicTo(13, 14, 13, 14, 13, 14);
        p.cubicTo(13, 14, 13, 14, 13, 15);
        p.cubicTo(13, 15, 13, 15, 13, 15);
        p.cubicTo(13, 16, 13, 15, 13, 15);
        p.cubicTo(13, 15, 13, 15, 13, 15);
        p.cubicTo(13, 15, 13, 16, 13, 15);
        p.cubicTo(13, 15, 13, 15, 13, 15);
        p.cubicTo(13, 15, 13, 15, 13, 16);
        p.close();
        p.pruneEmptySegments();
        auto iter = p.begin();
        auto end = p.end();
        check_iter(iter, end, PathVerb::move, {{1, 2}});
        check_iter(iter, end, PathVerb::line, {{1, 2}, {3, 4}});
        check_iter(iter, end, PathVerb::quad, {{3, 4}, {5, 6}, {7, 8}});
        check_iter(iter, end, PathVerb::quad, {{7, 8}, {7, 8}, {7, 9}});
        check_iter(iter, end, PathVerb::quad, {{7, 9}, {7, 9}, {7, 8}});
        check_iter(iter, end, PathVerb::cubic, {{7, 8}, {9, 10}, {11, 12}, {13, 14}});
        check_iter(iter, end, PathVerb::cubic, {{13, 14}, {13, 14}, {13, 14}, {13, 15}});
        check_iter(iter, end, PathVerb::cubic, {{13, 15}, {13, 16}, {13, 15}, {13, 15}});
        check_iter(iter, end, PathVerb::cubic, {{13, 15}, {13, 15}, {13, 16}, {13, 15}});
        check_iter(iter, end, PathVerb::cubic, {{13, 15}, {13, 15}, {13, 15}, {13, 16}});
        check_iter(iter, end, PathVerb::close, {});
        CHECK(iter == end);
    }

    {
        RawPath p;
        p.moveTo(1, 2);
        p.lineTo(1, 2);
        p.lineTo(3, 4);

        RawPath p2;
        p2.moveTo(5, 6);
        p2.quadTo(7, 8, 9, 10);
        p2.close();
        p2.moveTo(11, 12);
        p2.cubicTo(13, 14, 15, 16, 17, 18);

        Mat2D matZero = Mat2D(0, 0, 0, 0, 19, 20);
        auto p2it = p.addPath(p2, &matZero);

        // Pruning at the end does nothing.
        p.pruneEmptySegments(p.end());
        {
            auto iter = p.begin();
            auto end = p.end();
            check_iter(iter, end, PathVerb::move, {{1, 2}});
            check_iter(iter, end, PathVerb::line, {{1, 2}, {1, 2}});
            check_iter(iter, end, PathVerb::line, {{1, 2}, {3, 4}});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            check_iter(iter, end, PathVerb::quad, {{19, 20}, {19, 20}, {19, 20}});
            check_iter(iter, end, PathVerb::close, {});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            check_iter(iter, end, PathVerb::cubic, {{19, 20}, {19, 20}, {19, 20}, {19, 20}});
            CHECK(iter == end);
        }

        // Pruning just at the beginning of the added p2 won't remove the pre-existing empty
        // segment.
        p.pruneEmptySegments(p2it);
        {
            auto iter = p.begin();
            auto end = p.end();
            check_iter(iter, end, PathVerb::move, {{1, 2}});
            check_iter(iter, end, PathVerb::line, {{1, 2}, {1, 2}});
            check_iter(iter, end, PathVerb::line, {{1, 2}, {3, 4}});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            check_iter(iter, end, PathVerb::close, {});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            CHECK(iter == end);
        }

        // Now remove the pre-existing one.
        p.pruneEmptySegments(p.begin());
        {
            auto iter = p.begin();
            auto end = p.end();
            check_iter(iter, end, PathVerb::move, {{1, 2}});
            check_iter(iter, end, PathVerb::line, {{1, 2}, {3, 4}});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            check_iter(iter, end, PathVerb::close, {});
            check_iter(iter, end, PathVerb::move, {{19, 20}});
            CHECK(iter == end);
        }
    }
}
