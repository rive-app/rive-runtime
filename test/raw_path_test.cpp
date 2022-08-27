/*
 * Copyright 2022 Rive
 */

#include <rive/math/aabb.hpp>
#include <rive/math/raw_path.hpp>

#include <catch.hpp>
#include <cstdio>

using namespace rive;

TEST_CASE("rawpath-basics", "[rawpath]") {
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

TEST_CASE("rawpath-add-helpers", "[rawpath]") {
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

    for (auto isClosed : {false, true}) {
        path = RawPath();
        path.addPoly({pts, size}, isClosed);
        REQUIRE(path.bounds() == AABB{1, -100, 100, 5});
        REQUIRE(path.points().size() == size);
        REQUIRE(path.verbs().size() == size + isClosed);

        for (size_t i = 0; i < size; ++i) {
            REQUIRE(path.points()[i] == pts[i]);
        }
        REQUIRE(path.verbs()[0] == PathVerb::move);
        for (size_t i = 1; i < size; ++i) {
            REQUIRE(path.verbs()[i] == PathVerb::line);
        }
        if (isClosed) {
            REQUIRE(path.verbs()[size] == PathVerb::close);
        }
    }
}

//////////////////////////////////////////////////////////////////////////

static bool is_move(const RawPath::Iter::Rec& rec) {
    if (rec.verb == PathVerb::move) {
        REQUIRE(rec.count == 1);
        return true;
    }
    return false;
}

static bool is_line(const RawPath::Iter::Rec& rec) {
    if (rec.verb == PathVerb::line) {
        REQUIRE(rec.count == 1);
        return true;
    }
    return false;
}

static bool is_quad(const RawPath::Iter::Rec& rec) {
    if (rec.verb == PathVerb::quad) {
        REQUIRE(rec.count == 2);
        return true;
    }
    return false;
}

static bool is_cubic(const RawPath::Iter::Rec& rec) {
    if (rec.verb == PathVerb::cubic) {
        REQUIRE(rec.count == 3);
        return true;
    }
    return false;
}

static bool is_close(const RawPath::Iter::Rec& rec) {
    if (rec.verb == PathVerb::close) {
        REQUIRE(rec.count == 0);
        return true;
    }
    return false;
}

// clang-format off
static inline bool eq(Vec2D p, float x, float y) {
    return p.x == x && p.y == y;
}
// clang-format on

TEST_CASE("rawpath-iter", "[rawpath]") {
    {
        RawPath rp;
        RawPath::Iter iter(rp);
        REQUIRE(iter.next() == false);
        REQUIRE(iter.next() == false); // should be safe to call again
    }
    {
        RawPath rp;
        rp.moveTo(1, 2);
        rp.lineTo(3, 4);
        rp.quadTo(5, 6, 7, 8);
        rp.cubicTo(9, 10, 11, 12, 13, 14);
        rp.close();
        RawPath::Iter iter(rp);
        auto rec = iter.next();
        REQUIRE((rec && is_move(rec) && eq(rec.pts[0], 1, 2)));
        rec = iter.next();
        REQUIRE((rec && is_line(rec) && eq(rec.pts[0], 3, 4)));
        rec = iter.next();
        REQUIRE((rec && is_quad(rec) && eq(rec.pts[0], 5, 6) && eq(rec.pts[1], 7, 8)));
        rec = iter.next();
        REQUIRE((rec && is_cubic(rec) && eq(rec.pts[0], 9, 10) && eq(rec.pts[1], 11, 12) &&
                 eq(rec.pts[2], 13, 14)));
        rec = iter.next();
        REQUIRE((rec && is_close(rec)));
        rec = iter.next();
        REQUIRE(rec == false);
        REQUIRE(iter.next() == false); // should be safe to call again
    }
}

TEST_CASE("isDone", "[rawpath::iter]") {
    RawPath rp;
    rp.moveTo(1, 2);
    rp.lineTo(3, 4);
    RawPath::Iter iter(rp);

    REQUIRE(!iter.isDone()); // moveTo
    REQUIRE(iter.next());

    REQUIRE(!iter.isDone()); // lineTo
    REQUIRE(iter.next());

    REQUIRE(iter.isDone()); // now we're done
    REQUIRE(!iter.next());
    REQUIRE(iter.isDone()); // ensure we 'still' think we're done
}

TEST_CASE("reset", "[rawpath]") {
    RawPath path;
    path.moveTo(1, 2);
    path.lineTo(3, 4);
    RawPath::Iter iter(path);
    auto rec = iter.next();
    REQUIRE((rec && is_move(rec) && eq(rec.pts[0], 1, 2)));
    rec = iter.next();
    REQUIRE((rec && is_line(rec) && eq(rec.pts[0], 3, 4)));
    REQUIRE(!iter.next());

    // now change the path (not required for the test per-se)
    path = RawPath();
    path.moveTo(0, 0);
    path.close();

    iter.reset(path);
    rec = iter.next();
    REQUIRE((rec && is_move(rec) && eq(rec.pts[0], 0, 0)));
    rec = iter.next();
    REQUIRE((rec && is_close(rec)));
    REQUIRE(!iter.next());
}

TEST_CASE("backup", "[rawpath]") {
    RawPath rp;
    rp.moveTo(1, 2);
    rp.lineTo(3, 4);
    rp.close();
    RawPath::Iter iter(rp);

    auto rec = iter.next();
    REQUIRE((rec && is_move(rec) && eq(rec.pts[0], 1, 2)));
    const Vec2D* move_pts = rec.pts;

    rec = iter.next();
    REQUIRE((rec && is_line(rec) && eq(rec.pts[0], 3, 4)));
    const Vec2D* line_pts = rec.pts;

    rec = iter.next();
    REQUIRE((rec && is_close(rec)));

    rec = iter.next();
    REQUIRE(!rec);

    // Now try backing up

    iter.backUp(); // go back to 'close'
    rec = iter.next();
    REQUIRE((rec && is_close(rec)));

    iter.backUp(); // go back to 'close'
    iter.backUp(); // go back to 'line'
    rec = iter.next();
    REQUIRE((rec && is_line(rec) && eq(rec.pts[0], 3, 4)));
    REQUIRE(rec.pts == line_pts);

    iter.backUp(); // go back to 'line'
    iter.backUp(); // go back to 'move'
    rec = iter.next();
    REQUIRE((rec && is_move(rec) && eq(rec.pts[0], 1, 2)));
    REQUIRE(rec.pts == move_pts);
}

TEST_CASE("addPath", "[rawpath]") {
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
        if (mx) {
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

    for (auto i = 0; i < N; ++i) {
        for (auto j = 0; j < N; ++j) {
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
