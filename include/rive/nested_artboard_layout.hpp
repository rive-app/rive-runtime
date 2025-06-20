#ifndef _RIVE_NESTED_ARTBOARD_LAYOUT_HPP_
#define _RIVE_NESTED_ARTBOARD_LAYOUT_HPP_
#include "rive/generated/nested_artboard_layout_base.hpp"
#include "rive/constraints/layout_constraint.hpp"
#include "rive/layout/layout_node_provider.hpp"
#include "rive/viewmodel/viewmodel_instance_artboard.hpp"

namespace rive
{
class ArtboardInstance;
class NestedArtboardLayout : public NestedArtboardLayoutBase,
                             public LayoutNodeProvider
{
public:
#ifdef WITH_RIVE_LAYOUT
    void* layoutNode(int index) override;
#endif
    Core* clone() const override;
    void markHostingLayoutDirty(ArtboardInstance* artboardInstance) override;
    void markLayoutNodeDirty(
        bool shouldForceUpdateLayoutBounds = false) override;
    void update(ComponentDirt value) override;
    void updateConstraints() override;
    StatusCode onAddedClean(CoreContext* context) override;

    float actualInstanceWidth();
    float actualInstanceHeight();
    bool syncStyleChanges() override;
    void updateLayoutBounds(bool animate = true) override;
    AABB layoutBounds() override;
    size_t numLayoutNodes() override { return 1; }
    bool isLayoutProvider() override { return true; }
    void updateArtboard(
        ViewModelInstanceArtboard* viewModelInstanceArtboard) override;

    TransformComponent* transformComponent() override
    {
        return this->as<TransformComponent>();
    }

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