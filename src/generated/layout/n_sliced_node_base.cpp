#include "rive/generated/layout/n_sliced_node_base.hpp"
#include "rive/layout/n_sliced_node.hpp"

using namespace rive;

Core* NSlicedNodeBase::clone() const
{
    auto cloned = new NSlicedNode();
    cloned->copy(*this);
    return cloned;
}
