/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrInnerFanTriangulator_DEFINED
#define GrInnerFanTriangulator_DEFINED

#if !defined(SK_ENABLE_OPTIMIZE_SIZE)

#include "gr_triangulator.hpp"

namespace rive
{
// Triangulates the inner polygon(s) of a path (i.e., the triangle fan for a
// Redbook rendering method). When combined with the outer curves and grout
// triangles, these produce a complete path. If a groutCollector is not
// provided, pathToPolys fails upon self intersection.
class GrInnerFanTriangulator : private GrTriangulator
{
public:
    using GroutTriangleList = GrTriangulator::BreadcrumbTriangleList;

    GrInnerFanTriangulator(const RawPath& path,
                           Comparator::Direction direction,
                           TrivialBlockAllocator* alloc) :
        GrTriangulator(direction, alloc)
    {
        fPreserveCollinearVertices = true;
        fCollectBreadcrumbTriangles = true;
        bool isLinear;
        auto [polys, success] =
            GrTriangulator::pathToPolys(path, 0, AABB{}, &isLinear);
        if (success)
        {
            m_polys = polys;
        }
    }

    // The mesh is fill-rule-independent; the fill rule only filters it at
    // output, so it's supplied per call rather than baked in. This lets one
    // triangulator be cached and reused regardless of fill rule.
    size_t maxVertexCount(FillRule fillRule) const
    {
        return m_polys != nullptr ? countMaxTriangleVertices(m_polys, fillRule)
                                  : 0;
    }

    size_t polysToTriangles(
        uint16_t pathID,
        FillRule fillRule,
        bool reverseTriangles,
        bool negateWinding,
        gpu::WindingFaces windingFaces,
        gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* mappedMemory) const
    {
        if (m_polys == nullptr)
        {
            return 0;
        }
        return GrTriangulator::polysToTriangles(m_polys,
                                                maxVertexCount(fillRule),
                                                fillRule,
                                                pathID,
                                                reverseTriangles,
                                                negateWinding,
                                                windingFaces,
                                                mappedMemory);
    }

    const GroutTriangleList& groutList() const { return fBreadcrumbList; }

private:
    Poly* m_polys = nullptr;
};
} // namespace rive

#else

// Stub out GrInnerFanTriangulator::GroutTriangleList for function declarations.
namespace GrInnerFanTriangulator
{
struct GroutTriangleList
{
    GroutTriangleList() = delete;
};
}; // namespace GrInnerFanTriangulator

#endif // SK_ENABLE_OPTIMIZE_SIZE

#endif // GrInnerFanTriangulator_DEFINED
