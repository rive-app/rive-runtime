#ifndef _RIVE_DASH_PATH_HPP_
#define _RIVE_DASH_PATH_HPP_
#include "rive/generated/shapes/paint/dash_path_base.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
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
    void invalidateDash();
    RenderPath* dash(const RawPath& source, Factory* factory, Dash* offset, Span<Dash*> dashes);

private:
    RawPath m_rawPath;
    RenderPath* m_renderPath = nullptr;
    rcp<RenderPath> m_dashedPath;
    std::vector<rcp<ContourMeasure>> m_contours;

public:
    float pathLength() const;
};

class DashPath : public DashPathBase, public PathDasher, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    RenderPath* effectPath(const RawPath& source, Factory*) override;
    void invalidateEffect() override;
    void offsetChanged() override;
    void offsetIsPercentageChanged() override;

private:
    std::vector<Dash*> m_dashes;
};
} // namespace rive
#endif