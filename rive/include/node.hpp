#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "generated/node_base.hpp"
#include <stdio.h>
namespace rive
{
	class Node : public NodeBase
	{
		float m_X;

	public:
		Node() : m_X(1.0f) { printf("Constructing Node\n"); }
		float x() const { return m_X; }
		void set_x(float val) { m_X = val; }
	};

	float nodeX(rive::Node* node);
} // namespace rive

#endif