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
};
} // namespace rive

#endif