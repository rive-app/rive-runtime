#ifndef _RIVE_LAYOUT_NODE_STYLE_HPP_
#define _RIVE_LAYOUT_NODE_STYLE_HPP_
#include "rive/generated/layout/layout_node_style_base.hpp"
#include "rive/component.hpp"
#include "rive/container_component.hpp"
#include "rive/layout/layout_node_provider.hpp"
namespace rive
{
// Sizing style for a layout participant. Its properties change at runtime (data
// binding/animation), so route every change to the owning participant to re-run
// syncStyleChanges — otherwise the C++ runtime never re-syncs layout/display
// (mirrors how LayoutComponentStyle notifies its LayoutComponent).
class LayoutNodeStyle : public LayoutNodeStyleBase
{
public:
    void markLayoutNodeDirty()
    {
        if (auto* provider = LayoutNodeProvider::from(parent()))
        {
            provider->markLayoutNodeDirty();
        }
    }

protected:
    void widthChanged() override { markLayoutNodeDirty(); }
    void heightChanged() override { markLayoutNodeDirty(); }
    void fractionalWidthChanged() override { markLayoutNodeDirty(); }
    void fractionalHeightChanged() override { markLayoutNodeDirty(); }
    void layoutWidthScaleTypeChanged() override { markLayoutNodeDirty(); }
    void layoutHeightScaleTypeChanged() override { markLayoutNodeDirty(); }
    void minWidthChanged() override { markLayoutNodeDirty(); }
    void maxWidthChanged() override { markLayoutNodeDirty(); }
    void minHeightChanged() override { markLayoutNodeDirty(); }
    void maxHeightChanged() override { markLayoutNodeDirty(); }
    void minWidthUnitsValueChanged() override { markLayoutNodeDirty(); }
    void maxWidthUnitsValueChanged() override { markLayoutNodeDirty(); }
    void minHeightUnitsValueChanged() override { markLayoutNodeDirty(); }
    void maxHeightUnitsValueChanged() override { markLayoutNodeDirty(); }
    void widthUnitsValueChanged() override { markLayoutNodeDirty(); }
    void heightUnitsValueChanged() override { markLayoutNodeDirty(); }
    void justifySelfValueChanged() override { markLayoutNodeDirty(); }
    void displayValueChanged() override { markLayoutNodeDirty(); }
};
} // namespace rive

#endif
