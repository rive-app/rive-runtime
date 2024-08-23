#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "rive/generated/node_base.hpp"

namespace rive
{
/// A Rive Node
class Node : public NodeBase
{
protected:
    void xChanged() override;
    void yChanged() override;
};
} // namespace rive

#endif