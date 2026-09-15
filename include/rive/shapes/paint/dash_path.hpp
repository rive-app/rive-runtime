#ifndef _RIVE_DASH_PATH_HPP_
#define _RIVE_DASH_PATH_HPP_
#include "rive/generated/shapes/paint/dash_path_base.hpp"

#include "rive/shapes/paint/shape_paint.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/path_measure.hpp"
#include <vector>

namespace rive
{
class Dash;

class DashEffectPath : public EffectPath
{
public:
    void invalidateEffect() override;
    PathMeasure& pathMeasure() { return m_pathMeasure; }
    void createPathMeasure(const RawPath*);
    ShapePaintPath* path() override { return &m_path; }

private:
    ShapePaintPath m_path;
    PathMeasure m_pathMeasure;
};
class PathDasher
{
    friend class Dash;

protected:
    virtual void invalidateDash();
    ShapePaintPath* dash(ShapePaintPath* destination,
                         const RawPath* source,
                         PathMeasure* pathMeasure,
                         Dash* offset,
                         Span<Dash*> dashes);
    ShapePaintPath* applyDash(ShapePaintPath* destination,
                              const RawPath* source,
                              PathMeasure* pathMeasure,
                              Dash* offset,
                              Span<Dash*> dashes);

public:
    virtual ~PathDasher() {}
};

class DashPath : public DashPathBase, public PathDasher, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    void offsetChanged() override;
    void offsetIsPercentageChanged() override;
    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      const ShapePaint* shapePaint) override;
    void invalidateDash() override;
    EffectsContainer* parentPaint() override;
    virtual EffectPath* createEffectPath() override;

#ifdef WITH_RIVE_EDITOR
    // `m_dashes` is populated by `onAddedClean` in coop-arrival
    // order. `applyDash` walks dashes via `dashes[index++ %
    // size]` — a sequential modulo loop — so out-of-author-order
    // dashes corrupt the rendered pattern. Mirrors the
    // `Path::sortVerticesForEditor` fix for the same arrival-
    // order bug class.
    void sortDashesForEditor();
#endif

private:
    std::vector<Dash*> m_dashes;
};
} // namespace rive
#endif