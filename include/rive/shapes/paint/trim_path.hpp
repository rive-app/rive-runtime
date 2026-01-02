#ifndef _RIVE_TRIM_PATH_HPP_
#define _RIVE_TRIM_PATH_HPP_
#include "rive/generated/shapes/paint/trim_path_base.hpp"
#include "rive/shapes/paint/shape_paint.hpp"
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

class TrimEffectPath : public EffectPath
{
public:
    void invalidateEffect() override;
    std::vector<rcp<ContourMeasure>>& contours() { return m_contours; }
    ShapePaintPath* path() override { return &m_path; }

private:
    ShapePaintPath m_path;
    std::vector<rcp<ContourMeasure>> m_contours;
};

class TrimPath : public TrimPathBase, public StrokeEffect
{
public:
    StatusCode onAddedClean(CoreContext* context) override;

    void updateEffect(PathProvider* pathProvider,
                      const ShapePaintPath* source,
                      ShapePaintType shapePaintType) override;
    EffectsContainer* parentPaint() override;

    void startChanged() override;
    void endChanged() override;
    void offsetChanged() override;
    void modeValueChanged() override;

    TrimPathMode mode() const { return (TrimPathMode)modeValue(); }

    StatusCode onAddedDirty(CoreContext* context) override;

protected:
    void trimPath(ShapePaintPath* destination,
                  std::vector<rcp<ContourMeasure>>& contours,
                  const RawPath* source,
                  ShapePaintType shapePaintType);
    virtual EffectPath* createEffectPath() override;
};
} // namespace rive

#endif
