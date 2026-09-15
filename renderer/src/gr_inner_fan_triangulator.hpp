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

#include <optional>

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
                           const AABB& pathBounds,
                           TrivialBlockAllocator* alloc) :
        GrTriangulator(
            // Sweep along the longer dimension of pathBounds so the sweep line
            // crosses fewer edges.
            pathBounds.width() > pathBounds.height()
                ? Comparator::Direction::kHorizontal
                : Comparator::Direction::kVertical,
            alloc)
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

    // Emits the interior triangulation as weight-expanded retrofitted cubic
    // patches, merging into patches of up to 3 edge-adjacent triangles.
    // Returns the number of patches emitted.
    size_t polysToRetrofitCubicPatches(
        FillRule fillRule,
        gpu::WindingFaces windingFaces,
        const RetrofitCubicPatchEmitter& emitPatch) const
    {
        if (m_polys == nullptr)
        {
            return 0;
        }
        return GrTriangulator::polysToRetrofitCubicPatches(m_polys,
                                                           fillRule,
                                                           windingFaces,
                                                           emitPatch);
    }

    // Number of patches polysToRetrofitCubicPatches() emits for this
    // triangulation. Lazily computed and cached because it requires the full
    // (nontrivial) triangle traversal and shared-edge detection.
    size_t retrofitCubicPatchCount(FillRule fillRule) const
    {
        std::optional<size_t>& cached = fillRule == FillRule::evenOdd
                                            ? m_evenOddPatchCount
                                            : m_nonZeroPatchCount;
        if (!cached.has_value())
        {
            cached = polysToRetrofitCubicPatches(fillRule,
                                                 gpu::WindingFaces::all,
                                                 [](const Vec2D*, size_t) {});
        }
        return *cached;
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

    // Lazily-computed retrofitCubicPatchCount() cache: one slot per fill rule
    // the triangulator sees (nonZero -- which clockwise maps to -- and
    // evenOdd).
    mutable std::optional<size_t> m_nonZeroPatchCount;
    mutable std::optional<size_t> m_evenOddPatchCount;
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
