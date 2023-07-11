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
    using GrTriangulator::GroutTriangleList;

    GrInnerFanTriangulator(const RawPath& path,
                           const AABB& pathBounds,
                           FillRule fillRule,
                           TrivialBlockAllocator* alloc) :
        GrTriangulator(pathBounds, fillRule, alloc)
    {
        fPreserveCollinearVertices = true;
        fCollectGroutTriangles = true;
        bool isLinear;
        auto [polys, success] = GrTriangulator::pathToPolys(path, 0, AABB{}, &isLinear);
        if (success)
        {
            m_polys = polys;
            m_maxVertexCount = countMaxTriangleVertices(m_polys);
        }
    }

    FillRule fillRule() const { return fFillRule; }

    uint64_t maxVertexCount() const { return m_maxVertexCount; }

    void setPathID(uint16_t pathID) { m_pathID = pathID; }
    uint16_t pathID() const { return m_pathID; }

    size_t polysToTriangles(pls::BufferRing<pls::TriangleVertex>* bufferRing) const

    {
        if (m_polys == nullptr)
        {
            return 0;
        }
        return GrTriangulator::polysToTriangles(m_polys, m_maxVertexCount, m_pathID, bufferRing);
    }

    const GroutTriangleList& groutList() const { return fGroutList; }

private:
    uint16_t m_pathID = 0;
    Poly* m_polys = nullptr;
    uint64_t m_maxVertexCount = 0;
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
