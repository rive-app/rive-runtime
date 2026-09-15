#ifndef _RIVE_ARTBOARD_LIST_MAP_RULE_HPP_
#define _RIVE_ARTBOARD_LIST_MAP_RULE_HPP_
#include "rive/generated/artboard_list_map_rule_base.hpp"
#include <stdio.h>
namespace rive
{
class ArtboardListMapRule : public ArtboardListMapRuleBase
{
public:
    StatusCode onAddedDirty(CoreContext* context) override;
#ifdef WITH_RIVE_EDITOR
    void editorParentChanged(ContainerComponent* from,
                             ContainerComponent* to) override;
#endif
};
} // namespace rive

#endif