#ifndef _RIVE_DASH_PATH_HPP_
#define _RIVE_DASH_PATH_HPP_

#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/renderer.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"

namespace rive
{
class Dash
{
public:
    Dash();
    Dash(float value, bool percentage);

    float value() const;
    float normalizedValue(float length) const;
    bool percentage() const;

private:
    float m_value;
    bool m_percentage;
};

class PathDasher
{
protected:
    void invalidateSourcePath();
    void invalidateDash();
    RenderPath* dash(const RawPath& source, Factory* factory, Dash offset, Span<Dash> dashes);

private:
    RawPath m_rawPath;
    RenderPath* m_renderPath = nullptr;
    rcp<RenderPath> m_dashedPath;
    std::vector<rcp<ContourMeasure>> m_contours;

public:
    float pathLength() const;
};
} // namespace rive
#endif