#include "rive/generated/nested_artboard_leaf_base.hpp"
#include "rive/nested_artboard_leaf.hpp"

using namespace rive;

Core* NestedArtboardLeafBase::clone() const
{
    auto cloned = new NestedArtboardLeaf();
    cloned->copy(*this);
    return cloned;
}
