#ifndef _RIVE_ARTBOARD_COMPONENT_LIST_OVERRIDE_HPP_
#define _RIVE_ARTBOARD_COMPONENT_LIST_OVERRIDE_HPP_
#include "rive/generated/layout/artboard_component_list_override_base.hpp"
#include "rive/layout/style_overrider.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardInstance;
class ArtboardComponentListOverride : public ArtboardComponentListOverrideBase
{
public:
    void addArtboard(ArtboardInstance* artboard);
    void removeArtboard(ArtboardInstance* artboard);
    float actualInstanceWidth(ArtboardInstance* artboardInstance);
    float actualInstanceHeight(ArtboardInstance* artboardInstance);
    void markHostingLayoutDirty(ArtboardInstance* artboardInstance);
    bool isRow();

protected:
    void instanceWidthChanged() override;
    void instanceHeightChanged() override;
    void instanceWidthUnitsValueChanged() override;
    void instanceHeightUnitsValueChanged() override;
    void instanceWidthScaleTypeChanged() override;
    void instanceHeightScaleTypeChanged() override;

private:
    std::vector<ArtboardInstance*> m_artboards;
    void updateWidthOverride();
    void updateHeightOverride();
    StyleOverrider<ArtboardComponentListOverride> m_styleOverrider =
        StyleOverrider<ArtboardComponentListOverride>(this);
};

} // namespace rive

#endif