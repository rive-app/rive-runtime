/*
 * Copyright 2015 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Initial import from
 * skia:c2a399a74da523ec445f1202367764d04b5df2ec@src/gpu/ganesh/geometry/GrTriangulator.h
 *
 * Copyright 2023 Rive
 */

#include "gr_triangulator.hpp"

#include <algorithm>

#if !defined(SK_ENABLE_OPTIMIZE_SIZE)

#if TRIANGULATOR_LOGGING
#define TESS_LOG printf
#define DUMP_MESH(M) (M).dump()
#else
#define TESS_LOG(...)
#define DUMP_MESH(M)
#endif

namespace rive
{
using EdgeType = GrTriangulator::EdgeType;
using Vertex = GrTriangulator::Vertex;
using VertexList = GrTriangulator::VertexList;
using Line = GrTriangulator::Line;
using Edge = GrTriangulator::Edge;
using EdgeList = GrTriangulator::EdgeList;
using Poly = GrTriangulator::Poly;
using MonotonePoly = GrTriangulator::MonotonePoly;
using Comparator = GrTriangulator::Comparator;

static bool is_finite(Vec2D pt)
{
    float accum = 0;
    accum *= pt.x;
    accum *= pt.y;

    // accum is either NaN or it is finite (zero).
    assert(0 == accum || std::isnan(accum));

    // value==value will be true iff value is not NaN
    // TODO: is it faster to say !accum or accum==accum?
    return !std::isnan(accum);
}

template <class T, T* T::*Prev, T* T::*Next>
static void list_insert(T* t, T* prev, T* next, T** head, T** tail)
{
    t->*Prev = prev;
    t->*Next = next;
    if (prev)
    {
        prev->*Next = t;
    }
    else if (head)
    {
        *head = t;
    }
    if (next)
    {
        next->*Prev = t;
    }
    else if (tail)
    {
        *tail = t;
    }
}

template <class T, T* T::*Prev, T* T::*Next> static void list_remove(T* t, T** head, T** tail)
{
    if (t->*Prev)
    {
        t->*Prev->*Next = t->*Next;
    }
    else if (head)
    {
        *head = t->*Next;
    }
    if (t->*Next)
    {
        t->*Next->*Prev = t->*Prev;
    }
    else if (tail)
    {
        *tail = t->*Prev;
    }
    t->*Prev = t->*Next = nullptr;
}

typedef bool (*CompareFunc)(const Vec2D& a, const Vec2D& b);

static bool sweep_lt_horiz(const Vec2D& a, const Vec2D& b)
{
    return a.x < b.x || (a.x == b.x && a.y > b.y);
}

static bool sweep_lt_vert(const Vec2D& a, const Vec2D& b)
{
    return a.y < b.y || (a.y == b.y && a.x < b.x);
}

bool GrTriangulator::Comparator::sweep_lt(const Vec2D& a, const Vec2D& b) const
{
    return fDirection == Direction::kHorizontal ? sweep_lt_horiz(a, b) : sweep_lt_vert(a, b);
}

static inline void emit_vertex(Vertex* v,
                               int winding,
                               uint16_t pathID,
                               gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory)
{
    // GrTriangulator and pls unfortunately have opposite winding senses.
    int16_t plsWeight = -winding;
    mappedMemory->emplace_back(v->fPoint, plsWeight, pathID);
}

static void emit_triangle(Vertex* v0,
                          Vertex* v1,
                          Vertex* v2,
                          int winding,
                          uint16_t pathID,
                          gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory)
{
    TESS_LOG("emit_triangle %g (%g, %g) %d\n", v0->fID, v0->fPoint.x, v0->fPoint.y, v0->fAlpha);
    TESS_LOG("              %g (%g, %g) %d\n", v1->fID, v1->fPoint.x, v1->fPoint.y, v1->fAlpha);
    TESS_LOG("              %g (%g, %g) %d\n", v2->fID, v2->fPoint.x, v2->fPoint.y, v2->fAlpha);
#if TESSELLATOR_WIREFRAME
    emit_vertex(v0, winding, pathID, mappedMemory);
    emit_vertex(v1, winding, pathID, mappedMemory);
    emit_vertex(v1, winding, pathID, mappedMemory);
    emit_vertex(v2, winding, pathID, mappedMemory);
    emit_vertex(v2, winding, pathID, mappedMemory);
    emit_vertex(v0, winding, pathID, mappedMemory);
#else
    emit_vertex(v0, winding, pathID, mappedMemory);
    emit_vertex(v1, winding, pathID, mappedMemory);
    emit_vertex(v2, winding, pathID, mappedMemory);
#endif
}

void GrTriangulator::VertexList::insert(Vertex* v, Vertex* prev, Vertex* next)
{
    list_insert<Vertex, &Vertex::fPrev, &Vertex::fNext>(v, prev, next, &fHead, &fTail);
}

void GrTriangulator::VertexList::remove(Vertex* v)
{
    list_remove<Vertex, &Vertex::fPrev, &Vertex::fNext>(v, &fHead, &fTail);
}

// Round to nearest quarter-pixel. This is used for screenspace tessellation.

#if 0
static inline void round(Vec2D* p)
{
    p->x = SkScalarRoundToScalar(p->x * 4.0f) * 0.25f;
    p->y = SkScalarRoundToScalar(p->y * 4.0f) * 0.25f;
}
#endif

static inline float double_to_clamped_scalar(double d)
{
    // Clamps large values to what's finitely representable when cast back to a float.
    static const double kMaxLimit = (double)std::numeric_limits<float>::max();
    // It's not perfect, but a using a value larger than float_min helps protect from denormalized
    // values and ill-conditions in intermediate calculations on coordinates.
    static const double kNearZeroLimit = 16 * (double)std::numeric_limits<float>::min();
    if (std::abs(d) < kNearZeroLimit)
    {
        d = 0.f;
    }
    return static_cast<float>(std::max(-kMaxLimit, std::min(d, kMaxLimit)));
}

#if 0
bool GrTriangulator::Line::intersect(const Line& other, Vec2D* point) const
{
    double denom = fA * other.fB - fB * other.fA;
    if (denom == 0.0)
    {
        return false;
    }
    double scale = 1.0 / denom;
    point->x = double_to_clamped_scalar((fB * other.fC - other.fB * fC) * scale);
    point->y = double_to_clamped_scalar((other.fA * fC - fA * other.fC) * scale);
    round(point);
    return point->isFinite();
}
#endif

// If the edge's vertices differ by many orders of magnitude, the computed line equation can have
// significant error in its distance and intersection tests. To avoid this, we recursively subdivide
// long edges and effectively perform a binary search to perform a more accurate intersection test.
static bool edge_line_needs_recursion(const Vec2D& p0, const Vec2D& p1)
{
    // ilogbf(0) returns an implementation-defined constant, but we are choosing to saturate
    // negative exponents to 0 for comparisons sake. We're only trying to recurse on lines with
    // very large coordinates.
    int expDiffX = std::abs((std::abs(p0.x) < 1.f ? 0 : std::ilogbf(p0.x)) -
                            (std::abs(p1.x) < 1.f ? 0 : std::ilogbf(p1.x)));
    int expDiffY = std::abs((std::abs(p0.y) < 1.f ? 0 : std::ilogbf(p0.y)) -
                            (std::abs(p1.y) < 1.f ? 0 : std::ilogbf(p1.y)));
    // Differ by more than 2^20, or roughly a factor of one million.
    return expDiffX > 20 || expDiffY > 20;
}

static bool recursive_edge_intersect(const Line& u,
                                     Vec2D u0,
                                     Vec2D u1,
                                     const Line& v,
                                     Vec2D v0,
                                     Vec2D v1,
                                     Vec2D* p,
                                     double* s,
                                     double* t)
{
    // First check if the bounding boxes of [u0,u1] intersects [v0,v1]. If they do not, then the
    // two line segments cannot intersect in their domain (even if the lines themselves might).
    // - don't use AABB::intersect since the vertices aren't sorted and horiz/vertical lines
    //   appear as empty rects, which then never "intersect" according to AABB.
    if (std::min(u0.x, u1.x) > std::max(v0.x, v1.x) ||
        std::max(u0.x, u1.x) < std::min(v0.x, v1.x) ||
        std::min(u0.y, u1.y) > std::max(v0.y, v1.y) || std::max(u0.y, u1.y) < std::min(v0.y, v1.y))
    {
        return false;
    }

    // Compute intersection based on current segment vertices; if an intersection is found but the
    // vertices differ too much in magnitude, we recurse using the midpoint of the segment to
    // reject false positives. We don't currently try to avoid false negatives (e.g. large magnitude
    // line reports no intersection but there is one).
    double denom = u.fA * v.fB - u.fB * v.fA;
    if (denom == 0.0)
    {
        return false;
    }
    double dx = static_cast<double>(v0.x) - u0.x;
    double dy = static_cast<double>(v0.y) - u0.y;
    double sNumer = dy * v.fB + dx * v.fA;
    double tNumer = dy * u.fB + dx * u.fA;
    // If (sNumer / denom) or (tNumer / denom) is not in [0..1], exit early.
    // This saves us doing the divide below unless absolutely necessary.
    if (denom > 0.0 ? (sNumer < 0.0 || sNumer > denom || tNumer < 0.0 || tNumer > denom)
                    : (sNumer > 0.0 || sNumer < denom || tNumer > 0.0 || tNumer < denom))
    {
        return false;
    }

    *s = sNumer / denom;
    *t = tNumer / denom;
    assert(*s >= 0.0 && *s <= 1.0 && *t >= 0.0 && *t <= 1.0);

    const bool uNeedsSplit = edge_line_needs_recursion(u0, u1);
    const bool vNeedsSplit = edge_line_needs_recursion(v0, v1);
    if (!uNeedsSplit && !vNeedsSplit)
    {
        p->x = double_to_clamped_scalar(u0.x - (*s) * u.fB);
        p->y = double_to_clamped_scalar(u0.y + (*s) * u.fA);
        return true;
    }
    else
    {
        double sScale = 1.0, sShift = 0.0;
        double tScale = 1.0, tShift = 0.0;

        if (uNeedsSplit)
        {
            Vec2D uM = {(float)(0.5 * u0.x + 0.5 * u1.x), (float)(0.5 * u0.y + 0.5 * u1.y)};
            sScale = 0.5;
            if (*s >= 0.5)
            {
                u0 = uM;
                sShift = 0.5;
            }
            else
            {
                u1 = uM;
            }
        }
        if (vNeedsSplit)
        {
            Vec2D vM = {(float)(0.5 * v0.x + 0.5 * v1.x), (float)(0.5 * v0.y + 0.5 * v1.y)};
            tScale = 0.5;
            if (*t >= 0.5)
            {
                v0 = vM;
                tShift = 0.5;
            }
            else
            {
                v1 = vM;
            }
        }

        // Just recompute both lines, even if only one was split; we're already in a slow path.
        if (recursive_edge_intersect(Line(u0, u1), u0, u1, Line(v0, v1), v0, v1, p, s, t))
        {
            // Adjust s and t back to full range
            *s = sScale * (*s) + sShift;
            *t = tScale * (*t) + tShift;
            return true;
        }
        else
        {
            // False positive
            return false;
        }
    }
}

bool GrTriangulator::Edge::intersect(const Edge& other, Vec2D* p, uint8_t* alpha) const
{
    TESS_LOG("intersecting %g -> %g with %g -> %g\n",
             fTop->fID,
             fBottom->fID,
             other.fTop->fID,
             other.fBottom->fID);
    if (fTop == other.fTop || fBottom == other.fBottom || fTop == other.fBottom ||
        fBottom == other.fTop)
    {
        // If the two edges share a vertex by construction, they have already been split and
        // shouldn't be considered "intersecting" anymore.
        return false;
    }

    double s, t; // needed to interpolate vertex alpha
    const bool intersects = recursive_edge_intersect(fLine,
                                                     fTop->fPoint,
                                                     fBottom->fPoint,
                                                     other.fLine,
                                                     other.fTop->fPoint,
                                                     other.fBottom->fPoint,
                                                     p,
                                                     &s,
                                                     &t);
    if (!intersects)
    {
        return false;
    }

    if (alpha)
    {
        if (fType == EdgeType::kInner || other.fType == EdgeType::kInner)
        {
            // If the intersection is on any interior edge, it needs to stay fully opaque or later
            // triangulation could leech transparency into the inner fill region.
            *alpha = 255;
        }
        else if (fType == EdgeType::kOuter && other.fType == EdgeType::kOuter)
        {
            // Trivially, the intersection will be fully transparent since since it is by
            // construction on the outer edge.
            *alpha = 0;
        }
        else
        {
            // Could be two connectors crossing, or a connector crossing an outer edge.
            // Take the max interpolated alpha
            assert(fType == EdgeType::kConnector || other.fType == EdgeType::kConnector);
            *alpha = std::max((1.0 - s) * fTop->fAlpha + s * fBottom->fAlpha,
                              (1.0 - t) * other.fTop->fAlpha + t * other.fBottom->fAlpha);
        }
    }
    return true;
}

void GrTriangulator::EdgeList::insert(Edge* edge, Edge* prev, Edge* next)
{
    list_insert<Edge, &Edge::fLeft, &Edge::fRight>(edge, prev, next, &fHead, &fTail);
}

bool GrTriangulator::EdgeList::remove(Edge* edge)
{
    TESS_LOG("removing edge %g -> %g\n", edge->fTop->fID, edge->fBottom->fID);
    // assert(this->contains(edge));  // Leave this here for future debugging.
    if (!this->contains(edge))
    {
        return false;
    }
    list_remove<Edge, &Edge::fLeft, &Edge::fRight>(edge, &fHead, &fTail);
    return true;
}

void GrTriangulator::MonotonePoly::addEdge(Edge* edge)
{
    if (fSide == kRight_Side)
    {
        assert(!edge->fUsedInRightPoly);
        list_insert<Edge, &Edge::fRightPolyPrev, &Edge::fRightPolyNext>(edge,
                                                                        fLastEdge,
                                                                        nullptr,
                                                                        &fFirstEdge,
                                                                        &fLastEdge);
        edge->fUsedInRightPoly = true;
    }
    else
    {
        assert(!edge->fUsedInLeftPoly);
        list_insert<Edge, &Edge::fLeftPolyPrev, &Edge::fLeftPolyNext>(edge,
                                                                      fLastEdge,
                                                                      nullptr,
                                                                      &fFirstEdge,
                                                                      &fLastEdge);
        edge->fUsedInLeftPoly = true;
    }
}

void GrTriangulator::emitMonotonePoly(
    const MonotonePoly* monotonePoly,
    uint16_t pathID,
    bool reverseTriangles,
    gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
{
    assert(monotonePoly->fWinding != 0);
    Edge* e = monotonePoly->fFirstEdge;
    VertexList vertices;
    vertices.append(e->fTop);
    int count = 1;
    while (e != nullptr)
    {
        if (kRight_Side == monotonePoly->fSide)
        {
            vertices.append(e->fBottom);
            e = e->fRightPolyNext;
        }
        else
        {
            vertices.prepend(e->fBottom);
            e = e->fLeftPolyNext;
        }
        count++;
    }
    Vertex* first = vertices.fHead;
    Vertex* v = first->fNext;
    while (v != vertices.fTail)
    {
        assert(v && v->fPrev && v->fNext);
        Vertex* prev = v->fPrev;
        Vertex* curr = v;
        Vertex* next = v->fNext;
        if (count == 3)
        {
            return emitTriangle(prev,
                                curr,
                                next,
                                monotonePoly->fWinding,
                                pathID,
                                reverseTriangles,
                                mappedMemory);
        }
        double ax = static_cast<double>(curr->fPoint.x) - prev->fPoint.x;
        double ay = static_cast<double>(curr->fPoint.y) - prev->fPoint.y;
        double bx = static_cast<double>(next->fPoint.x) - curr->fPoint.x;
        double by = static_cast<double>(next->fPoint.y) - curr->fPoint.y;
        if (ax * by - ay * bx >= 0.0)
        {
            emitTriangle(prev,
                         curr,
                         next,
                         monotonePoly->fWinding,
                         pathID,
                         reverseTriangles,
                         mappedMemory);
            v->fPrev->fNext = v->fNext;
            v->fNext->fPrev = v->fPrev;
            count--;
            if (v->fPrev == first)
            {
                v = v->fNext;
            }
            else
            {
                v = v->fPrev;
            }
        }
        else
        {
            v = v->fNext;
        }
    }
}

void GrTriangulator::emitTriangle(
    Vertex* prev,
    Vertex* curr,
    Vertex* next,
    int winding,
    uint16_t pathID,
    bool reverseTriangles,
    gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
{
    if (reverseTriangles)
    {
        std::swap(prev, next);
    }
    return emit_triangle(prev, curr, next, winding, pathID, mappedMemory);
}

GrTriangulator::Poly::Poly(Vertex* v, int winding) :
    fFirstVertex(v),
    fWinding(winding),
    fHead(nullptr),
    fTail(nullptr),
    fNext(nullptr),
    fPartner(nullptr),
    fCount(0)
{
#if TRIANGULATOR_LOGGING
    static int gID = 0;
    fID = gID++;
    TESS_LOG("*** created Poly %d\n", fID);
#endif
}

Poly* GrTriangulator::Poly::addEdge(Edge* e, Side side, GrTriangulator* tri)
{
    TESS_LOG("addEdge (%g -> %g) to poly %d, %s side\n",
             e->fTop->fID,
             e->fBottom->fID,
             fID,
             side == kLeft_Side ? "left" : "right");
    Poly* partner = fPartner;
    Poly* poly = this;
    if (side == kRight_Side)
    {
        if (e->fUsedInRightPoly)
        {
            return this;
        }
    }
    else
    {
        if (e->fUsedInLeftPoly)
        {
            return this;
        }
    }
    if (partner)
    {
        fPartner = partner->fPartner = nullptr;
    }
    if (!fTail)
    {
        fHead = fTail = tri->allocateMonotonePoly(e, side, fWinding);
        fCount += 2;
    }
    else if (e->fBottom == fTail->fLastEdge->fBottom)
    {
        return poly;
    }
    else if (side == fTail->fSide)
    {
        fTail->addEdge(e);
        fCount++;
    }
    else
    {
        e = tri->allocateEdge(fTail->fLastEdge->fBottom, e->fBottom, 1, EdgeType::kInner);
        fTail->addEdge(e);
        fCount++;
        if (partner)
        {
            partner->addEdge(e, side, tri);
            poly = partner;
        }
        else
        {
            MonotonePoly* m = tri->allocateMonotonePoly(e, side, fWinding);
            m->fPrev = fTail;
            fTail->fNext = m;
            fTail = m;
        }
    }
    return poly;
}
void GrTriangulator::emitPoly(const Poly* poly,
                              uint16_t pathID,
                              bool reverseTriangles,
                              gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
{
    if (poly->fCount < 3)
    {
        return;
    }
    TESS_LOG("emit() %d, size %d\n", poly->fID, poly->fCount);
    for (MonotonePoly* m = poly->fHead; m != nullptr; m = m->fNext)
    {
        emitMonotonePoly(m, pathID, reverseTriangles, mappedMemory);
        ;
    }
}

static bool coincident(const Vec2D& a, const Vec2D& b) { return a == b; }

Poly* GrTriangulator::makePoly(Poly** head, Vertex* v, int winding) const
{
    Poly* poly = fAlloc->make<Poly>(v, winding);
    poly->fNext = *head;
    *head = poly;
    return poly;
}

void GrTriangulator::appendPointToContour(const Vec2D& p, VertexList* contour) const
{
    Vertex* v = fAlloc->make<Vertex>(p, 255);
#if TRIANGULATOR_LOGGING
    static float gID = 0.0f;
    v->fID = gID++;
#endif
    contour->append(v);
}

#if 0
static float quad_error_at(const Vec2D pts[3], float t, float u)
{
    SkQuadCoeff quad(pts);
    Vec2D p0 = to_point(quad.eval(t - 0.5f * u));
    Vec2D mid = to_point(quad.eval(t));
    Vec2D p1 = to_point(quad.eval(t + 0.5f * u));
    if (!p0.isFinite() || !mid.isFinite() || !p1.isFinite())
    {
        return 0;
    }
    return SkPointPriv::DistanceToLineSegmentBetweenSqd(mid, p0, p1);
}

void GrTriangulator::appendQuadraticToContour(const Vec2D pts[3],
                                              float toleranceSqd,
                                              VertexList* contour) const
{
    SkQuadCoeff quad(pts);
    skvx::float2 aa = quad.fA * quad.fA;
    float denom = 2.0f * (aa[0] + aa[1]);
    skvx::float2 ab = quad.fA * quad.fB;
    float t = denom ? (-ab[0] - ab[1]) / denom : 0.0f;
    int nPoints = 1;
    float u = 1.0f;
    // Test possible subdivision values only at the point of maximum curvature.
    // If it passes the flatness metric there, it'll pass everywhere.
    while (nPoints < GrPathUtils::kMaxPointsPerCurve)
    {
        u = 1.0f / nPoints;
        if (quad_error_at(pts, t, u) < toleranceSqd)
        {
            break;
        }
        nPoints++;
    }
    for (int j = 1; j <= nPoints; j++)
    {
        this->appendPointToContour(to_point(quad.eval(j * u)), contour);
    }
}

void GrTriangulator::generateCubicPoints(const Vec2D& p0,
                                         const Vec2D& p1,
                                         const Vec2D& p2,
                                         const Vec2D& p3,
                                         float tolSqd,
                                         VertexList* contour,
                                         int pointsLeft) const
{
    float d1 = SkPointPriv::DistanceToLineSegmentBetweenSqd(p1, p0, p3);
    float d2 = SkPointPriv::DistanceToLineSegmentBetweenSqd(p2, p0, p3);
    if (pointsLeft < 2 || (d1 < tolSqd && d2 < tolSqd) || !SkIsFinite(d1, d2))
    {
        this->appendPointToContour(p3, contour);
        return;
    }
    const Vec2D q[] = {{SkScalarAve(p0.x, p1.x), SkScalarAve(p0.y, p1.y)},
                       {SkScalarAve(p1.x, p2.x), SkScalarAve(p1.y, p2.y)},
                       {SkScalarAve(p2.x, p3.x), SkScalarAve(p2.y, p3.y)}};
    const Vec2D r[] = {{SkScalarAve(q[0].x, q[1].x), SkScalarAve(q[0].y, q[1].y)},
                       {SkScalarAve(q[1].x, q[2].x), SkScalarAve(q[1].y, q[2].y)}};
    const Vec2D s = {SkScalarAve(r[0].x, r[1].x), SkScalarAve(r[0].y, r[1].y)};
    pointsLeft >>= 1;
    this->generateCubicPoints(p0, q[0], r[0], s, tolSqd, contour, pointsLeft);
    this->generateCubicPoints(s, r[1], q[2], p3, tolSqd, contour, pointsLeft);
}
#endif

// Stage 1: convert the input path to a set of linear contours (linked list of Vertices).

void GrTriangulator::pathToContours(const RawPath& path,
                                    float tolerance,
                                    const AABB& clipBounds,
                                    VertexList* contours,
                                    bool* isLinear) const
{
#if 0
    float toleranceSqd = tolerance * tolerance;
    Vec2D pts[4];
#endif
    *isLinear = true;
    VertexList* contour = contours;
#if 0
    RawPath::Iter iter(fPath, false);
    if (path.isInverseFillType())
    {
        Vec2D quad[4];
        clipBounds.toQuad(quad);
        for (int i = 3; i >= 0; i--)
        {
            this->appendPointToContour(quad[i], contours);
        }
        contour++;
    }
    SkAutoConicToQuads converter;
#endif
    for (const auto [verb, pts] : path)
    {
        switch (verb)
        {
#if 0
            case SkPath::kConic_Verb:
            {
                *isLinear = false;
                if (toleranceSqd == 0)
                {
                    this->appendPointToContour(pts[2], contour);
                    break;
                }
                float weight = iter.conicWeight();
                const Vec2D* quadPts = converter.computeQuads(pts, weight, toleranceSqd);
                for (int i = 0; i < converter.countQuads(); ++i)
                {
                    this->appendQuadraticToContour(quadPts, toleranceSqd, contour);
                    quadPts += 2;
                }
                break;
            }
#endif
            case PathVerb::move:
                if (contour->fHead)
                {
                    contour++;
                }
                if (is_finite(pts[0]))
                {
                    this->appendPointToContour(pts[0], contour);
                }
                break;
            case PathVerb::line:
            {
                if (is_finite(pts[1]))
                {
                    this->appendPointToContour(pts[1], contour);
                }
                break;
            }
            case PathVerb::quad:
            {
#if 0
                *isLinear = false;
                if (toleranceSqd == 0)
                {
                    this->appendPointToContour(pts[2], contour);
                    break;
                }
                this->appendQuadraticToContour(pts, toleranceSqd, contour);
                break;
#else
                RIVE_UNREACHABLE();
#endif
            }
            case PathVerb::cubic:
            {
#if 0
                *isLinear = false;
                if (toleranceSqd == 0)
                {
                    this->appendPointToContour(pts[3], contour);
                    break;
                }
                int pointsLeft = GrPathUtils::cubicPointCount(pts, tolerance);
                this->generateCubicPoints(pts[0],
                                          pts[1],
                                          pts[2],
                                          pts[3],
                                          toleranceSqd,
                                          contour,
                                          pointsLeft);
                break;
#else
                RIVE_UNREACHABLE();
#endif
            }
            case PathVerb::close:
                break;
        }
    }
}

static inline bool apply_fill_type(FillRule fillRule, int winding)
{
    switch (fillRule)
    {
        case FillRule::nonZero:
            return winding != 0;
        case FillRule::evenOdd:
            return (winding & 1) != 0;
        default:
            RIVE_UNREACHABLE();
    }
}

bool GrTriangulator::applyFillType(int winding) const
{
    return apply_fill_type(fFillRule, winding);
}

static inline bool apply_fill_type(FillRule fillType, const Poly* poly)
{
    return poly && apply_fill_type(fillType, poly->fWinding);
}

MonotonePoly* GrTriangulator::allocateMonotonePoly(Edge* edge, Side side, int winding)
{
    ++fNumMonotonePolys;
    return fAlloc->make<MonotonePoly>(edge, side, winding);
}

Edge* GrTriangulator::allocateEdge(Vertex* top, Vertex* bottom, int winding, EdgeType type)
{
    ++fNumEdges;
    return fAlloc->make<Edge>(top, bottom, winding, type);
}

Edge* GrTriangulator::makeEdge(Vertex* prev, Vertex* next, EdgeType type, const Comparator& c)
{
    assert(prev->fPoint != next->fPoint);
    int winding = c.sweep_lt(prev->fPoint, next->fPoint) ? 1 : -1;
    Vertex* top = winding < 0 ? next : prev;
    Vertex* bottom = winding < 0 ? prev : next;
    return this->allocateEdge(top, bottom, winding, type);
}

bool EdgeList::insert(Edge* edge, Edge* prev)
{
    TESS_LOG("inserting edge %g -> %g\n", edge->fTop->fID, edge->fBottom->fID);
    // assert(!this->contains(edge));  // Leave this here for debugging.
    if (this->contains(edge))
    {
        return false;
    }
    Edge* next = prev ? prev->fRight : fHead;
    this->insert(edge, prev, next);
    return true;
}

void GrTriangulator::FindEnclosingEdges(const Vertex& v,
                                        const EdgeList& edges,
                                        Edge** left,
                                        Edge** right)
{
    if (v.fFirstEdgeAbove && v.fLastEdgeAbove)
    {
        *left = v.fFirstEdgeAbove->fLeft;
        *right = v.fLastEdgeAbove->fRight;
        return;
    }
    Edge* next = nullptr;
    Edge* prev;
    for (prev = edges.fTail; prev != nullptr; prev = prev->fLeft)
    {
        if (prev->isLeftOf(v))
        {
            break;
        }
        next = prev;
    }
    *left = prev;
    *right = next;
}

void GrTriangulator::Edge::insertAbove(Vertex* v, const Comparator& c)
{
    if (fTop->fPoint == fBottom->fPoint || c.sweep_lt(fBottom->fPoint, fTop->fPoint))
    {
        return;
    }
    TESS_LOG("insert edge (%g -> %g) above vertex %g\n", fTop->fID, fBottom->fID, v->fID);
    Edge* prev = nullptr;
    Edge* next;
    for (next = v->fFirstEdgeAbove; next; next = next->fNextEdgeAbove)
    {
        if (next->isRightOf(*fTop))
        {
            break;
        }
        prev = next;
    }
    list_insert<Edge, &Edge::fPrevEdgeAbove, &Edge::fNextEdgeAbove>(this,
                                                                    prev,
                                                                    next,
                                                                    &v->fFirstEdgeAbove,
                                                                    &v->fLastEdgeAbove);
}

void GrTriangulator::Edge::insertBelow(Vertex* v, const Comparator& c)
{
    if (fTop->fPoint == fBottom->fPoint || c.sweep_lt(fBottom->fPoint, fTop->fPoint))
    {
        return;
    }
    TESS_LOG("insert edge (%g -> %g) below vertex %g\n", fTop->fID, fBottom->fID, v->fID);
    Edge* prev = nullptr;
    Edge* next;
    for (next = v->fFirstEdgeBelow; next; next = next->fNextEdgeBelow)
    {
        if (next->isRightOf(*fBottom))
        {
            break;
        }
        prev = next;
    }
    list_insert<Edge, &Edge::fPrevEdgeBelow, &Edge::fNextEdgeBelow>(this,
                                                                    prev,
                                                                    next,
                                                                    &v->fFirstEdgeBelow,
                                                                    &v->fLastEdgeBelow);
}

static void remove_edge_above(Edge* edge)
{
    assert(edge->fTop && edge->fBottom);
    TESS_LOG("removing edge (%g -> %g) above vertex %g\n",
             edge->fTop->fID,
             edge->fBottom->fID,
             edge->fBottom->fID);
    list_remove<Edge, &Edge::fPrevEdgeAbove, &Edge::fNextEdgeAbove>(edge,
                                                                    &edge->fBottom->fFirstEdgeAbove,
                                                                    &edge->fBottom->fLastEdgeAbove);
}

static void remove_edge_below(Edge* edge)
{
    assert(edge->fTop && edge->fBottom);
    TESS_LOG("removing edge (%g -> %g) below vertex %g\n",
             edge->fTop->fID,
             edge->fBottom->fID,
             edge->fTop->fID);
    list_remove<Edge, &Edge::fPrevEdgeBelow, &Edge::fNextEdgeBelow>(edge,
                                                                    &edge->fTop->fFirstEdgeBelow,
                                                                    &edge->fTop->fLastEdgeBelow);
}

void GrTriangulator::Edge::disconnect()
{
    remove_edge_above(this);
    remove_edge_below(this);
}

static bool rewind(EdgeList* activeEdges, Vertex** current, Vertex* dst, const Comparator& c)
{
    if (!current || *current == dst || c.sweep_lt((*current)->fPoint, dst->fPoint))
    {
        return true;
    }
    Vertex* v = *current;
    TESS_LOG("rewinding active edges from vertex %g to vertex %g\n", v->fID, dst->fID);
    while (v != dst)
    {
        v = v->fPrev;
        for (Edge* e = v->fFirstEdgeBelow; e; e = e->fNextEdgeBelow)
        {
            if (!activeEdges->remove(e))
            {
                return false;
            }
        }
        Edge* leftEdge = v->fLeftEnclosingEdge;
        for (Edge* e = v->fFirstEdgeAbove; e; e = e->fNextEdgeAbove)
        {
            if (!activeEdges->insert(e, leftEdge))
            {
                return false;
            }
            leftEdge = e;
            Vertex* top = e->fTop;
            if (c.sweep_lt(top->fPoint, dst->fPoint) &&
                ((top->fLeftEnclosingEdge && !top->fLeftEnclosingEdge->isLeftOf(*e->fTop)) ||
                 (top->fRightEnclosingEdge && !top->fRightEnclosingEdge->isRightOf(*e->fTop))))
            {
                dst = top;
            }
        }
    }
    *current = v;
    return true;
}

static bool rewind_if_necessary(Edge* edge,
                                EdgeList* activeEdges,
                                Vertex** current,
                                const Comparator& c)
{
    if (!activeEdges || !current)
    {
        return true;
    }
    if (!edge)
    {
        return false;
    }
    Vertex* top = edge->fTop;
    Vertex* bottom = edge->fBottom;
    if (edge->fLeft)
    {
        Vertex* leftTop = edge->fLeft->fTop;
        Vertex* leftBottom = edge->fLeft->fBottom;
        if (leftTop && leftBottom)
        {
            if (c.sweep_lt(leftTop->fPoint, top->fPoint) && !edge->fLeft->isLeftOf(*top))
            {
                if (!rewind(activeEdges, current, leftTop, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(top->fPoint, leftTop->fPoint) && !edge->isRightOf(*leftTop))
            {
                if (!rewind(activeEdges, current, top, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(bottom->fPoint, leftBottom->fPoint) &&
                     !edge->fLeft->isLeftOf(*bottom))
            {
                if (!rewind(activeEdges, current, leftTop, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(leftBottom->fPoint, bottom->fPoint) &&
                     !edge->isRightOf(*leftBottom))
            {
                if (!rewind(activeEdges, current, top, c))
                {
                    return false;
                }
            }
        }
    }
    if (edge->fRight)
    {
        Vertex* rightTop = edge->fRight->fTop;
        Vertex* rightBottom = edge->fRight->fBottom;
        if (rightTop && rightBottom)
        {
            if (c.sweep_lt(rightTop->fPoint, top->fPoint) && !edge->fRight->isRightOf(*top))
            {
                if (!rewind(activeEdges, current, rightTop, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(top->fPoint, rightTop->fPoint) && !edge->isLeftOf(*rightTop))
            {
                if (!rewind(activeEdges, current, top, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(bottom->fPoint, rightBottom->fPoint) &&
                     !edge->fRight->isRightOf(*bottom))
            {
                if (!rewind(activeEdges, current, rightTop, c))
                {
                    return false;
                }
            }
            else if (c.sweep_lt(rightBottom->fPoint, bottom->fPoint) &&
                     !edge->isLeftOf(*rightBottom))
            {
                if (!rewind(activeEdges, current, top, c))
                {
                    return false;
                }
            }
        }
    }
    return true;
}

bool GrTriangulator::setTop(Edge* edge,
                            Vertex* v,
                            EdgeList* activeEdges,
                            Vertex** current,
                            const Comparator& c) const
{
    remove_edge_below(edge);
    if (fCollectBreadcrumbTriangles)
    {
        fBreadcrumbList.append(fAlloc,
                               edge->fTop->fPoint,
                               edge->fBottom->fPoint,
                               v->fPoint,
                               edge->fWinding);
    }
    edge->fTop = v;
    edge->recompute();
    edge->insertBelow(v, c);
    if (!rewind_if_necessary(edge, activeEdges, current, c))
    {
        return false;
    }
    return this->mergeCollinearEdges(edge, activeEdges, current, c);
}

bool GrTriangulator::setBottom(Edge* edge,
                               Vertex* v,
                               EdgeList* activeEdges,
                               Vertex** current,
                               const Comparator& c) const
{
    remove_edge_above(edge);
    if (fCollectBreadcrumbTriangles)
    {
        fBreadcrumbList.append(fAlloc,
                               edge->fTop->fPoint,
                               edge->fBottom->fPoint,
                               v->fPoint,
                               edge->fWinding);
    }
    edge->fBottom = v;
    edge->recompute();
    edge->insertAbove(v, c);
    if (!rewind_if_necessary(edge, activeEdges, current, c))
    {
        return false;
    }
    return this->mergeCollinearEdges(edge, activeEdges, current, c);
}

bool GrTriangulator::mergeEdgesAbove(Edge* edge,
                                     Edge* other,
                                     EdgeList* activeEdges,
                                     Vertex** current,
                                     const Comparator& c) const
{
    if (!edge || !other)
    {
        return false;
    }
    if (coincident(edge->fTop->fPoint, other->fTop->fPoint))
    {
        TESS_LOG("merging coincident above edges (%g, %g) -> (%g, %g)\n",
                 edge->fTop->fPoint.x,
                 edge->fTop->fPoint.y,
                 edge->fBottom->fPoint.x,
                 edge->fBottom->fPoint.y);
        if (!rewind(activeEdges, current, edge->fTop, c))
        {
            return false;
        }
        other->fWinding += edge->fWinding;
        edge->disconnect();
        edge->fTop = edge->fBottom = nullptr;
    }
    else if (c.sweep_lt(edge->fTop->fPoint, other->fTop->fPoint))
    {
        if (!rewind(activeEdges, current, edge->fTop, c))
        {
            return false;
        }
        other->fWinding += edge->fWinding;
        if (!this->setBottom(edge, other->fTop, activeEdges, current, c))
        {
            return false;
        }
    }
    else
    {
        if (!rewind(activeEdges, current, other->fTop, c))
        {
            return false;
        }
        edge->fWinding += other->fWinding;
        if (!this->setBottom(other, edge->fTop, activeEdges, current, c))
        {
            return false;
        }
    }
    return true;
}

bool GrTriangulator::mergeEdgesBelow(Edge* edge,
                                     Edge* other,
                                     EdgeList* activeEdges,
                                     Vertex** current,
                                     const Comparator& c) const
{
    if (!edge || !other)
    {
        return false;
    }
    if (coincident(edge->fBottom->fPoint, other->fBottom->fPoint))
    {
        TESS_LOG("merging coincident below edges (%g, %g) -> (%g, %g)\n",
                 edge->fTop->fPoint.x,
                 edge->fTop->fPoint.y,
                 edge->fBottom->fPoint.x,
                 edge->fBottom->fPoint.y);
        if (!rewind(activeEdges, current, edge->fTop, c))
        {
            return false;
        }
        other->fWinding += edge->fWinding;
        edge->disconnect();
        edge->fTop = edge->fBottom = nullptr;
    }
    else if (c.sweep_lt(edge->fBottom->fPoint, other->fBottom->fPoint))
    {
        if (!rewind(activeEdges, current, other->fTop, c))
        {
            return false;
        }
        edge->fWinding += other->fWinding;
        if (!this->setTop(other, edge->fBottom, activeEdges, current, c))
        {
            return false;
        }
    }
    else
    {
        if (!rewind(activeEdges, current, edge->fTop, c))
        {
            return false;
        }
        other->fWinding += edge->fWinding;
        if (!this->setTop(edge, other->fBottom, activeEdges, current, c))
        {
            return false;
        }
    }
    return true;
}

static bool top_collinear(Edge* left, Edge* right)
{
    if (!left || !right)
    {
        return false;
    }
    return left->fTop->fPoint == right->fTop->fPoint || !left->isLeftOf(*right->fTop) ||
           !right->isRightOf(*left->fTop);
}

static bool bottom_collinear(Edge* left, Edge* right)
{
    if (!left || !right)
    {
        return false;
    }
    return left->fBottom->fPoint == right->fBottom->fPoint || !left->isLeftOf(*right->fBottom) ||
           !right->isRightOf(*left->fBottom);
}

bool GrTriangulator::mergeCollinearEdges(Edge* edge,
                                         EdgeList* activeEdges,
                                         Vertex** current,
                                         const Comparator& c) const
{
    for (;;)
    {
        if (top_collinear(edge->fPrevEdgeAbove, edge))
        {
            if (!this->mergeEdgesAbove(edge->fPrevEdgeAbove, edge, activeEdges, current, c))
            {
                return false;
            }
        }
        else if (top_collinear(edge, edge->fNextEdgeAbove))
        {
            if (!this->mergeEdgesAbove(edge->fNextEdgeAbove, edge, activeEdges, current, c))
            {
                return false;
            }
        }
        else if (bottom_collinear(edge->fPrevEdgeBelow, edge))
        {
            if (!this->mergeEdgesBelow(edge->fPrevEdgeBelow, edge, activeEdges, current, c))
            {
                return false;
            }
        }
        else if (bottom_collinear(edge, edge->fNextEdgeBelow))
        {
            if (!this->mergeEdgesBelow(edge->fNextEdgeBelow, edge, activeEdges, current, c))
            {
                return false;
            }
        }
        else
        {
            break;
        }
    }
    assert(!top_collinear(edge->fPrevEdgeAbove, edge));
    assert(!top_collinear(edge, edge->fNextEdgeAbove));
    assert(!bottom_collinear(edge->fPrevEdgeBelow, edge));
    assert(!bottom_collinear(edge, edge->fNextEdgeBelow));
    return true;
}

GrTriangulator::BoolFail GrTriangulator::splitEdge(Edge* edge,
                                                   Vertex* v,
                                                   EdgeList* activeEdges,
                                                   Vertex** current,
                                                   const Comparator& c)
{
    if (!edge->fTop || !edge->fBottom || v == edge->fTop || v == edge->fBottom)
    {
        return BoolFail::kFalse;
    }
    TESS_LOG("splitting edge (%g -> %g) at vertex %g (%g, %g)\n",
             edge->fTop->fID,
             edge->fBottom->fID,
             v->fID,
             v->fPoint.x,
             v->fPoint.y);
    Vertex* top;
    Vertex* bottom;
    int winding = edge->fWinding;
    // Theoretically, and ideally, the edge betwee p0 and p1 is being split by v, and v is "between"
    // the segment end points according to c. This is equivalent to p0 < v < p1.  Unfortunately, if
    // v was clamped/rounded this relation doesn't always hold.
    if (c.sweep_lt(v->fPoint, edge->fTop->fPoint))
    {
        // Actually "v < p0 < p1": update 'edge' to be v->p1 and add v->p0. We flip the winding on
        // the new edge so that it winds as if it were p0->v.
        top = v;
        bottom = edge->fTop;
        winding *= -1;
        if (!this->setTop(edge, v, activeEdges, current, c))
        {
            return BoolFail::kFail;
        }
    }
    else if (c.sweep_lt(edge->fBottom->fPoint, v->fPoint))
    {
        // Actually "p0 < p1 < v": update 'edge' to be p0->v and add p1->v. We flip the winding on
        // the new edge so that it winds as if it were v->p1.
        top = edge->fBottom;
        bottom = v;
        winding *= -1;
        if (!this->setBottom(edge, v, activeEdges, current, c))
        {
            return BoolFail::kFail;
        }
    }
    else
    {
        // The ideal case, "p0 < v < p1": update 'edge' to be p0->v and add v->p1. Original winding
        // is valid for both edges.
        top = v;
        bottom = edge->fBottom;
        if (!this->setBottom(edge, v, activeEdges, current, c))
        {
            return BoolFail::kFail;
        }
    }
    Edge* newEdge = this->allocateEdge(top, bottom, winding, edge->fType);
    newEdge->insertBelow(top, c);
    newEdge->insertAbove(bottom, c);
    if (!this->mergeCollinearEdges(newEdge, activeEdges, current, c))
    {
        return BoolFail::kFail;
    }
    return BoolFail::kTrue;
}

GrTriangulator::BoolFail GrTriangulator::intersectEdgePair(Edge* left,
                                                           Edge* right,
                                                           EdgeList* activeEdges,
                                                           Vertex** current,
                                                           const Comparator& c)
{
    if (!left->fTop || !left->fBottom || !right->fTop || !right->fBottom)
    {
        return BoolFail::kFalse;
    }
    if (left->fTop == right->fTop || left->fBottom == right->fBottom)
    {
        return BoolFail::kFalse;
    }

    // Check if the lines intersect as determined by isLeftOf and isRightOf, since that is the
    // source of ground truth. It may suggest an intersection even if Edge::intersect() did not have
    // the precision to check it. In this case we are explicitly correcting the edge topology to
    // match the sided-ness checks.
    Edge* split = nullptr;
    Vertex* splitAt = nullptr;
    if (c.sweep_lt(left->fTop->fPoint, right->fTop->fPoint))
    {
        if (!left->isLeftOf(*right->fTop))
        {
            split = left;
            splitAt = right->fTop;
        }
    }
    else
    {
        if (!right->isRightOf(*left->fTop))
        {
            split = right;
            splitAt = left->fTop;
        }
    }
    if (c.sweep_lt(right->fBottom->fPoint, left->fBottom->fPoint))
    {
        if (!left->isLeftOf(*right->fBottom))
        {
            split = left;
            splitAt = right->fBottom;
        }
    }
    else
    {
        if (!right->isRightOf(*left->fBottom))
        {
            split = right;
            splitAt = left->fBottom;
        }
    }

    if (!split)
    {
        return BoolFail::kFalse;
    }

    // Rewind to the top of the edge that is "moving" since this topology correction can change the
    // geometry of the split edge.
    if (!rewind(activeEdges, current, split->fTop, c))
    {
        return BoolFail::kFail;
    }
    return this->splitEdge(split, splitAt, activeEdges, current, c);
}

Edge* GrTriangulator::makeConnectingEdge(Vertex* prev,
                                         Vertex* next,
                                         EdgeType type,
                                         const Comparator& c,
                                         int windingScale)
{
    if (!prev || !next || prev->fPoint == next->fPoint)
    {
        return nullptr;
    }
    Edge* edge = this->makeEdge(prev, next, type, c);
    edge->insertBelow(edge->fTop, c);
    edge->insertAbove(edge->fBottom, c);
    edge->fWinding *= windingScale;
    this->mergeCollinearEdges(edge, nullptr, nullptr, c);
    return edge;
}

void GrTriangulator::mergeVertices(Vertex* src,
                                   Vertex* dst,
                                   VertexList* mesh,
                                   const Comparator& c) const
{
    TESS_LOG("found coincident verts at %g, %g; merging %g into %g\n",
             src->fPoint.x,
             src->fPoint.y,
             src->fID,
             dst->fID);
    dst->fAlpha = std::max(src->fAlpha, dst->fAlpha);
    if (src->fPartner)
    {
        src->fPartner->fPartner = dst;
    }
    while (Edge* edge = src->fFirstEdgeAbove)
    {
        std::ignore = this->setBottom(edge, dst, nullptr, nullptr, c);
    }
    while (Edge* edge = src->fFirstEdgeBelow)
    {
        std::ignore = this->setTop(edge, dst, nullptr, nullptr, c);
    }
    mesh->remove(src);
    dst->fSynthetic = true;
}

Vertex* GrTriangulator::makeSortedVertex(const Vec2D& p,
                                         uint8_t alpha,
                                         VertexList* mesh,
                                         Vertex* reference,
                                         const Comparator& c) const
{
    Vertex* prevV = reference;
    while (prevV && c.sweep_lt(p, prevV->fPoint))
    {
        prevV = prevV->fPrev;
    }
    Vertex* nextV = prevV ? prevV->fNext : mesh->fHead;
    while (nextV && c.sweep_lt(nextV->fPoint, p))
    {
        prevV = nextV;
        nextV = nextV->fNext;
    }
    Vertex* v;
    if (prevV && coincident(prevV->fPoint, p))
    {
        v = prevV;
    }
    else if (nextV && coincident(nextV->fPoint, p))
    {
        v = nextV;
    }
    else
    {
        v = fAlloc->make<Vertex>(p, alpha);
#if TRIANGULATOR_LOGGING
        if (!prevV)
        {
            v->fID = mesh->fHead->fID - 1.0f;
        }
        else if (!nextV)
        {
            v->fID = mesh->fTail->fID + 1.0f;
        }
        else
        {
            v->fID = (prevV->fID + nextV->fID) * 0.5f;
        }
#endif
        mesh->insert(v, prevV, nextV);
    }
    return v;
}

// Clamps x and y coordinates independently, so the returned point will lie within the bounding
// box formed by the corners of 'min' and 'max' (although min/max here refer to the ordering
// imposed by 'c').
static Vec2D clamp(Vec2D p, Vec2D min, Vec2D max, const Comparator& c)
{
    if (c.fDirection == Comparator::Direction::kHorizontal)
    {
        // With horizontal sorting, we know min.x <= max.x, but there's no relation between
        // Y components unless min.x == max.x.
        return {std::clamp(p.x, min.x, max.x),
                min.y < max.y ? std::clamp(p.y, min.y, max.y) : std::clamp(p.y, max.y, min.y)};
    }
    else
    {
        // And with vertical sorting, we know Y's relation but not necessarily X's.
        return {min.x < max.x ? std::clamp(p.x, min.x, max.x) : std::clamp(p.x, max.x, min.x),
                std::clamp(p.y, min.y, max.y)};
    }
}

#if 0
void GrTriangulator::computeBisector(Edge* edge1, Edge* edge2, Vertex* v) const
{
    assert(fEmitCoverage); // Edge-AA only!
    Line line1 = edge1->fLine;
    Line line2 = edge2->fLine;
    line1.normalize();
    line2.normalize();
    double cosAngle = line1.fA * line2.fA + line1.fB * line2.fB;
    if (cosAngle > 0.999)
    {
        return;
    }
    line1.fC += edge1->fWinding > 0 ? -1 : 1;
    line2.fC += edge2->fWinding > 0 ? -1 : 1;
    Vec2D p;
    if (line1.intersect(line2, &p))
    {
        uint8_t alpha = edge1->fType == EdgeType::kOuter ? 255 : 0;
        v->fPartner = fAlloc->make<Vertex>(p, alpha);
        TESS_LOG("computed bisector (%g,%g) alpha %d for vertex %g\n", p.x, p.y, alpha, v->fID);
    }
}
#endif

GrTriangulator::BoolFail GrTriangulator::checkForIntersection(Edge* left,
                                                              Edge* right,
                                                              EdgeList* activeEdges,
                                                              Vertex** current,
                                                              VertexList* mesh,
                                                              const Comparator& c)
{
    if (!left || !right)
    {
        return BoolFail::kFalse;
    }
    Vec2D p;
    uint8_t alpha;
    // If we are going to call intersect, then there must be tops and bottoms.
    if (!left->fTop || !left->fBottom || !right->fTop || !right->fBottom)
    {
        return BoolFail::kFail;
    }
    if (left->intersect(*right, &p, &alpha) && is_finite(p))
    {
        Vertex* v;
        TESS_LOG("found intersection, pt is %g, %g\n", p.x, p.y);
        Vertex* top = *current;
        // If the intersection point is above the current vertex, rewind to the vertex above the
        // intersection.
        while (top && c.sweep_lt(p, top->fPoint))
        {
            top = top->fPrev;
        }

        // Always clamp the intersection to lie between the vertices of each segment, since
        // in theory that's where the intersection is, but in reality, floating point error may
        // have computed an intersection beyond a vertex's component(s).
        p = clamp(p, left->fTop->fPoint, left->fBottom->fPoint, c);
        p = clamp(p, right->fTop->fPoint, right->fBottom->fPoint, c);

        if (coincident(p, left->fTop->fPoint))
        {
            v = left->fTop;
        }
        else if (coincident(p, left->fBottom->fPoint))
        {
            v = left->fBottom;
        }
        else if (coincident(p, right->fTop->fPoint))
        {
            v = right->fTop;
        }
        else if (coincident(p, right->fBottom->fPoint))
        {
            v = right->fBottom;
        }
        else
        {
            v = this->makeSortedVertex(p, alpha, mesh, top, c);
#if 0
            if (left->fTop->fPartner)
            {
                assert(fEmitCoverage); // Edge-AA only!
                v->fSynthetic = true;
                this->computeBisector(left, right, v);
            }
#endif
        }
        if (!rewind(activeEdges, current, top ? top : v, c))
        {
            return BoolFail::kFail;
        }
        if (this->splitEdge(left, v, activeEdges, current, c) == BoolFail::kFail)
        {
            return BoolFail::kFail;
        }
        if (this->splitEdge(right, v, activeEdges, current, c) == BoolFail::kFail)
        {
            return BoolFail::kFail;
        }
        v->fAlpha = std::max(v->fAlpha, alpha);
        return BoolFail::kTrue;
    }
    return this->intersectEdgePair(left, right, activeEdges, current, c);
}

void GrTriangulator::sanitizeContours(VertexList* contours, int contourCnt) const
{
    for (VertexList* contour = contours; contourCnt > 0; --contourCnt, ++contour)
    {
        if (contour->fHead == nullptr)
        {
            continue; // empty
        }

        Vertex* prev = contour->fTail;
        prev->fPoint.x = double_to_clamped_scalar((double)prev->fPoint.x);
        prev->fPoint.y = double_to_clamped_scalar((double)prev->fPoint.y);
#if 0
        if (fRoundVerticesToQuarterPixel)
        {
            round(&prev->fPoint);
        }
#endif
        for (Vertex* v = contour->fHead; v;)
        {
            v->fPoint.x = double_to_clamped_scalar((double)v->fPoint.x);
            v->fPoint.y = double_to_clamped_scalar((double)v->fPoint.y);
#if 0
            if (fRoundVerticesToQuarterPixel)
            {
                round(&v->fPoint);
            }
#endif
            Vertex* next = v->fNext;
            Vertex* nextWrap = next ? next : contour->fHead;
            if (coincident(prev->fPoint, v->fPoint))
            {
                TESS_LOG("vertex %g,%g coincident; removing\n", v->fPoint.x, v->fPoint.y);
                contour->remove(v);
            }
            else if (!is_finite(v->fPoint))
            {
                TESS_LOG("vertex %g,%g non-finite; removing\n", v->fPoint.x, v->fPoint.y);
                contour->remove(v);
            }
            else if (!fPreserveCollinearVertices &&
                     Line(prev->fPoint, nextWrap->fPoint).dist(v->fPoint) == 0.0)
            {
                TESS_LOG("vertex %g,%g collinear; removing\n", v->fPoint.x, v->fPoint.y);
                contour->remove(v);
            }
            else
            {
                prev = v;
            }
            v = next;
        }
    }
}

bool GrTriangulator::mergeCoincidentVertices(VertexList* mesh, const Comparator& c) const
{
    if (!mesh->fHead)
    {
        return false;
    }
    bool merged = false;
    for (Vertex* v = mesh->fHead->fNext; v;)
    {
        Vertex* next = v->fNext;
        if (c.sweep_lt(v->fPoint, v->fPrev->fPoint))
        {
            v->fPoint = v->fPrev->fPoint;
        }
        if (coincident(v->fPrev->fPoint, v->fPoint))
        {
            this->mergeVertices(v, v->fPrev, mesh, c);
            merged = true;
        }
        v = next;
    }
    return merged;
}

// Stage 2: convert the contours to a mesh of edges connecting the vertices.

void GrTriangulator::buildEdges(VertexList* contours,
                                int contourCnt,
                                VertexList* mesh,
                                const Comparator& c)
{
    for (VertexList* contour = contours; contourCnt > 0; --contourCnt, ++contour)
    {
        Vertex* prev = contour->fTail;
        for (Vertex* v = contour->fHead; v;)
        {
            Vertex* next = v->fNext;
            this->makeConnectingEdge(prev, v, EdgeType::kInner, c);
            mesh->append(v);
            prev = v;
            v = next;
        }
    }
}

template <CompareFunc sweep_lt>
static void sorted_merge(VertexList* front, VertexList* back, VertexList* result)
{
    Vertex* a = front->fHead;
    Vertex* b = back->fHead;
    while (a && b)
    {
        if (sweep_lt(a->fPoint, b->fPoint))
        {
            front->remove(a);
            result->append(a);
            a = front->fHead;
        }
        else
        {
            back->remove(b);
            result->append(b);
            b = back->fHead;
        }
    }
    result->append(*front);
    result->append(*back);
}

void GrTriangulator::SortedMerge(VertexList* front,
                                 VertexList* back,
                                 VertexList* result,
                                 const Comparator& c)
{
    if (c.fDirection == Comparator::Direction::kHorizontal)
    {
        sorted_merge<sweep_lt_horiz>(front, back, result);
    }
    else
    {
        sorted_merge<sweep_lt_vert>(front, back, result);
    }
#if TRIANGULATOR_LOGGING
    float id = 0.0f;
    for (Vertex* v = result->fHead; v; v = v->fNext)
    {
        v->fID = id++;
    }
#endif
}

// Stage 3: sort the vertices by increasing sweep direction.

template <CompareFunc sweep_lt> static void merge_sort(VertexList* vertices)
{
    Vertex* slow = vertices->fHead;
    if (!slow)
    {
        return;
    }
    Vertex* fast = slow->fNext;
    if (!fast)
    {
        return;
    }
    do
    {
        fast = fast->fNext;
        if (fast)
        {
            fast = fast->fNext;
            slow = slow->fNext;
        }
    } while (fast);
    VertexList front(vertices->fHead, slow);
    VertexList back(slow->fNext, vertices->fTail);
    front.fTail->fNext = back.fHead->fPrev = nullptr;

    merge_sort<sweep_lt>(&front);
    merge_sort<sweep_lt>(&back);

    vertices->fHead = vertices->fTail = nullptr;
    sorted_merge<sweep_lt>(&front, &back, vertices);
}

#if TRIANGULATOR_LOGGING
void VertexList::dump() const
{
    for (Vertex* v = fHead; v; v = v->fNext)
    {
        TESS_LOG("vertex %g (%g, %g) alpha %d", v->fID, v->fPoint.x, v->fPoint.y, v->fAlpha);
        if (Vertex* p = v->fPartner)
        {
            TESS_LOG(", partner %g (%g, %g) alpha %d\n",
                     p->fID,
                     p->fPoint.x,
                     p->fPoint.y,
                     p->fAlpha);
        }
        else
        {
            TESS_LOG(", null partner\n");
        }
        for (Edge* e = v->fFirstEdgeAbove; e; e = e->fNextEdgeAbove)
        {
            TESS_LOG("  edge %g -> %g, winding %d\n", e->fTop->fID, e->fBottom->fID, e->fWinding);
        }
        for (Edge* e = v->fFirstEdgeBelow; e; e = e->fNextEdgeBelow)
        {
            TESS_LOG("  edge %g -> %g, winding %d\n", e->fTop->fID, e->fBottom->fID, e->fWinding);
        }
    }
}
#endif

#ifdef SK_DEBUG
static void validate_edge_pair(Edge* left, Edge* right, const Comparator& c)
{
    if (!left || !right)
    {
        return;
    }
    if (left->fTop == right->fTop)
    {
        assert(left->isLeftOf(*right->fBottom));
        assert(right->isRightOf(*left->fBottom));
    }
    else if (c.sweep_lt(left->fTop->fPoint, right->fTop->fPoint))
    {
        assert(left->isLeftOf(*right->fTop));
    }
    else
    {
        assert(right->isRightOf(*left->fTop));
    }
    if (left->fBottom == right->fBottom)
    {
        assert(left->isLeftOf(*right->fTop));
        assert(right->isRightOf(*left->fTop));
    }
    else if (c.sweep_lt(right->fBottom->fPoint, left->fBottom->fPoint))
    {
        assert(left->isLeftOf(*right->fBottom));
    }
    else
    {
        assert(right->isRightOf(*left->fBottom));
    }
}

static void validate_edge_list(EdgeList* edges, const Comparator& c)
{
    Edge* left = edges->fHead;
    if (!left)
    {
        return;
    }
    for (Edge* right = left->fRight; right; right = right->fRight)
    {
        validate_edge_pair(left, right, c);
        left = right;
    }
}
#endif

// Stage 4: Simplify the mesh by inserting new vertices at intersecting edges.

GrTriangulator::SimplifyResult GrTriangulator::simplify(VertexList* mesh, const Comparator& c)
{
    TESS_LOG("simplifying complex polygons\n");

    int initialNumEdges = fNumEdges;
    int numSelfIntersections = 0;

    EdgeList activeEdges;
    auto result = SimplifyResult::kAlreadySimple;
    for (Vertex* v = mesh->fHead; v != nullptr; v = v->fNext)
    {
        if (!v->isConnected())
        {
            continue;
        }

        // The max increase across all skps, svgs and gms with only the triangulating and SW path
        // renderers enabled and with the triangulator's maxVerbCount set to the Chrome value is
        // 17x.
        if (fNumEdges > 170 * initialNumEdges)
        {
            return SimplifyResult::kFailed;
        }

        // In pathological cases, a path can intersect itself millions of times. After 500,000
        // self-intersections are found, reject the path.
        if (numSelfIntersections > 500000)
        {
            return SimplifyResult::kFailed;
        }

        Edge* leftEnclosingEdge;
        Edge* rightEnclosingEdge;
        bool restartChecks;
        do
        {
            TESS_LOG("\nvertex %g: (%g,%g), alpha %d\n",
                     v->fID,
                     v->fPoint.x,
                     v->fPoint.y,
                     v->fAlpha);
            restartChecks = false;
            FindEnclosingEdges(*v, activeEdges, &leftEnclosingEdge, &rightEnclosingEdge);
            v->fLeftEnclosingEdge = leftEnclosingEdge;
            v->fRightEnclosingEdge = rightEnclosingEdge;
            if (v->fFirstEdgeBelow)
            {
                for (Edge* edge = v->fFirstEdgeBelow; edge; edge = edge->fNextEdgeBelow)
                {
                    BoolFail l = this->checkForIntersection(leftEnclosingEdge,
                                                            edge,
                                                            &activeEdges,
                                                            &v,
                                                            mesh,
                                                            c);
                    if (l == BoolFail::kFail)
                    {
                        return SimplifyResult::kFailed;
                    }
                    if (l == BoolFail::kFalse)
                    {
                        BoolFail r = this->checkForIntersection(edge,
                                                                rightEnclosingEdge,
                                                                &activeEdges,
                                                                &v,
                                                                mesh,
                                                                c);
                        if (r == BoolFail::kFail)
                        {
                            return SimplifyResult::kFailed;
                        }
                        if (r == BoolFail::kFalse)
                        {
                            // Neither l and r are both false.
                            continue;
                        }
                    }

                    // Either l or r are true.
                    result = SimplifyResult::kFoundSelfIntersection;
                    restartChecks = true;
                    ++numSelfIntersections;
                    break;
                } // for
            }
            else
            {
                BoolFail bf = this->checkForIntersection(leftEnclosingEdge,
                                                         rightEnclosingEdge,
                                                         &activeEdges,
                                                         &v,
                                                         mesh,
                                                         c);
                if (bf == BoolFail::kFail)
                {
                    return SimplifyResult::kFailed;
                }
                if (bf == BoolFail::kTrue)
                {
                    result = SimplifyResult::kFoundSelfIntersection;
                    restartChecks = true;
                    ++numSelfIntersections;
                }
            }
        } while (restartChecks);
#ifdef SK_DEBUG
        validate_edge_list(&activeEdges, c);
#endif
        for (Edge* e = v->fFirstEdgeAbove; e; e = e->fNextEdgeAbove)
        {
            if (!activeEdges.remove(e))
            {
                return SimplifyResult::kFailed;
            }
        }
        Edge* leftEdge = leftEnclosingEdge;
        for (Edge* e = v->fFirstEdgeBelow; e; e = e->fNextEdgeBelow)
        {
            activeEdges.insert(e, leftEdge);
            leftEdge = e;
        }
    }
    assert(!activeEdges.fHead && !activeEdges.fTail);
    return result;
}

// Stage 5: Tessellate the simplified mesh into monotone polygons.

std::tuple<Poly*, bool> GrTriangulator::tessellate(const VertexList& vertices, const Comparator&)
{
    TESS_LOG("\ntessellating simple polygons\n");
    EdgeList activeEdges;
    Poly* polys = nullptr;
    for (Vertex* v = vertices.fHead; v != nullptr; v = v->fNext)
    {
        if (!v->isConnected())
        {
            continue;
        }
#if TRIANGULATOR_LOGGING
        TESS_LOG("\nvertex %g: (%g,%g), alpha %d\n", v->fID, v->fPoint.x, v->fPoint.y, v->fAlpha);
#endif
        Edge* leftEnclosingEdge;
        Edge* rightEnclosingEdge;
        FindEnclosingEdges(*v, activeEdges, &leftEnclosingEdge, &rightEnclosingEdge);
        Poly* leftPoly;
        Poly* rightPoly;
        if (v->fFirstEdgeAbove)
        {
            leftPoly = v->fFirstEdgeAbove->fLeftPoly;
            rightPoly = v->fLastEdgeAbove->fRightPoly;
        }
        else
        {
            leftPoly = leftEnclosingEdge ? leftEnclosingEdge->fRightPoly : nullptr;
            rightPoly = rightEnclosingEdge ? rightEnclosingEdge->fLeftPoly : nullptr;
        }
#if TRIANGULATOR_LOGGING
        TESS_LOG("edges above:\n");
        for (Edge* e = v->fFirstEdgeAbove; e; e = e->fNextEdgeAbove)
        {
            TESS_LOG("%g -> %g, lpoly %d, rpoly %d\n",
                     e->fTop->fID,
                     e->fBottom->fID,
                     e->fLeftPoly ? e->fLeftPoly->fID : -1,
                     e->fRightPoly ? e->fRightPoly->fID : -1);
        }
        TESS_LOG("edges below:\n");
        for (Edge* e = v->fFirstEdgeBelow; e; e = e->fNextEdgeBelow)
        {
            TESS_LOG("%g -> %g, lpoly %d, rpoly %d\n",
                     e->fTop->fID,
                     e->fBottom->fID,
                     e->fLeftPoly ? e->fLeftPoly->fID : -1,
                     e->fRightPoly ? e->fRightPoly->fID : -1);
        }
#endif
        if (v->fFirstEdgeAbove)
        {
            if (leftPoly)
            {
                leftPoly = leftPoly->addEdge(v->fFirstEdgeAbove, kRight_Side, this);
            }
            if (rightPoly)
            {
                rightPoly = rightPoly->addEdge(v->fLastEdgeAbove, kLeft_Side, this);
            }
            for (Edge* e = v->fFirstEdgeAbove; e != v->fLastEdgeAbove; e = e->fNextEdgeAbove)
            {
                Edge* rightEdge = e->fNextEdgeAbove;
                activeEdges.remove(e);
                if (e->fRightPoly)
                {
                    e->fRightPoly->addEdge(e, kLeft_Side, this);
                }
                if (rightEdge->fLeftPoly && rightEdge->fLeftPoly != e->fRightPoly)
                {
                    rightEdge->fLeftPoly->addEdge(e, kRight_Side, this);
                }
            }
            activeEdges.remove(v->fLastEdgeAbove);
            if (!v->fFirstEdgeBelow)
            {
                if (leftPoly && rightPoly && leftPoly != rightPoly)
                {
                    assert(leftPoly->fPartner == nullptr && rightPoly->fPartner == nullptr);
                    rightPoly->fPartner = leftPoly;
                    leftPoly->fPartner = rightPoly;
                }
            }
        }
        if (v->fFirstEdgeBelow)
        {
            if (!v->fFirstEdgeAbove)
            {
                if (leftPoly && rightPoly)
                {
                    if (leftPoly == rightPoly)
                    {
                        if (leftPoly->fTail && leftPoly->fTail->fSide == kLeft_Side)
                        {
                            leftPoly =
                                this->makePoly(&polys, leftPoly->lastVertex(), leftPoly->fWinding);
                            leftEnclosingEdge->fRightPoly = leftPoly;
                        }
                        else
                        {
                            rightPoly = this->makePoly(&polys,
                                                       rightPoly->lastVertex(),
                                                       rightPoly->fWinding);
                            rightEnclosingEdge->fLeftPoly = rightPoly;
                        }
                    }
                    Edge* join = this->allocateEdge(leftPoly->lastVertex(), v, 1, EdgeType::kInner);
                    leftPoly = leftPoly->addEdge(join, kRight_Side, this);
                    rightPoly = rightPoly->addEdge(join, kLeft_Side, this);
                }
            }
            Edge* leftEdge = v->fFirstEdgeBelow;
            leftEdge->fLeftPoly = leftPoly;
            activeEdges.insert(leftEdge, leftEnclosingEdge);
            for (Edge* rightEdge = leftEdge->fNextEdgeBelow; rightEdge;
                 rightEdge = rightEdge->fNextEdgeBelow)
            {
                activeEdges.insert(rightEdge, leftEdge);
                int winding = leftEdge->fLeftPoly ? leftEdge->fLeftPoly->fWinding : 0;
                winding += leftEdge->fWinding;
                if (winding != 0)
                {
                    Poly* poly = this->makePoly(&polys, v, winding);
                    leftEdge->fRightPoly = rightEdge->fLeftPoly = poly;
                }
                leftEdge = rightEdge;
            }
            v->fLastEdgeBelow->fRightPoly = rightPoly;
        }
#if TRIANGULATOR_LOGGING
        TESS_LOG("\nactive edges:\n");
        for (Edge* e = activeEdges.fHead; e != nullptr; e = e->fRight)
        {
            TESS_LOG("%g -> %g, lpoly %d, rpoly %d\n",
                     e->fTop->fID,
                     e->fBottom->fID,
                     e->fLeftPoly ? e->fLeftPoly->fID : -1,
                     e->fRightPoly ? e->fRightPoly->fID : -1);
        }
#endif
    }
    return {polys, true};
}

// This is a driver function that calls stages 2-5 in turn.

void GrTriangulator::contoursToMesh(VertexList* contours,
                                    int contourCnt,
                                    VertexList* mesh,
                                    const Comparator& c)
{
#if TRIANGULATOR_LOGGING
    for (int i = 0; i < contourCnt; ++i)
    {
        Vertex* v = contours[i].fHead;
        assert(v);
        TESS_LOG("path.moveTo(%20.20g, %20.20g);\n", v->fPoint.x, v->fPoint.y);
        for (v = v->fNext; v; v = v->fNext)
        {
            TESS_LOG("path.lineTo(%20.20g, %20.20g);\n", v->fPoint.x, v->fPoint.y);
        }
    }
#endif
    this->sanitizeContours(contours, contourCnt);
    this->buildEdges(contours, contourCnt, mesh, c);
}

void GrTriangulator::SortMesh(VertexList* vertices, const Comparator& c)
{
    if (!vertices || !vertices->fHead)
    {
        return;
    }

    // Sort vertices in Y (secondarily in X).
    if (c.fDirection == Comparator::Direction::kHorizontal)
    {
        merge_sort<sweep_lt_horiz>(vertices);
    }
    else
    {
        merge_sort<sweep_lt_vert>(vertices);
    }
#if TRIANGULATOR_LOGGING
    for (Vertex* v = vertices->fHead; v != nullptr; v = v->fNext)
    {
        static float gID = 0.0f;
        v->fID = gID++;
    }
#endif
}

std::tuple<Poly*, bool> GrTriangulator::contoursToPolys(VertexList* contours, int contourCnt)
{
    Comparator c(fDirection);
    VertexList mesh;
    this->contoursToMesh(contours, contourCnt, &mesh, c);
    TESS_LOG("\ninitial mesh:\n");
    DUMP_MESH(mesh);
    SortMesh(&mesh, c);
    TESS_LOG("\nsorted mesh:\n");
    DUMP_MESH(mesh);
    this->mergeCoincidentVertices(&mesh, c);
    TESS_LOG("\nsorted+merged mesh:\n");
    DUMP_MESH(mesh);
    auto result = this->simplify(&mesh, c);
    if (result == SimplifyResult::kFailed)
    {
        return {nullptr, false};
    }
    TESS_LOG("\nsimplified mesh:\n");
    DUMP_MESH(mesh);
    return this->tessellate(mesh, c);
}

// Stage 6: Triangulate the monotone polygons into a vertex buffer.
void GrTriangulator::polysToTriangles(
    Poly* polys,
    FillRule overrideFillType,
    uint16_t pathID,
    bool reverseTriangles,
    gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
{
    for (Poly* poly = polys; poly; poly = poly->fNext)
    {
        if (apply_fill_type(overrideFillType, poly))
        {
            emitPoly(poly, pathID, reverseTriangles, mappedMemory);
        }
    }
}

static int get_contour_count(const RawPath& path, float tolerance)
{
    // We could theoretically be more aggressive about not counting empty contours, but we need to
    // actually match the exact number of contour linked lists the tessellator will create later on.
    int contourCnt = 1;
    bool hasPoints = false;

    bool first = true;
    for (auto [verb, pts] : path)
    {
        switch (verb)
        {
            case PathVerb::move:
                if (!first)
                {
                    ++contourCnt;
                }
                [[fallthrough]];
            case PathVerb::line:
            case PathVerb::quad:
            case PathVerb::cubic:
                hasPoints = true;
                break;
            default:
                break;
        }
        first = false;
    }
    if (!hasPoints)
    {
        return 0;
    }
    return contourCnt;
}

std::tuple<Poly*, bool> GrTriangulator::pathToPolys(const RawPath& path,
                                                    float tolerance,
                                                    const AABB& clipBounds,
                                                    bool* isLinear)
{
    int contourCnt = get_contour_count(path, tolerance);
    if (contourCnt <= 0)
    {
        *isLinear = true;
        return {nullptr, true};
    }

#if 0
    if (SkPathFillType_IsInverse(fPath.getFillType()))
    {
        contourCnt++;
    }
#endif
    std::unique_ptr<VertexList[]> contours(new VertexList[contourCnt]);

    this->pathToContours(path, tolerance, clipBounds, contours.get(), isLinear);
    return this->contoursToPolys(contours.get(), contourCnt);
}

int64_t GrTriangulator::CountPoints(Poly* polys, FillRule overrideFillType)
{
    int64_t count = 0;
    for (Poly* poly = polys; poly; poly = poly->fNext)
    {
        if (apply_fill_type(overrideFillType, poly) && poly->fCount >= 3)
        {
            count += (poly->fCount - 2) * (TRIANGULATOR_WIREFRAME ? 6 : 3);
        }
    }
    return count;
}

// Stage 6: Triangulate the monotone polygons into a vertex buffer.

size_t GrTriangulator::countMaxTriangleVertices(Poly* polys) const
{
    return math::lossless_numeric_cast<size_t>(CountPoints(polys, fFillRule));
}

size_t GrTriangulator::polysToTriangles(
    Poly* polys,
    uint64_t maxVertexCount,
    uint16_t pathID,
    bool reverseTriangles,
    gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
{
    if (0 == maxVertexCount || maxVertexCount > std::numeric_limits<int32_t>::max())
    {
        return 0;
    }

    size_t vertexStride = sizeof(gpu::TriangleVertex);
#if 0
    if (fEmitCoverage)
    {
        vertexStride += sizeof(float);
    }
#endif

    size_t start = mappedMemory->bytesWritten();
    polysToTriangles(polys, fFillRule, pathID, reverseTriangles, mappedMemory);
    size_t actualCount = (mappedMemory->bytesWritten() - start) / vertexStride;
    assert(actualCount <= maxVertexCount * vertexStride);
    return actualCount;
}
} // namespace rive

#endif // SK_ENABLE_OPTIMIZE_SIZE
