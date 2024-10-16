/*
 * Copyright 2022 Rive
 */

#pragma once

#include "rive/math/raw_path.hpp"
#include "rive/renderer.hpp"
#include "rive/renderer/draw.hpp"
#include "rive_render_paint.hpp"

namespace rive
{
// RenderPath implementation for Rive's pixel local storage renderer.
class RiveRenderPath : public lite_rtti_override<RenderPath, RiveRenderPath>
{
public:
    RiveRenderPath() = default;
    RiveRenderPath(FillRule fillRule, RawPath& rawPath);

    void rewind() override;
    void fillRule(FillRule rule) override
    {
        if (m_fillRule == rule)
        {
            return;
        }
        m_fillRule = rule;
        // Most cached draws can be used interchangeably with any fill rule, but
        // if there is a triangulator, it needs to be invalidated when the fill
        // rule changes.
        if (m_cachedElements[CACHE_FILLED].draw != nullptr &&
            m_cachedElements[CACHE_FILLED].draw->triangulator() != nullptr)
        {
            invalidateDrawCache(CACHE_FILLED);
        }
    }

    void moveTo(float x, float y) override;
    void lineTo(float x, float y) override;
    void cubicTo(float ox, float oy, float ix, float iy, float x, float y)
        override;
    void close() override;

    void addPath(CommandPath* p, const Mat2D& m) override
    {
        addRenderPath(p->renderPath(), m);
    }
    void addRenderPath(RenderPath* path, const Mat2D& matrix) override;

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

    enum Dirt
    {
        kPathBoundsDirt = 1 << 0,
        kRawPathMutationIDDirt = 1 << 1,
        kPathCoarseAreaDirt = 1 << 2,
        kAllDirt = ~0,
    };

    mutable uint32_t m_dirt = kAllDirt;
    RIVE_DEBUG_CODE(mutable int m_rawPathMutationLockCount = 0;)

public:
    void invalidateDrawCache() const
    {
        invalidateDrawCache(CACHE_STROKED);
        invalidateDrawCache(CACHE_FILLED);
    }

    void invalidateDrawCache(int index) const
    {
        m_cachedElements[index].draw = nullptr;
    }

    void setDrawCache(gpu::RiveRenderPathDraw* drawCache,
                      const Mat2D& mat,
                      rive::RiveRenderPaint* riveRenderPaint) const;

    gpu::DrawUniquePtr getDrawCache(const Mat2D& matrix,
                                    const RiveRenderPaint* paint,
                                    FillRule fillRule,
                                    TrivialBlockAllocator* allocator,
                                    const gpu::RenderContext::FrameDescriptor&,
                                    gpu::InterlockMode interlockMode) const;

private:
    enum
    {
        CACHE_STROKED,
        CACHE_FILLED,
        NUM_CACHES,
    };
    struct CacheElements
    {
        gpu::RiveRenderPathDraw* draw = nullptr;
        float xx;
        float xy;
        float yx;
        float yy;
    };
    mutable CacheElements m_cachedElements[NUM_CACHES];
    mutable float m_cachedThickness;
    mutable StrokeJoin m_cachedJoin;
    mutable StrokeCap m_cachedCap;
};
} // namespace rive
