#ifndef _RIVE_DASH_PATH_HPP_
#define _RIVE_DASH_PATH_HPP_
#include "rive/generated/shapes/paint/dash_path_base.hpp"

#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/renderer.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"
#include <vector>

namespace rive
{
class Dash;
class PathDasher
{
    friend class Dash;

protected:
    void invalidateSourcePath();
    virtual void invalidateDash();
    ShapePaintPath* dash(const RawPath* source,
                         Dash* offset,
                         Span<Dash*> dashes);
    ShapePaintPath* applyDash(const RawPath* source,
                              Dash* offset,
                              Span<Dash*> dashes);

protected:
    ShapePaintPath m_path;
    std::vector<rcp<ContourMeasure>> m_contours;

public:
    float pathLength() const;
};

class DashPath : public DashPathBase, public PathDasher, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    void invalidateEffect() override;
    void offsetChanged() override;
    void offsetIsPercentageChanged() override;
    void updateEffect(const ShapePaintPath* source) override;
    ShapePaintPath* effectPath() override;
    void invalidateDash() override;

private:
    std::vector<Dash*> m_dashes;
};
} // namespace rive
#endif