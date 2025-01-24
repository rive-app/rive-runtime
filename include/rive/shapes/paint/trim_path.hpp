#ifndef _RIVE_TRIM_PATH_HPP_
#define _RIVE_TRIM_PATH_HPP_
#include "rive/generated/shapes/paint/trim_path_base.hpp"
#include "rive/shapes/shape_paint_path.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/renderer.hpp"
#include "rive/math/raw_path.hpp"
#include "rive/math/contour_measure.hpp"

namespace rive
{
enum class TrimPathMode : uint8_t
{
    sequential = 1,
    synchronized = 2

};

class TrimPath : public TrimPathBase, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;
    void invalidateEffect() override;

    void updateEffect(const ShapePaintPath* source) override;
    ShapePaintPath* effectPath() override;

    void startChanged() override;
    void endChanged() override;
    void offsetChanged() override;
    void modeValueChanged() override;

    TrimPathMode mode() const { return (TrimPathMode)modeValue(); }

    StatusCode onAddedDirty(CoreContext* context) override;

    const ShapePaintPath& path() const { return m_path; }

protected:
    void invalidateTrim();
    void trimPath(const RawPath* source);
    ShapePaintPath m_path;
    std::vector<rcp<ContourMeasure>> m_contours;
};
} // namespace rive

#endif
