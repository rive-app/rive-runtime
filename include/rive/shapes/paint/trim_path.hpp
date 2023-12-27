#ifndef _RIVE_TRIM_PATH_HPP_
#define _RIVE_TRIM_PATH_HPP_
#include "rive/generated/shapes/paint/trim_path_base.hpp"
#include "rive/shapes/paint/stroke_effect.hpp"
#include "rive/renderer.hpp"

namespace rive
{
class TrimPath : public TrimPathBase, public StrokeEffect
{
private:
    rcp<RenderPath> m_TrimmedPath;
    RenderPath* m_RenderPath = nullptr;

public:
    StatusCode onAddedClean(CoreContext* context) override;
    RenderPath* effectPath(MetricsPath* source, Factory*) override;
    void invalidateEffect() override;

    void startChanged() override;
    void endChanged() override;
    void offsetChanged() override;
    void modeValueChanged() override;
};
} // namespace rive

#endif
