#include "rive/generated/node_base.hpp"
#include "rive/node.hpp"

using namespace rive;

Core* NodeBase::clone() const
{
    auto cloned = new Node();
    cloned->copy(*this);
    return cloned;
}
