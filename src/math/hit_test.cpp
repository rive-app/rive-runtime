/*
 * Copyright 2022 Rive
 */

#include "rive/math/hit_test.hpp"

#include "rive/math/mat2d.hpp"
#include <algorithm>
#include <assert.h>
#include <cmath>

// Should we make this an option at runtime?
#define CULL_BOUNDS true

using namespace rive;

static inline float graphics_roundf(float x) { return std::floor(x + 0.5f); }

static inline int graphics_round(float x) { return (int)graphics_roundf(x); }

struct Point
{
    float x, y;

    Point() {}
    Point(float xx, float yy) : x(xx), y(yy) {}
    Point(const Vec2D& src) : x(src.x), y(src.y) {}

    Point operator+(Point v) const { return {x + v.x, y + v.y}; }
    Point operator-(Point v) const { return {x - v.x, y - v.y}; }

    Point& operator+=(Point v)
    {
        *this = *this + v;
        return *this;
    }
    Point& operator-=(Point v)
    {
        *this = *this - v;
        return *this;
    }

    friend Point operator*(Point v, float s) { return {v.x * s, v.y * s}; }
    friend Point operator*(float s, Point v) { return {v.x * s, v.y * s}; }
};

template <typename T> T lerp(T a, T b, float t) { return a + (b - a) * t; }

template <typename T> T ave(T a, T b) { return lerp(a, b, 0.5f); }

static void append_line(const float height,
                        Point p0,
                        Point p1,
                        float m,
                        int winding,
                        int delta[],
                        int iwidth)
{
    assert(winding == 1 || winding == -1);

    int top = graphics_round(p0.y);
    int bottom = graphics_round(p1.y);
    if (top == bottom)
    {
        return;
    }

    assert(top < bottom);
    assert(top >= 0);
    assert((float)bottom <= height);

    // we add 0.5 at the end to pre-round the values
    float x = p0.x + m * (top - p0.y + 0.5f) + 0.5f;

    int* row = delta + top * iwidth;
    for (int y = top; y < bottom; ++y)
    {
        int ix = (int)std::max(x, 0.0f);
        if (ix < iwidth)
        {
            row[ix] += winding;
        }
        x += m;
        row += iwidth;
    }
}

static void clip_line(const float height, Point p0, Point p1, int delta[], const int iwidth)
{
    if (p0.y == p1.y)
    {
        return;
    }

    int winding = 1;
    if (p0.y > p1.y)
    {
        winding = -1;
        std::swap(p0, p1);
    }
    // now we're monotonic in Y: p0 <= p1
    if (p1.y <= 0 || p0.y >= height)
    {
        return;
    }

    const float m = (float)(p1.x - p0.x) / (p1.y - p0.y);
    if (p0.y < 0)
    {
        p0.x += m * (0 - p0.y);
        p0.y = 0;
    }
    if (p1.y > height)
    {
        p1.x += m * (height - p1.y);
        p1.y = height;
    }

    assert(p0.y <= p1.y);
    assert(p0.y >= 0);
    assert(p1.y <= height);

    append_line(height, p0, p1, m, winding, delta, iwidth);
}

#define MAX_CURVE_SEGMENTS (1 << 8)

static int compute_cubic_segments(Point a, Point b, Point c, Point d)
{
    Point abc = a - b - b + c;
    Point bcd = b - c - c + d;
    float dx = std::max(std::abs(abc.x), std::abs(bcd.x));
    float dy = std::max(std::abs(abc.y), std::abs(bcd.y));
    float dist = sqrtf(dx * dx + dy * dy);
    // count = sqrt(6*dist / 8*tol)
    // tol = 0.25
    // count = sqrt(3*dist)
    float count = sqrtf(3 * dist);
    return std::max(1, std::min((int)ceilf(count), MAX_CURVE_SEGMENTS));
}

// cubic a(1-t)^3 + 3bt(1-t)^2 + 3c(1-t)t^2 + dt^3
// becomes
// At^3 + Bt^2 + Ct + D
//
struct CubicCoeff
{
    Point A, B, C, D;

    // a(1-t)^3 + 3bt(1-t)^2 + 3ct^2(1-t) + dt^3
    // a - 3at + 3at^2 -  at^3      a(1 - 3t + 3t^2 - t^3)
    //     3bt - 6bt^2 + 3bt^3     3b(     t - 2t^2 + t^3)
    //           3ct^2 - 3ct^3     3c(          t^2 - t^3)
    //                    dt^3      d(                t^3)
    // ...
    // D  + Ct  + Bt^2  + At^3
    //
    CubicCoeff(Point a, Point b, Point c, Point d)
    {
        A = (d - a) + 3.0f * (b - c);
        B = 3.0f * ((c - b) + (a - b));
        C = 3.0f * (b - a);
        D = a;
    }

    Point eval(float t) const { return ((A * t + B) * t + C) * t + D; }
};

////////////////////////////////////////////

void HitTester::reset() { m_DW.clear(); }

void HitTester::reset(const IAABB& clip)
{
    m_offset = Vec2D{(float)clip.left, (float)clip.top};
    m_height = (float)clip.height();

    m_IWidth = clip.width();
    m_IHeight = clip.height();
    m_DW.resize(m_IWidth * m_IHeight);
    for (size_t i = 0; i < m_DW.size(); ++i)
    {
        m_DW[i] = 0;
    }

    m_ExpectsMove = true;
}

void HitTester::move(Vec2D v)
{
    if (!m_ExpectsMove)
    {
        this->close();
    }
    m_First = m_Prev = v - m_offset;
    m_ExpectsMove = false;
}

void HitTester::line(Vec2D v)
{
    assert(!m_ExpectsMove);

    v = v - m_offset;
    clip_line(m_height, m_Prev, v, m_DW.data(), m_IWidth);
    m_Prev = v;
}

void HitTester::quad(Vec2D b, Vec2D c)
{
    assert(!m_ExpectsMove);

    m_Prev = c;
}

static bool quickRejectCubic(float height, Point a, Point b, Point c, Point d)
{
    const float h = height;
    return (a.y <= 0 && b.y <= 0 && c.y <= 0 && d.y <= 0) ||
           (a.y >= h && b.y >= h && c.y >= h && d.y >= h);
}

struct CubicChop
{
    Vec2D storage[7];

    CubicChop(Vec2D a, Vec2D b, Vec2D c, Vec2D d)
    {
        auto ab = ave(a, b);
        auto bc = ave(b, c);
        auto cd = ave(c, d);
        auto abc = ave(ab, bc);
        auto bcd = ave(bc, cd);

        storage[0] = a;
        storage[1] = ab;
        storage[2] = abc;
        storage[3] = ave(abc, bcd);
        storage[4] = bcd;
        storage[5] = cd;
        storage[6] = d;
    }

    Vec2D operator[](unsigned index) const
    {
        assert(index < 7);
        return storage[index];
    }
};

// Trial and error to pick a good value for this.
//
// Subdivision and recursion have their own cost, so at some point
// just evaluating the cubic (count) times is cheaper than continuing
// to chop.
//
// The key win is quickRejectCubic. This is how we save time over just
// evaluating the cubic up front.
//
#define MAX_LOCAL_SEGMENTS 16

void HitTester::recurse_cubic(Vec2D b, Vec2D c, Vec2D d, int count)
{
    if (quickRejectCubic(m_height, m_Prev, b, c, d))
    {
        m_Prev = d;
        return;
    }

    if (count > MAX_LOCAL_SEGMENTS)
    {
        CubicChop chop(m_Prev, b, c, d);
        const int newCount = (count + 1) >> 1;
        assert(newCount < count);
        this->recurse_cubic(chop[1], chop[2], chop[3], newCount);
        this->recurse_cubic(chop[4], chop[5], chop[6], newCount);
    }
    else
    {
        const float dt = 1.0f / (float)count;
        float t = dt;

        CubicCoeff cube(m_Prev, b, c, d);
        // we don't need the first point eval(0) or the last eval(1)
        Point prev = m_Prev;
        for (int i = 1; i < count - 1; ++i)
        {
            auto next = cube.eval(t);
            clip_line(m_height, prev, next, m_DW.data(), m_IWidth);
            prev = next;
            t += dt;
        }
        clip_line(m_height, prev, d, m_DW.data(), m_IWidth);
        m_Prev = d;
    }
}
void HitTester::cubic(Vec2D b, Vec2D c, Vec2D d)
{
    assert(!m_ExpectsMove);

    b = b - m_offset;
    c = c - m_offset;
    d = d - m_offset;

    if (quickRejectCubic(m_height, m_Prev, b, c, d))
    {
        m_Prev = d;
        return;
    }

    const int count = compute_cubic_segments(m_Prev, b, c, d);

    this->recurse_cubic(b, c, d, count);
}

void HitTester::close()
{
    assert(!m_ExpectsMove);

    clip_line(m_height, m_Prev, m_First, m_DW.data(), m_IWidth);
    m_ExpectsMove = true;
}

void HitTester::addRect(const AABB& rect, const Mat2D& xform, PathDirection dir)
{
    const Vec2D pts[] = {
        xform * Vec2D{rect.left(), rect.top()},
        xform * Vec2D{rect.right(), rect.top()},
        xform * Vec2D{rect.right(), rect.bottom()},
        xform * Vec2D{rect.left(), rect.bottom()},
    };

    move(pts[0]);
    if (dir == PathDirection::clockwise)
    {
        line(pts[1]);
        line(pts[2]);
        line(pts[3]);
    }
    else
    {
        line(pts[3]);
        line(pts[2]);
        line(pts[1]);
    }
    close();
}

bool HitTester::test(FillRule rule)
{
    if (!m_ExpectsMove)
    {
        this->close();
    }

    const int mask = (rule == rive::FillRule::nonZero) ? -1 : 1;

    int nonzero = 0;
    for (auto m : m_DW)
    {
        nonzero |= (m & mask);
    }
    return nonzero != 0;
}

/////////////////////////

static bool cross_lt(Vec2D a, Vec2D b) { return a.x * b.y < a.y * b.x; }

bool HitTester::testMesh(Vec2D pt, Span<Vec2D> verts, Span<uint16_t> indices)
{
    if (verts.size() < 3)
    {
        return false;
    }

    // Test against the bounds of the entire mesh
    // Make this optional?
    if (CULL_BOUNDS)
    {
        const auto bounds = AABB(verts);

        if (bounds.bottom() < pt.y || pt.y < bounds.top() || bounds.right() < pt.x ||
            pt.x < bounds.left())
        {
            return false;
        }
    }

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        const auto a = verts[indices[i + 0]];
        const auto b = verts[indices[i + 1]];
        const auto c = verts[indices[i + 2]];

        auto pa = a - pt;
        auto pb = b - pt;
        auto pc = c - pt;

        auto ab = cross_lt(pa, pb);
        auto bc = cross_lt(pb, pc);
        auto ca = cross_lt(pc, pa);

        if (ab == bc && ab == ca)
        {
            return true;
        }
    }
    return false;
}

bool HitTester::testMesh(const IAABB& area, Span<Vec2D> verts, Span<uint16_t> indices)
{
    // this version can give slightly different results, so perhaps we should do this
    // automatically, ... its just much faster if we do.
    if (area.width() * area.height() == 1)
    {
        return testMesh(Vec2D((float)area.left, (float)area.top), verts, indices);
    }

    if (verts.size() < 3)
    {
        return false;
    }

    // Test against the bounds of the entire mesh
    // Make this optional?
    if (CULL_BOUNDS)
    {
        const auto bounds = AABB(verts);

        if (bounds.bottom() <= area.top || area.bottom <= bounds.top() ||
            bounds.right() <= area.left || area.right <= bounds.left())
        {
            return false;
        }
    }

    std::vector<int> windings(area.width() * area.height());
    const auto offset = Vec2D((float)area.left, (float)area.top);
    int* deltas = windings.data();

    for (size_t i = 0; i < indices.size(); i += 3)
    {
        const auto a = verts[indices[i + 0]] - offset;
        const auto b = verts[indices[i + 1]] - offset;
        const auto c = verts[indices[i + 2]] - offset;

        clip_line((float)area.height(), a, b, deltas, area.width());
        clip_line((float)area.height(), b, c, deltas, area.width());
        clip_line((float)area.height(), c, a, deltas, area.width());

        int nonzero = 0;
        for (auto w : windings)
        {
            nonzero |= w;
        }
        if (nonzero)
        {
            return true;
        }
    }
    return false;
}
