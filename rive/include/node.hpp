#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "generated/node_base.hpp"
#include <stdio.h>
namespace rive
{
	class Node : public NodeBase
	{
	public:
		Node() { printf("Constructing Node\n"); }
	};

	float nodeX(rive::Node* node);
} // namespace rive

#endif