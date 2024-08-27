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
// Triangulates the inner polygon(s) of a path (i.e., the triangle fan for a Redbook rendering
// method). When combined with the outer curves and grout triangles, these produce a complete path.
// If a groutCollector is not provided, pathToPolys fails upon self intersection.
class GrInnerFanTriangulator : private GrTriangulator
{
public:
    using GroutTriangleList = GrTriangulator::BreadcrumbTriangleList;

    GrInnerFanTriangulator(const RawPath& path,
                           const Mat2D& viewMatrix,
                           Comparator::Direction direction,
                           FillRule fillRule,
                           TrivialBlockAllocator* alloc) :
        GrTriangulator(direction, fillRule, alloc),
        m_shouldReverseTriangles(viewMatrix[0] * viewMatrix[3] - viewMatrix[2] * viewMatrix[1] < 0)
    {
        fPreserveCollinearVertices = true;
        fCollectBreadcrumbTriangles = true;
        bool isLinear;
        auto [polys, success] = GrTriangulator::pathToPolys(path, 0, AABB{}, &isLinear);
        if (success)
        {
            m_polys = polys;
            m_maxVertexCount = countMaxTriangleVertices(m_polys);
        }
    }

    FillRule fillRule() const { return fFillRule; }

    size_t maxVertexCount() const { return m_maxVertexCount; }

    size_t polysToTriangles(gpu::WriteOnlyMappedMemory<gpu::TriangleVertex>* bufferRing,
                            uint16_t pathID) const

    {
        if (m_polys == nullptr || m_maxVertexCount == 0)
        {
            return 0;
        }
        return GrTriangulator::polysToTriangles(m_polys,
                                                m_maxVertexCount,
                                                pathID,
                                                m_shouldReverseTriangles,
                                                bufferRing);
    }

    const GroutTriangleList& groutList() const { return fBreadcrumbList; }

private:
    // We reverse triangles whe using a left-handed view matrix, in order to ensure we always emit
    // clockwise triangles.
    bool m_shouldReverseTriangles;
    Poly* m_polys = nullptr;
    size_t m_maxVertexCount = 0;
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
