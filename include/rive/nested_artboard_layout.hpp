#ifndef _RIVE_NESTED_ARTBOARD_LAYOUT_HPP_
#define _RIVE_NESTED_ARTBOARD_LAYOUT_HPP_
#include "rive/generated/nested_artboard_layout_base.hpp"

namespace rive
{
class NestedArtboardLayout : public NestedArtboardLayoutBase
{
public:
#ifdef WITH_RIVE_LAYOUT
    void* layoutNode();
#endif
    Core* clone() const override;
    void markNestedLayoutDirty();
    void update(ComponentDirt value) override;
    StatusCode onAddedClean(CoreContext* context) override;

    float actualInstanceWidth();
    float actualInstanceHeight();

protected:
    void instanceWidthChanged() override;
    void instanceHeightChanged() override;
    void instanceWidthUnitsValueChanged() override;
    void instanceHeightUnitsValueChanged() override;
    void instanceWidthScaleTypeChanged() override;
    void instanceHeightScaleTypeChanged() override;

private:
    void updateWidthOverride();
    void updateHeightOverride();
};
} // namespace rive

#endif