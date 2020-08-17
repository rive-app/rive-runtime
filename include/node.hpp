#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "generated/node_base.hpp"

namespace rive
{
	/// A Rive Node
	class Node : public NodeBase
	{
	protected:
		void xChanged() override;
		void yChanged() override;
		void rotationChanged() override;
		void scaleXChanged() override;
		void scaleYChanged() override;
		void opacityChanged() override;
	};
} // namespace rive

#endif