#ifndef _RIVE_NODE_HPP_
#define _RIVE_NODE_HPP_
#include "generated/node_base.hpp"
namespace rive
{
	/// A Rive Node
	class Node : public NodeBase
	{
	public:
		void onAddedClean(CoreContext* context) {}
	};
} // namespace rive

#endif