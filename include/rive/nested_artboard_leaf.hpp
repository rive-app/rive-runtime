#ifndef _RIVE_NESTED_ARTBOARD_LEAF_HPP_
#define _RIVE_NESTED_ARTBOARD_LEAF_HPP_
#include "rive/generated/nested_artboard_leaf_base.hpp"
#include <stdio.h>
namespace rive
{
class NestedArtboardLeaf : public NestedArtboardLeafBase
{
public:
    Core* clone() const override;
    void update(ComponentDirt value) override;
};
} // namespace rive

#endif