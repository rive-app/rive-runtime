#ifndef _RIVE_SHAPE_HPP_
#define _RIVE_SHAPE_HPP_
#include "generated/shapes/shape_base.hpp"
#include <vector>

namespace rive
{
	class Path;
	class Shape : public ShapeBase
	{
	private:
		std::vector<Path*> m_Paths;

	public:
		void addPath(Path* path);
		std::vector<Path*>& paths() { return m_Paths; }
	};
} // namespace rive

#endif