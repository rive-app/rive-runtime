/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/trivial_block_allocator.hpp"

#include <memory>

namespace rive
{
class GrInnerFanTriangulator;

// RenderPath implementation for Rive's pixel local storage renderer.
class RiveRenderPath : public LITE_RTTI_OVERRIDE(RenderPath, RiveRenderPath)
{
public:
    RiveRenderPath() = default;
    RiveRenderPath(FillRule fillRule, RawPath& rawPath);

    void rewind() override;
    void fillRule(FillRule rule) override { m_fillRule = rule; }

    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override;
    void close() override;

    void addPath(CommandPath* p, const Mat2D& m) override
    {
        addRenderPath(p->renderPath(), m);
    }
    void addRenderPath(const RenderPath* path, const Mat2D& matrix) override;
    void addRenderPathBackwards(const RenderPath* path,
                                const Mat2D& transform) override;
    void addRawPath(const RawPath& path) override;
    const RawPath& getRawPath() const { return m_rawPath; }
    FillRule getFillRule() const { return m_fillRule; }

    const AABB& getBounds() const;
    // Approximates the area of the path by linearizing it with a coarse
    // tolerance of 8px in artboard space.
    constexpr static float kCoarseAreaTolerance = 8;
    float getCoarseArea() const;
    // Determine if the path's signed, post-transform area is positive.
    bool isClockwiseDominant(const Mat2D& viewMatrix) const;
    uint64_t getRawPathMutationID() const;

    // Returns this path's cached triangulation, or null if it doesn't have one
    // for its current geometry.
    // NOTE: An inner-fan triangulation of this path is transform- and
    // fill-rule-independent (both are applied when its output is emitted), so
    // one instance serves every draw and transform.
    GrInnerFanTriangulator* cachedTriangulator() const;

    // Builds a triangulation.
    //
    // Generational: the first sighting for a given geometry builds it in
    // 'perFrameAllocator', where it dies with the frame, so a path that mutates
    // every frame is never charged for retaining a triangulation it can't
    // reuse. Only on the second sighting does the path allocate persistent
    // storage of its own and cache the triangulator.
    //
    // Costs real CPU time, so callers gate this; see cachedTriangulator().
    GrInnerFanTriangulator* createTriangulator(
        TrivialBlockAllocator& perFrameAllocator) const;

    // 1-dimensional feathering along the normal vector quits looking like a
    // blur when there is strong curvature. This method returns a copy of the
    // path with shorter, flatter curves that will more accurately depict a
    // gaussian blur when drawn with the given feather.
    //
    // TODO: Move this work to the GPU.
    rcp<RiveRenderPath> makeSoftenedCopyForFeathering(float feather,
                                                      float matrixMaxScale);

#ifdef DEBUG
    // Allows ref holders to guarantee the rawPath doesn't mutate during a
    // specific time.
    void lockRawPathMutations() const { ++m_rawPathMutationLockCount; }
    void unlockRawPathMutations() const
    {
        assert(m_rawPathMutationLockCount > 0);
        --m_rawPathMutationLockCount;
    }
#endif

private:
    FillRule m_fillRule = FillRule::nonZero;
    RawPath m_rawPath;
    mutable AABB m_bounds;
    mutable float m_coarseArea;
    mutable uint64_t m_rawPathMutationID;

    // Cached inner-fan triangulation, in a dedicated allocator so it outlives
    // any single frame. Allocated only once a path has been asked to
    // triangulate the same geometry twice; invalidated when
    // m_rawPathMutationID changes. See createTriangulator().
    mutable std::unique_ptr<TrivialBlockAllocator> m_triangulatorAllocator;
    mutable GrInnerFanTriangulator* m_cachedTriangulator = nullptr;
    mutable uint64_t m_cachedTriangulatorMutationID = 0;
    // Geometry seen once, but not yet a second time, so not cached yet.
    // Mutation IDs start at 1, so 0 means "nothing seen".
    mutable uint64_t m_triangulatorFirstSightingMutationID = 0;

    enum Dirt
    {
        kPathBoundsDirt = 1 << 0,
        kRawPathMutationIDDirt = 1 << 1,
        kPathCoarseAreaDirt = 1 << 2,
        kAllDirt = ~0,
    };

    mutable uint32_t m_dirt = kAllDirt;
    RIVE_DEBUG_CODE(mutable int m_rawPathMutationLockCount = 0;)
};
} // namespace rive
