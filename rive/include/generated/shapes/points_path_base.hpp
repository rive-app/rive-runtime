#ifndef _RIVE_POINTS_PATH_BASE_HPP_
#define _RIVE_POINTS_PATH_BASE_HPP_
#include "shapes/path.hpp"
namespace rive
{
	class PointsPathBase : public Path
	{
	private:
		bool m_IsClosed = false;
	public:
		bool isClosed() const { return m_IsClosed; }
		void isClosed(bool value) { m_IsClosed = value; }
	};
} // namespace rive

#endif