#ifndef _RIVE_TRIM_PATH_HPP_
#define _RIVE_TRIM_PATH_HPP_
#include "rive/generated/shapes/paint/trim_path_base.hpp"
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
    RenderPath* effectPath(const RawPath& source, Factory*) override;
    void invalidateEffect() override;

    void startChanged() override;
    void endChanged() override;
    void offsetChanged() override;
    void modeValueChanged() override;

    TrimPathMode mode() const { return (TrimPathMode)modeValue(); }

    StatusCode onAddedDirty(CoreContext* context) override;

    const RawPath& rawPath() const { return m_rawPath; }

private:
    void invalidateTrim();
    void trimRawPath(const RawPath& source);
    RawPath m_rawPath;
    std::vector<rcp<ContourMeasure>> m_contours;
    rcp<RenderPath> m_trimmedPath;
    RenderPath* m_renderPath = nullptr;
};
} // namespace rive

#endif
