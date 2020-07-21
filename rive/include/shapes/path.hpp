#ifndef _RIVE_PATH_HPP_
#define _RIVE_PATH_HPP_
#include "generated/shapes/path_base.hpp"
namespace rive
{
	class Shape;
	class Path : public PathBase
	{
	private:
		Shape* m_Shape;

	public:
		Shape* shape() const { return m_Shape; }
		void onAddedClean(CoreContext* context);
	};
} // namespace rive

#endif