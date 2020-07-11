#ifndef _RIVE_PATH_BASE_HPP_
#define _RIVE_PATH_BASE_HPP_
#include "node.hpp"
namespace rive
{
	class PathBase : public Node
	{
	public:
		static const int typeKey = 12;
		int coreType() const override { return typeKey; }
	};
} // namespace rive

#endif